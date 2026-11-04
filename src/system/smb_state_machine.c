/**
 * @file smb_state_machine.c
 * @brief Machine d'états principale (voir smb_state_machine.h).
 *
 * Chaque état est une fonction. Le chronomètre d'état est timebase.ticks_100ms (pas de 100 ms),
 * remis à zéro à chaque transition par go_to().
 *
 * Organisation de chaque fonction d'état : actions sur les relais propres à l'état, puis
 * transitions évaluées par ordre de priorité (l'erreur critique vient toujours en premier).
 * Les commentaires « relais » de chaque état donnent l'état attendu des quatre contacteurs :
 *  - SUPPLY : précharge de l'alimentation 24 V (fermé en permanence, sauf maintenance) ;
 *  - DCAC_PRE : précharge de l'onduleur ; DCAC_IN : contacteur direct de l'onduleur ;
 *  - FCS_OUT : sortie de la pile à combustible vers le bus.
 *
 * Les erreurs sont agrégées par update_error_flags() à chaque tour, avant l'état courant.
 * La logique reproduit le firmware v1.3.0 d'origine, y compris ses anomalies connues
 * (repérées par TODO(bug n) et décrites dans docs/bugs.md).
 */
#include "src/system/smb_state_machine.h"
#include "src/smb_config.h"
#include "src/system/timebase.h"
#include "src/devices/battery.h"
#include "src/devices/fcs.h"
#include "src/devices/inverter.h"
#include "src/measures/measures.h"
#include "src/hmi/hmi.h"
#include "src/hmi/maintenance.h"
#include "hal/board_io.h"

/* Temporisations des états (ticks de 100 ms) */
#define INIT_DURATION            20   /* 2 s */
#define AUTOCHECK_DURATION       30   /* 3 s */
#define DCAC_PRECHARGE_DURATION  30   /* 3 s */
#define DCAC_SWITCH_DURATION     5    /* 0.5 s */
#define DCAC_MODE_TIMEOUT        150  /* 15 s : attente du mode onduleur */
#define FCS_START_1_TIMEOUT      30   /* 3 s : attente FCCB en standby */
#define FC_STOP_1_DURATION       2    /* 200 ms */
#define DCAC_STOP_DURATION       15   /* 1.5 s */
#define STOPPING_DURATION        50   /* 5 s */

sSmb smb;

/* Sous-compteur de l'état STOPPING : nombre de pas de 100 ms écoulés depuis l'entrée dans l'état
 * (timebase.ticks_100ms y est remis à zéro à chaque pas, il ne peut donc pas servir de chronomètre). */
static uint16_t step_100ms_cnt;

/* Démarrage en SMB_INIT, erreurs effacées. Appelée par main() avant le démarrage de TMR1. */
void Smb_Init(void)
{
    smb.step = SMB_INIT;
    smb.critical_error_flag = false;
    smb.software_error_flag = false;
    smb.reset_order_flag = false;
    step_100ms_cnt = 0;
    timebase.ticks_100ms = 0;
}

/* Appelée par can_rx.c à la réception de SMB_RX_RESET_ORDER (vraie seulement en mode défaut avec la
 * bonne clé). L'ordre est consommé par state_emergency(). */
void Smb_SetResetOrder(bool order)
{
    smb.reset_order_flag = order;
}

/* ---- Utilitaires ------------------------------------------------------------------- */

/* Changement d'état avec remise à zéro du chronomètre d'état. */
static void go_to(uint8_t step)
{
    smb.step = step;
    timebase.ticks_100ms = 0;
}

/* Ouvre l'onduleur si la protection SOC bas est active (ne le referme pas ici). */
static void apply_soc_disconnect(void)
{
    if (inverter.soc_disconnect_error_flag)
        Board_SetDcacInputRelay(false);
}

/* Ouvre / ferme l'onduleur selon la protection SOC bas. */
static void apply_soc_disconnect_both_ways(void)
{
    Board_SetDcacInputRelay(!inverter.soc_disconnect_error_flag);
}

/*
 * Agrégation des erreurs, à chaque tour de boucle :
 *  - erreur critique SMB (verrouillée) : erreur logicielle, erreur batterie bloquante, fuite H2 ;
 *  - erreur générale pile (recalculée) et erreur générale onduleur (verrouillée), dans leur module.
 */
static void update_error_flags(void)
{
    if (smb.software_error_flag || Battery_HasCriticalError() || fcs.ppmh2_error_flag)
        smb.critical_error_flag = 1;

    Fcs_UpdateGeneralError();
    Inverter_UpdateGeneralError(measures.ac_ambiant_temp.error_flag);
}

/* Acquittement de toutes les erreurs (sortie du mode défaut). */
static void reset_all_errors(void)
{
    Fcs_ResetAllErrors();          /* envoie aussi la commande de RAZ des erreurs à la FCCB */
    hmi.buzzer_5_minutes.error = 0;
    smb.software_error_flag = 0;
    Battery_ResetErrors();
    smb.critical_error_flag = 0;
    Inverter_ResetErrors();
    Measures_ResetSensorErrors();
    hmi.raz_erreur_ihm_flag = 1;   /* signale la RAZ à l'IHM (trame 0x2C00004), acquitté par l'IHM */
}

/* ---- États ------------------------------------------------------------------------- */

/* INIT (10) : relais en sécurité pendant 2 s, le temps que les mesures et les communications démarrent.
 * Relais : SUPPLY fermé, DCAC_PRE / DCAC_IN / FCS_OUT ouverts. Sortie : AUTOCHECK après 2 s. */
static void state_init(void)
{
    Board_RelaysSafeState();
    if (timebase.ticks_100ms >= INIT_DURATION)
        go_to(SMB_AUTOCHECK);
}

/* AUTOCHECK (20) : 3 s sans erreur critique ni erreur onduleur avant d'autoriser le fonctionnement.
 * Relais inchangés (sécurité). Sorties : EMERGENCY sur erreur, STANDBY après 3 s. */
static void state_autocheck(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= AUTOCHECK_DURATION)
        go_to(SMB_STANDBY);
}

/* STANDBY (30) : repos, onduleur et pile hors tension.
 * Relais : SUPPLY fermé, les autres ouverts.
 * Sorties : EMERGENCY sur erreur ; DCAC_START_1 sur bouton mode batterie ; MAINTENANCE sur demande CAN
 * (la FCCB est alors aussi passée en maintenance). Le chronomètre est maintenu à zéro dans cet état. */
static void state_standby(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        smb.step = SMB_EMERGENCY;
    else if (hmi.battery_mode_button.state && !inverter.general_error_flag)
        smb.step = SMB_DCAC_START_1;
    else if (maintenance.mode_request_flag)
    {
        smb.step = SMB_MAINTENANCE;
        Fcs_SendMaintenanceMode();
    }
    timebase.ticks_100ms = 0;
}

/* MAINTENANCE (40) : les relais, LED, buzzer et ventilateurs sont commandés à la main par CAN
 * (voir src/hmi/maintenance.c) ; cet état ne touche à aucune sortie.
 * Sorties : EMERGENCY sur erreur ; STOPPING (mise en sécurité séquencée) à la fin de la demande. */
static void state_maintenance(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        smb.step = SMB_EMERGENCY;
    else if (!maintenance.mode_request_flag)
        smb.step = SMB_STOPPING;
    timebase.ticks_100ms = 0;
}

/* DCAC_START_1 (50) : précharge des condensateurs d'entrée de l'onduleur pendant 3 s.
 * Relais : DCAC_PRE fermé (commandé à chaque tour), DCAC_IN ouvert.
 * Sorties : EMERGENCY sur erreur, DCAC_START_2 après 3 s. */
static void state_dcac_start_1(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= DCAC_PRECHARGE_DURATION)
        go_to(SMB_DCAC_START_2);
    Board_SetDcacPrechargeRelay(true);
}

/* DCAC_START_2 (52) : après 0,5 s supplémentaire, bascule de la précharge vers le contacteur direct.
 * Relais en sortie : DCAC_PRE ouvert, DCAC_IN fermé.
 * Sorties : EMERGENCY sur erreur, DCAC_START_3 après la bascule. */
static void state_dcac_start_2(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= DCAC_SWITCH_DURATION)
    {
        Board_SetDcacPrechargeRelay(false);
        Board_SetDcacInputRelay(true);
        go_to(SMB_DCAC_START_3);
    }
}

/* DCAC_START_3 (54) : attente du démarrage de l'onduleur (mode lu par QMOD toutes les ~7,5 s).
 * Relais : DCAC_IN fermé.
 * Sorties : EMERGENCY sur erreur critique ou onduleur en défaut ; BATTERY_MODE dès que l'onduleur
 * est en mode AC_SOURCE, BATTERY ou POWER_SAVING (valeur > STANDBY), ou après 15 s dans tous les cas. */
static void state_dcac_start_3(void)
{
    if (smb.critical_error_flag || (inverter.mode == INVERTER_FAULT_MODE))
        go_to(SMB_EMERGENCY);
    else if ((inverter.mode > INVERTER_STANDBY_MODE) || (timebase.ticks_100ms >= DCAC_MODE_TIMEOUT))
        go_to(SMB_BATTERY_MODE);
}

/* BATTERY_MODE (100) : l'onduleur fonctionne sur les batteries seules, la pile est isolée.
 * Relais : DCAC_IN fermé sauf protection SOC bas (ouvert / refermé automatiquement), FCS_OUT ouvert.
 * Sorties : EMERGENCY sur erreur ou surcharge de sortie ; STOPPING si les deux boutons de mode sont
 * relâchés ; FCS_START_1 sur bouton hybride si la pile n'est pas en erreur et SOC <= 70 % ;
 * EMERGENCY sur bouton hybride avec la pile en erreur. */
static void state_battery_mode(void)
{
    apply_soc_disconnect_both_ways();

    if (smb.critical_error_flag || inverter.general_error_flag || inverter.apparent_power_very_high_error_flag)
        go_to(SMB_EMERGENCY);
    else if (!hmi.battery_mode_button.state && !hmi.hybrid_mode_button.state)
        go_to(SMB_STOPPING);
    else if (hmi.hybrid_mode_button.state && !fcs.general_error_flag && (battery.soc <= BATTERY_SOC_HIGH_LEVEL))
        go_to(SMB_FCS_START_1);
    else if (hmi.hybrid_mode_button.state && fcs.general_error_flag)
        go_to(SMB_EMERGENCY);
    /* (origine) une dernière branche vers SMB_DCAC_STOP reprenait la condition de SMB_STOPPING : inatteignable */

    Board_SetFcsOutputRelay(false);
    timebase.ticks_100ms = 0;
}

/* FCS_START_1 (120) : attente que la FCCB soit en standby (3 s max) avant de démarrer la pile.
 * Relais : DCAC_IN ouvert si protection SOC bas ; FCS_OUT fermé au moment de l'ordre de démarrage.
 * Sorties : EMERGENCY sur erreur critique ; FCS_START_2 quand la FCCB est en standby (ordre de démarrage
 * envoyé) ; FC_STOP_1 avec erreur de démarrage si la FCCB n'est pas prête en 3 s, est en défaut, ou si
 * la pile ou l'onduleur sont en erreur. */
static void state_fcs_start_1(void)
{
    apply_soc_disconnect();
    if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if (fcs.step == FCCB_STANDBY)
    {
        Fcs_SendStart();
        Board_SetFcsOutputRelay(true);
        go_to(SMB_FCS_START_2);
    }
    else if ((timebase.ticks_100ms >= FCS_START_1_TIMEOUT) || (fcs.step == FCCB_EMERGENCY) ||
             fcs.general_error_flag || inverter.general_error_flag)
    {
        Board_SetFcsOutputRelay(false);
        fcs.startup_error_flag = 1;
        Fcs_SendStop();
        go_to(SMB_FC_STOP_1);
    }
}

/* FCS_START_2 (122) : démarrage de la pile (purge, contrôle de tension, montée en courant...).
 * Relais : FCS_OUT fermé ; DCAC_IN ouvert si protection SOC bas.
 * Sorties : EMERGENCY sur erreur critique ; FC_STOP_1 avec erreur de démarrage si la FCCB passe en défaut
 * ou si la pile / l'onduleur sont en erreur ; HYBRID_MODE quand la FCCB est OPERATING. Si la FCCB n'y est
 * pas parvenue en ~60 s (temporisation comptée par Fcs_Update100ms), une seconde tentative repart de
 * FCS_START_1 ; après cette seconde tentative, la sortie se fait vers EMERGENCY. */
static void state_fcs_start_2(void)
{
    /* TODO(bug 7) : tant que la protection SOC est active, les autres transitions ne sont pas évaluées */
    if (inverter.soc_disconnect_error_flag)
        Board_SetDcacInputRelay(false);
    else if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if ((fcs.step == FCCB_EMERGENCY) || fcs.general_error_flag || inverter.general_error_flag)
    {
        Board_SetFcsOutputRelay(false);
        fcs.startup_error_flag = 1;
        go_to(SMB_FC_STOP_1);
    }
    else if (fcs.step == FCCB_OPERATING)
        go_to(SMB_HYBRID_MODE);
    else if ((fcs.start_2_timeout.error == 1) && (fcs.startup_retry_flag == 0))
    {
        /* première expiration : nouvelle tentative de démarrage */
        fcs.startup_retry_flag = 1;
        fcs.start_2_timeout.error = 0;
        go_to(SMB_FCS_START_1);
    }
    else if (fcs.startup_retry_flag == 1)
        /* TODO(bug 13) : déjà une tentative : tout état non OPERATING au retour dans FCS_START_2
         * mène au défaut immédiatement, sans attendre une seconde expiration des 60 s (origine) */
        go_to(SMB_EMERGENCY);
}

/* HYBRID_MODE (150) : la pile alimente le bus avec les batteries ; consigne de courant envoyée chaque
 * seconde par ems.c.
 * Relais : DCAC_IN fermé sauf protection SOC bas, FCS_OUT fermé.
 * Sorties : EMERGENCY sur erreur critique ; FC_STOP_1 (arrêt de la pile, retour en mode batterie) si le
 * bouton hybride est relâché, si SOC >= 85 %, si la FCCB quitte OPERATING, sur erreur pile ou onduleur,
 * sur courant pile trop faible trop longtemps, ou sur surcharge de sortie. */
static void state_hybrid_mode(void)
{
    apply_soc_disconnect_both_ways();
    if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if (!hmi.hybrid_mode_button.state ||
             battery.soc >= BATTERY_SOC_VERY_HIGH_LEVEL ||
             (fcs.step != FCCB_OPERATING) ||
             fcs.general_error_flag ||
             inverter.general_error_flag ||
             fcs.low_current_timeout.error ||
             inverter.apparent_power_very_high_error_flag)
    {
        fcs.low_current_timeout.error = 0; /* consommé ici, pour ne pas bloquer un prochain démarrage */
        go_to(SMB_FC_STOP_1);
    }
}

/* FC_STOP_1 (200) : début de l'arrêt de la pile ; l'ordre d'arrêt est envoyé après 200 ms.
 * Relais : inchangés (FCS_OUT encore fermé) ; DCAC_IN ouvert si protection SOC bas.
 * Sorties : EMERGENCY sur erreur critique, pile ou onduleur ; FC_STOP_2 après l'ordre d'arrêt. */
static void state_fc_stop_1(void)
{
    /* TODO(bug 7) : idem SMB_FCS_START_2 */
    if (inverter.soc_disconnect_error_flag)
        Board_SetDcacInputRelay(false);
    else if (smb.critical_error_flag || fcs.general_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= FC_STOP_1_DURATION)
    {
        Fcs_SendStop();
        go_to(SMB_FC_STOP_2);
    }
}

/* FC_STOP_2 (201) : attente du retour de la FCCB en standby, pile isolée.
 * Relais : FCS_OUT ouvert ; DCAC_IN ouvert si protection SOC bas.
 * Pendant l'arrêt de la FCCB (SHUTDOWN), double bip à 2,5 s et 3,5 s.
 * Sorties : EMERGENCY sur erreur critique ou FCCB en défaut ; BATTERY_MODE quand la FCCB est en standby. */
static void state_fc_stop_2(void)
{
    apply_soc_disconnect();
    Board_SetFcsOutputRelay(false);

    /* Double bip signalant l'arrêt de la pile */
    if ((timebase.ticks_100ms == 25) && (fcs.step == FCCB_SHUTDOWN))
        Board_SetBuzzer(true);
    else if (timebase.ticks_100ms == 30)
        Board_SetBuzzer(false);
    else if ((timebase.ticks_100ms == 35) && (fcs.step == FCCB_SHUTDOWN))
        Board_SetBuzzer(true);
    else if (timebase.ticks_100ms == 40)
        Board_SetBuzzer(false);
    else
        Board_SetBuzzer(false); /* le buzzer n'est actif qu'aux ticks 25 et 35 exactement */

    if (smb.critical_error_flag || fcs.step == FCCB_EMERGENCY)
        go_to(SMB_EMERGENCY);
    else if (fcs.step == FCCB_STANDBY)
        go_to(SMB_BATTERY_MODE);
}

/* DCAC_STOP (210) : arrêt de l'onduleur seul. TODO(bug 14) : aucune transition n'y mène (branche d'origine
 * inatteignable, voir state_battery_mode) ; l'état est conservé à l'identique.
 * Relais : DCAC_IN ouvert. Sorties : EMERGENCY sur erreur, STANDBY après 1,5 s. */
static void state_dcac_stop(void)
{
    Board_SetDcacInputRelay(false);

    if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms == DCAC_STOP_DURATION)
    {
        Inverter_ClearDataAfterStop();
        go_to(SMB_STANDBY);
    }
}

/* STOPPING (240) : arrêt complet séquencé, un pas toutes les 100 ms :
 *  - pas 1 (100 ms) : ordre d'arrêt pile ; pas 2 (200 ms) : FCS_OUT ouvert ;
 *  - pas 15 (1,5 s) : DCAC_IN ouvert ; pas 50 (5 s) : tout ouvert, buzzer coupé, retour en STANDBY.
 * DCAC_PRE est ouvert à chaque pas. Sortie : EMERGENCY (avec arrêt critique de la pile) sur erreur. */
static void state_stopping(void)
{
    if (smb.critical_error_flag)
    {
        smb.step = SMB_EMERGENCY;
        Fcs_SendCriticalStop();
        timebase.ticks_100ms = 0;
        step_100ms_cnt = 0;
    }
    else if (timebase.ticks_100ms > 0) /* un pas de 100 ms s'est écoulé depuis le précédent */
    {
        Board_SetDcacPrechargeRelay(false);

        step_100ms_cnt++;
        if (step_100ms_cnt == 1) /* après 100 ms */
        {
            /* TODO(bug 1) : l'origine écrit "fcs_step = FCCB_OPERATING" (affectation) dans la
             * condition : toujours vraie, et l'état FCCB est écrasé jusqu'à la prochaine trame. */
            fcs.step = FCCB_OPERATING;
            Fcs_SendStop();
        }
        else if (step_100ms_cnt == 2) /* après 200 ms */
            Board_SetFcsOutputRelay(false);
        else if (step_100ms_cnt == 15) /* après 1.5 s */
            Board_SetDcacInputRelay(false);
        else if (step_100ms_cnt >= STOPPING_DURATION)
        {
            Board_SetDcacPrechargeRelay(false);
            Board_SetFcsOutputRelay(false);
            Board_SetDcacInputRelay(false);
            Board_SetBuzzer(false);
            Inverter_ClearDataAfterStop(); /* dernière trame onduleur à zéro vers l'IHM */
            smb.step = SMB_STANDBY;
            step_100ms_cnt = 0;
        }
        timebase.ticks_100ms = 0; /* attente du pas suivant */
    }
}

/* EMERGENCY (250) : mode défaut.
 * Relais : SUPPLY fermé, DCAC_PRE / DCAC_IN / FCS_OUT ouverts (commandés à chaque tour).
 * Buzzer : continu en cas de fuite H2, sinon basculé toutes les 300 ms ; coupé après 5 min (hmi.c).
 * Les mesures onduleur sont remises à zéro.
 * Sortie : AUTOCHECK, après acquittement de toutes les erreurs, sur appui long du bouton central ou sur
 * ordre CAN SMB_RX_RESET_ORDER. Si une erreur persiste, AUTOCHECK renvoie aussitôt en EMERGENCY. */
static void state_emergency(void)
{
    Board_RelaysSafeState();

    /* Buzzer : continu sur fuite H2, alterné sinon, coupé après 5 minutes */
    if (fcs.ppmh2_error_flag && !hmi.buzzer_5_minutes.error)
        Board_SetBuzzer(true);
    else if ((timebase.ticks_100ms > 2) && !fcs.ppmh2_error_flag && !hmi.buzzer_5_minutes.error)
    {
        Board_ToggleBuzzer();       /* bascule toutes les 300 ms */
        timebase.ticks_100ms = 0;
    }
    else if (hmi.buzzer_5_minutes.error)
        Board_SetBuzzer(false);

    Inverter_ClearData();

    if (hmi.middle_button.state || smb.reset_order_flag) /* sortie du mode défaut */
    {
        smb.reset_order_flag = 0;
        hmi.middle_button.state = 0;
        reset_all_errors();

        /* (origine) motif sonore évalué une seule fois sur la valeur courante du chronomètre */
        if (timebase.ticks_100ms == 8)
            Board_SetBuzzer(true);
        else if (timebase.ticks_100ms == 9)
            Board_SetBuzzer(false);
        else if (timebase.ticks_100ms == 10)
            Board_SetBuzzer(true);
        else if (timebase.ticks_100ms == 11)
        {
            Board_SetBuzzer(false);
            timebase.ticks_100ms = 0;
        }
        else
            Board_SetBuzzer(false);

        go_to(SMB_AUTOCHECK);
    }
    else if (timebase.ticks_100ms > 12)
        timebase.ticks_100ms = 0; /* borne le chronomètre (fuite H2) */
}

/* État inconnu (corruption mémoire, valeur inattendue) : mise en sécurité, erreur logicielle
 * (donc erreur critique au tour suivant) et passage en EMERGENCY. */
static void state_default(void)
{
    Board_RelaysSafeState();
    timebase.ticks_100ms = 0;
    smb.software_error_flag = 1;
    smb.step = SMB_EMERGENCY;
}

/* Point d'entrée, à chaque tour de la boucle principale : erreurs puis état courant. */
void Smb_Run(void)
{
    update_error_flags();

    switch (smb.step)
    {
    case SMB_INIT:          state_init(); break;
    case SMB_AUTOCHECK:     state_autocheck(); break;
    case SMB_STANDBY:       state_standby(); break;
    case SMB_MAINTENANCE:   state_maintenance(); break;
    case SMB_DCAC_START_1:  state_dcac_start_1(); break;
    case SMB_DCAC_START_2:  state_dcac_start_2(); break;
    case SMB_DCAC_START_3:  state_dcac_start_3(); break;
    case SMB_BATTERY_MODE:  state_battery_mode(); break;
    case SMB_FCS_START_1:   state_fcs_start_1(); break;
    case SMB_FCS_START_2:   state_fcs_start_2(); break;
    case SMB_HYBRID_MODE:   state_hybrid_mode(); break;
    case SMB_FC_STOP_1:     state_fc_stop_1(); break;
    case SMB_FC_STOP_2:     state_fc_stop_2(); break;
    case SMB_DCAC_STOP:     state_dcac_stop(); break;
    case SMB_STOPPING:      state_stopping(); break;
    case SMB_EMERGENCY:     state_emergency(); break;
    default:                state_default(); break;
    }
}
