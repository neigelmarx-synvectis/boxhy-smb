#!/usr/bin/env python3
"""Générateur de la base CAN des batteries PowerTech PowerModule.

Source : « PowerModule – Installation et manuel utilisateur » V0.2 (29/04/26), chapitre 10
« Protocole CAN ». Bus CAN 2.0B, identifiants étendus, données au format Intel, 250 kbit/s par défaut.

Les identifiants du PowerModule ne sont pas fixes : ils encodent l'émetteur (Master, Slave,
SuperMaster), le numéro de module et le numéro de stack (manuel §10.2.1) :

    bit 28      priorité        0 = haute, 1 = basse
    bits 25-24  device          0b10 = batterie, 0b11 = outil / contrôleur externe
    bits 23-22  émetteur        0b00 = broadcast, 0b01 = SuperMaster / MainMaster, 0b10 = slave, 0b11 = master
    bits 21-17  numéro de module (1..31 ; 0 pour les informations globales et les SuperMasters)
    bits 16-12  numéro de stack  (1..31 ; 0 pour les informations globales)
    bits 11-10  type de trame   0b00 = Tx, 0b01 = Rx (écriture), 0b10 = Dreq (demande)
    bits 9-0    command ID

Un fichier DBC ne connaît que des identifiants fixes : ce script instancie chaque message pour
chaque module d'une topologie donnée.

Usage :
  python3 tools/powermodule_dbc.py                      Master + 1 slave, sans SuperMaster, 500 kbit/s (défaut :
                                                        configuration BOXHY, 1 ou 2 modules)
  python3 tools/powermodule_dbc.py --baudrate 250000    débit usine des modules
  python3 tools/powermodule_dbc.py --modules 6          Master + 5 slaves en parallèle
  python3 tools/powermodule_dbc.py --supermaster --stacks 3 --modules 4
                                                        3 stacks de 4 slaves, un SuperMaster par stack
  python3 tools/powermodule_dbc.py --check              vérifie que le fichier par défaut est à jour
  python3 tools/powermodule_dbc.py --output autre.dbc   écrit ailleurs

Python standard uniquement.
"""
import argparse
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DEFAULT_OUTPUT = os.path.join(ROOT, "can database", "POWERMODULE.dbc")
ENCODING = "cp1252"

# ---- Champs de l'identifiant ------------------------------------------------------------------
PRIO_HIGH, PRIO_LOW = 0, 1
DEV_BATTERY, DEV_TOOL = 0b10, 0b11
TR_BROADCAST, TR_SUPERMASTER, TR_SLAVE, TR_MASTER = 0b00, 0b01, 0b10, 0b11
FR_TX, FR_RX, FR_DREQ = 0b00, 0b01, 0b10


def can_id(prio, dev, trans, module, stack, frame, cmd):
    """Identifiant étendu 29 bits (manuel §10.2.1)."""
    assert 0 <= module <= 31 and 0 <= stack <= 31 and 0 <= cmd <= 0x3FF
    return (prio << 28) | (dev << 24) | (trans << 22) | (module << 17) | (stack << 12) | (frame << 10) | cmd


# ---- Signaux --------------------------------------------------------------------------------------
class Sig:
    def __init__(self, name, start, length, signed=False, factor=1, offset=0, unit="", comment="", values=None,
                 vmin=None, vmax=None):
        self.name, self.start, self.length, self.signed = name, start, length, signed
        self.factor, self.offset, self.unit, self.comment = factor, offset, unit, comment
        self.values = values or []
        if signed:
            raw_min, raw_max = -(1 << (length - 1)), (1 << (length - 1)) - 1
        else:
            raw_min, raw_max = 0, (1 << length) - 1
        self.min = raw_min * factor + offset if vmin is None else vmin
        self.max = raw_max * factor + offset if vmax is None else vmax


def flag(name, start, comment=""):
    return Sig(name, start, 1, comment=comment)


class Msg:
    def __init__(self, name, mid, dlc, tx, rx, signals, comment="", cycle=None):
        self.name, self.id, self.dlc, self.tx, self.rx = name, mid, dlc, tx, rx
        self.signals, self.comment, self.cycle = signals, comment, cycle


# ---- Contenu des messages (manuel §10.2.3 et §10.2.4) --------------------------------------------
STATUS_SYSTEM_VALUES = [(0, "Idle"), (1, "Operative"), (2, "Error"), (3, "Precharge"), (4, "Part_operative")]


def global_status_1():
    """§10.2.3.1 / §10.2.3.3 - DLC 7."""
    return [
        Sig("soc_system", 0, 16, factor=0.01, unit="%", comment="Etat de charge du systeme."),
        Sig("imr_system", 16, 13, factor=0.1, unit="A",
            comment="Courant de charge maximal (IMR). Unite 0,1 A si imr_unit = 0 ; si imr_unit = 1 l'unite est "
                    "1 A : multiplier la valeur physique par 10."),
        flag("imr_unit", 29, "0 : imr_system en 100 mA ; 1 : imr_system en 1 A."),
        Sig("imd_system", 30, 11, factor=1, unit="A",
            comment="Courant de decharge maximal (IMD). Unite 1 A si imd_unit = 0 ; si imd_unit = 1 l'unite est "
                    "10 A : multiplier la valeur physique par 10."),
        flag("imd_unit", 41, "0 : imd_system en 1 A ; 1 : imd_system en 10 A."),
        Sig("status_system", 42, 4, values=STATUS_SYSTEM_VALUES,
            comment="Etat du systeme. 3 (Precharge) n'existe que pour un SuperMaster."),
        flag("error_system", 46, "Au moins un module (ou un rack) en erreur."),
        flag("warn_system", 47, "Au moins un module (ou un rack) en warning."),
        flag("end_of_charge_system", 48, "Fin de charge du systeme : chaque charge doit se terminer par ce signal."),
        flag("floating_system", 49, "Systeme en floating."),
    ]


def global_status_2():
    """§10.2.3.2 / §10.2.3.4 - DLC 6."""
    return [
        Sig("current_system", 0, 16, signed=True, factor=0.1, unit="A", comment="Courant total du systeme."),
        Sig("voltage_system", 16, 16, factor=0.025, unit="V", comment="Tension totale du systeme."),
        Sig("soh_system", 32, 16, factor=0.01, unit="%", comment="Etat de sante du systeme."),
    ]


def status_1_module():
    """§10.2.3.9 - Status 1 Master et Slave, DLC 8."""
    return [
        Sig("voltage", 0, 16, factor=0.001, unit="V", comment="Tension du pack."),
        Sig("current", 16, 16, signed=True, factor=0.01, unit="A",
            comment="Courant du module : positif en charge (entrant), negatif en decharge (sortant)."),
        Sig("soc", 32, 16, factor=0.01, unit="%", comment="Etat de charge."),
        Sig("soh", 48, 16, factor=0.01, unit="%", comment="Etat de sante."),
    ]


def status_2_module():
    """§10.2.3.10 - Status 2 Master et Slave, DLC 8."""
    return [
        flag("open_power_contactor_manually", 0, "Contacteur ouvert manuellement par la commande 0x20C."),
        flag("status_power_contactor", 1, "1 : contacteur de puissance ferme ; 0 : ouvert."),
        flag("reserved", 2),
        Sig("nb_cells_in_balancing", 3, 5, comment="Nombre de cellules en equilibrage (0 a 16)."),
        flag("status_balancing_active", 8, "Au moins un slave en equilibrage (emis par le Master seulement)."),
        flag("end_of_charge", 9, "Fin de charge : batterie pleine."),
        flag("status_heater", 10, "Chauffage actif."),
        flag("floating_mode", 11, "Mode floating."),
        Sig("imr", 16, 16, factor=0.1, unit="A", comment="Courant de charge maximal recommande."),
        Sig("imd", 32, 8, factor=1, unit="A", comment="Courant de decharge maximal recommande."),
        Sig("temp_board", 40, 8, offset=-40, unit="°C", comment="Temperature de la carte BMS."),
        Sig("temp_pack_1", 48, 8, offset=-40, unit="°C", comment="Temperature du banc de cellules 1."),
        Sig("temp_pack_2", 56, 8, offset=-40, unit="°C", comment="Temperature du banc de cellules 2."),
    ]


def error_warning_module():
    """§10.2.3.5 - Error et Warning Master et Slave, DLC 8. Seuils par défaut, paramétrables."""
    e = [
        ("err_output_current", 0, "#1 : courant de sortie > 300 A, ou > 260 A pendant 30 s, ou > 200 A pendant 300 s."),
        ("err_input_current", 1, "#2 : courant d'entree > 110 A, ou > 105 A pendant 30 s."),
        ("err_cell_over_voltage", 2, "#3 : une cellule > 3850 mV."),
        ("err_cell_under_voltage", 3, "#4 : une cellule < 2700 mV."),
        ("err_cell_over_temp", 4, "#5 : pack > 60 °C."),
        ("err_cell_under_temp", 5, "#6 : pack < 0 °C en charge ou < -20 °C en decharge."),
        ("err_bms_board_over_temp", 6, "#7 : carte BMS > 75 °C."),
        ("err_bms_board_under_temp", 7, "#8 : carte BMS < -20 °C."),
        ("err_power_contactor", 8, "#19 : contacteur ouvert 3 fois / 3 erreurs en 15 minutes."),
        ("err_lose_comm_master", 9, "#9 : le slave a perdu la communication avec le master."),
        ("err_lose_comm_slave", 10, "#20 : le master a perdu la communication avec un slave."),
        ("err_current_inconsistency", 12, "#10 : courant du slave incoherent avec celui des autres slaves."),
        ("err_voltage_bus", 13, "#21 : erreur de tension de bus."),
        ("err_heater", 14, "#22 : erreur de chauffage."),
        ("err_stack", 15, "#23 : erreur dans le stack ou erreur de tension de bus du stack."),
        ("err_lifeline", 16, "#24 : Lifeline deconnectee (0 si connectee ou option absente)."),
        ("err_ground", 17, "#25 : defaut de masse detecte dans le stack."),
        ("err_imr", 18, "#26 : courant d'entree > IMR avec temperature negative."),
        ("err_imd", 19, "#27 : courant de sortie > IMD avec temperature negative."),
        ("warn_output_current", 32, "#11 : courant de sortie > 200 A."),
        ("warn_input_current", 33, "#12 : courant d'entree > 105 A."),
        ("warn_cell_over_voltage", 34, "#13 : une cellule > 3680 mV."),
        ("warn_cell_under_voltage", 35, "#14 : une cellule < 3000 mV."),
        ("warn_cell_over_temp", 36, "#15 : pack > 58 °C."),
        ("warn_cell_under_temp", 37, "#16 : pack < 2 °C en charge ou < -18 °C en decharge."),
        ("warn_bms_board_over_temp", 38, "#17 : carte BMS > 58 °C."),
        ("warn_bms_board_under_temp", 39, "#18 : carte BMS < -18 °C."),
        ("warn_imr", 40, "#28 : courant d'entree > IMR."),
        ("warn_imd", 41, "#29 : courant de sortie > IMD."),
    ]
    return [flag(n, s, c) for n, s, c in e]


def error_warning_supermaster():
    """§10.2.3.6 - Error et Warning SuperMaster, DLC 8."""
    return [
        flag("err_output_current", 0, "#30 : courant de sortie trop eleve (capteur a effet Hall du SuperMaster)."),
        flag("err_input_current", 1, "#31 : courant d'entree trop eleve (capteur a effet Hall du SuperMaster)."),
        flag("timeout_precharge", 2, "#32 : delai de precharge depasse."),
        flag("err_power_contactor", 3, "#33 : contacteur ouvert 3 fois sur erreur en une heure."),
        flag("err_lose_comm_main_master", 4, "#34 : le SuperMaster a perdu la communication avec le MainMaster."),
        flag("err_lose_comm_supermaster", 5, "#35 : le MainMaster a perdu la communication avec un SuperMaster."),
        flag("err_lose_comm_slave", 6, "#36 : le SuperMaster a perdu la communication avec un slave."),
        flag("err_voltage_bus", 7, "#37 : en precharge, bus < 80 % de la tension batterie ; ou, pour rejoindre "
                                   "les stacks, tension du stack hors 95..105 % de la tension de bus."),
        Sig("nb_slave_error", 8, 5, comment="Nombre de slaves en erreur."),
        flag("err_lifeline", 13, "#38 : Lifeline deconnectee."),
        flag("err_ground", 14, "#39 : defaut de masse detecte dans le stack."),
        flag("err_current_inconsistency", 15, "#40 : courant du stack incoherent avec les autres stacks."),
        flag("err_insulation_contactor_open", 16, "#46 : tension d'isolement > 30 V contacteur ouvert."),
        flag("warn_output_current", 32, "#41 : surintensite de sortie (capteur du SuperMaster)."),
        flag("warn_input_current", 33, "#42 : surintensite d'entree (capteur du SuperMaster)."),
        Sig("nb_slave_warning", 34, 5, comment="Nombre de slaves en warning."),
        flag("warn_lose_iso_board", 39, "#43 : communication perdue avec la carte d'isolement."),
        flag("warn_bat_voltage_iso", 40, "#44 : tension batterie de la carte d'isolement hors +-5 %."),
        flag("warn_insulation_contactor_closed", 41, "#45 : tension d'isolement > 30 V contacteur ferme."),
    ]


def status_1_supermaster():
    """§10.2.3.7 - DLC 8."""
    return [
        Sig("voltage", 0, 16, factor=0.025, unit="V", comment="Tension du rack calculee par le SuperMaster."),
        Sig("current", 16, 16, signed=True, factor=0.1, unit="A",
            comment="Courant du stack : positif entrant, negatif sortant."),
        Sig("soc", 32, 16, factor=0.01, unit="%", comment="Etat de charge."),
        Sig("soh", 48, 16, factor=0.01, unit="%", comment="Etat de sante."),
    ]


def status_2_supermaster():
    """§10.2.3.8 - DLC 8."""
    return [
        flag("open_power_contactor_manually", 0, "Contacteur ouvert manuellement par commande CAN."),
        flag("status_power_contactor", 1, "1 : contacteur de puissance ferme ; 0 : ouvert."),
        flag("status_precharge", 2, "1 : precharge active."),
        Sig("nb_slave_in_balancing", 3, 5, comment="Nombre de slaves en equilibrage."),
        Sig("nb_slave_inter_balancing", 8, 5, comment="Nombre de slaves en equilibrage inter-modules."),
        flag("end_of_charge", 13, "Fin de charge : batterie pleine."),
        flag("status_heater", 14, "Chauffage actif."),
        flag("floating_mode", 15, "Mode floating."),
        Sig("imr", 16, 14, factor=0.1, unit="A", comment="Courant de charge maximal recommande."),
        Sig("imd", 30, 12, factor=1, unit="A", comment="Courant de decharge maximal recommande."),
        Sig("status_supermaster", 42, 4, values=STATUS_SYSTEM_VALUES[:3], comment="Etat du SuperMaster."),
        Sig("voltage_bus", 48, 16, factor=0.1, unit="V", comment="Tension de bus mesuree par le SuperMaster."),
    ]


def cell_voltages(first):
    """§10.2.4.2 - quatre tensions de cellules, DLC 8."""
    return [Sig("cell_voltage_%d" % (first + i), 16 * i, 16, factor=0.001, unit="V",
                comment="Tension de la cellule %d." % (first + i)) for i in range(4)]


def cell_balancing():
    """§10.2.4.3 - DLC 2."""
    return [flag("bal_cell_%d" % (i + 1), i, "1 : cellule %d en equilibrage." % (i + 1)) for i in range(16)]


def parameters():
    """§10.2.4.6.1 - DLC 3 (un paramètre) ou 8 (trois paramètres)."""
    return [
        Sig("address_parameter", 0, 8, comment="Adresse du premier parametre en E2PROM."),
        Sig("number_parameter", 8, 8, comment="Nombre de parametres dans la trame (1 : DLC 3 ; 3 : DLC 8)."),
        Sig("parameter_1", 16, 16),
        Sig("parameter_2", 32, 16),
        Sig("parameter_3", 48, 16),
    ]


def maintenance_request():
    """§10.2.4.1 - DLC 2 (le manuel indique 1, mais les demandes de status vont jusqu'au bit 13)."""
    return [
        flag("req_cell_voltage", 0, "Demande l'envoi des tensions de cellules."),
        flag("req_cell_balancing", 1, "Demande l'envoi de l'etat d'equilibrage des cellules."),
        flag("req_all_slave_status", 2, "Demande au SuperMaster les Status 1 et 2 de tous les slaves."),
        flag("req_time", 3, "Reserve SuperMaster."),
        flag("req_renumber_slave", 4, "Reserve SuperMaster."),
        flag("req_reboot_all", 5, "Reserve SuperMaster."),
        flag("req_slave_error_warning", 6, "Demande au SuperMaster les messages Error/Warning des slaves."),
        flag("req_slave_status_3", 7, "Min/max de tension et de courant batterie."),
        flag("req_slave_status_1", 12, "Demande au SuperMaster le Status 1 d'un slave."),
        flag("req_slave_status_2", 13, "Demande au SuperMaster le Status 2 d'un slave."),
    ]


def contactor_command():
    """§10.2.4.4 - DLC 1."""
    return [
        flag("close_power_contactor", 0, "1 : ferme le contacteur de puissance (s'il n'y a pas d'erreur)."),
        flag("open_power_contactor", 1, "1 : ouvre le contacteur de puissance."),
        flag("close_all_power_contactor", 2, "Reserve."),
        flag("open_all_power_contactor", 3, "Reserve."),
    ]


def heater_command():
    """§10.2.4.5 - DLC 4."""
    return [
        Sig("ctl_heat", 0, 8, values=[(0, "Disable"), (1, "Active")], comment="Active ou desactive le chauffage."),
        Sig("temp_heat", 8, 8, offset=-40, unit="°C", vmin=-20, vmax=60,
            comment="Temperature cible (brut 20 a 100, soit -20 a 60 °C)."),
        Sig("rate_heat", 16, 8, vmin=1, vmax=10, comment="Taux d'activation du chauffage (1 a 10)."),
        Sig("delay_heat", 24, 8, unit="min",
            comment="Maintien du chauffage apres atteinte de la temperature cible."),
    ]


def set_parameter():
    """§10.2.4.6.2 - DLC 3."""
    return [
        Sig("address_parameter", 0, 8, comment="Adresse E2PROM a ecrire."),
        Sig("data_1", 8, 8, comment="Octet 1 de la nouvelle valeur."),
        Sig("data_2", 16, 8, comment="Octet 2 de la nouvelle valeur."),
    ]


# ---- Topologie --------------------------------------------------------------------------------------
class Node:
    """Un émetteur batterie : master, slave ou SuperMaster."""
    def __init__(self, name, trans, module, stack):
        self.name, self.trans, self.module, self.stack = name, trans, module, stack


def topology(args):
    """Retourne (nœud global, liste des modules, liste des SuperMasters)."""
    if args.supermaster:
        sms = [Node("PM_SM%d" % s, TR_SUPERMASTER, 0, s) for s in range(1, args.stacks + 1)]
        modules = [Node("PM_S%dM%d" % (s, m), TR_SLAVE, m, s)
                   for s in range(1, args.stacks + 1) for m in range(1, args.modules + 1)]
        return Node("PM_SM1", TR_SUPERMASTER, 0, 0), modules, sms
    master = Node("PM_MASTER", TR_MASTER, 1, 1)
    slaves = [Node("PM_S1M%d" % m, TR_SLAVE, m, 1) for m in range(2, args.modules + 1)]
    return Node("PM_MASTER", TR_MASTER, 0, 0), [master] + slaves, []


def tag(node):
    return "MASTER" if node.trans == TR_MASTER else "S%dM%d" % (node.stack, node.module)


def build(args):
    glob, modules, sms = topology(args)
    ctrl = "CONTROLLER"
    rx_all = ",".join([n.name for n in modules] + [n.name for n in sms]) or ctrl
    msgs = []
    who = "SuperMaster 1 (MainMaster)" if args.supermaster else "Master"

    # Messages globaux (module 0, stack 0)
    gname = "MAINMASTER" if args.supermaster else "MASTER"
    msgs.append(Msg("PM_GLOBAL_STATUS1_" + gname, can_id(PRIO_HIGH, DEV_BATTERY, glob.trans, 0, 0, FR_TX, 0x002), 7,
                    glob.name, ctrl, global_status_1(),
                    "Global Status 1 emis par le %s : SOC, IMR, IMD et etat du systeme." % who, 1000))
    msgs.append(Msg("PM_GLOBAL_STATUS2_" + gname, can_id(PRIO_HIGH, DEV_BATTERY, glob.trans, 0, 0, FR_TX, 0x003), 6,
                    glob.name, ctrl, global_status_2(),
                    "Global Status 2 emis par le %s : courant, tension et SOH du systeme." % who, 1000))

    # SuperMasters
    for sm in sms:
        t = "SM%d" % sm.stack
        msgs.append(Msg("PM_SM_STATUS1_" + t, can_id(PRIO_LOW, DEV_BATTERY, TR_SUPERMASTER, 0, sm.stack, FR_TX, 0x020),
                        8, sm.name, ctrl, status_1_supermaster(),
                        "Status 1 du SuperMaster du stack %d." % sm.stack, 1000))
        msgs.append(Msg("PM_SM_STATUS2_" + t, can_id(PRIO_LOW, DEV_BATTERY, TR_SUPERMASTER, 0, sm.stack, FR_TX, 0x021),
                        8, sm.name, ctrl, status_2_supermaster(),
                        "Status 2 du SuperMaster du stack %d." % sm.stack, 1000))
        msgs.append(Msg("PM_SM_ERRWARN_" + t, can_id(PRIO_HIGH, DEV_BATTERY, TR_SUPERMASTER, 0, sm.stack, FR_TX, 0x022),
                        8, sm.name, ctrl, error_warning_supermaster(),
                        "Erreurs et warnings du SuperMaster du stack %d. Emis toutes les 500 ms tant qu'un evenement "
                        "est present, puis deux fois a 0." % sm.stack, 500))
        msgs.append(Msg("PM_CMD_MAINTENANCE_" + t, can_id(PRIO_LOW, DEV_TOOL, TR_BROADCAST, 0, sm.stack, FR_DREQ, 0x205),
                        2, ctrl, sm.name, maintenance_request(),
                        "Demande de maintenance adressee au SuperMaster du stack %d." % sm.stack))

    # Modules (master et slaves)
    for n in modules:
        t = tag(n)
        what = "du master" if n.trans == TR_MASTER else "du slave %d du stack %d" % (n.module, n.stack)
        def bid(prio, cmd, n=n):
            return can_id(prio, DEV_BATTERY, n.trans, n.module, n.stack, FR_TX, cmd)
        def cid(prio, frame, cmd, n=n):
            return can_id(prio, DEV_TOOL, TR_BROADCAST, n.module, n.stack, frame, cmd)
        msgs += [
            Msg("PM_STATUS1_" + t, bid(PRIO_LOW, 0x020), 8, n.name, ctrl, status_1_module(),
                "Status 1 %s : tension, courant, SOC, SOH." % what, 1000),
            Msg("PM_STATUS2_" + t, bid(PRIO_LOW, 0x021), 8, n.name, ctrl, status_2_module(),
                "Status 2 %s : contacteur, equilibrage, IMR, IMD, temperatures." % what, 1000),
            Msg("PM_ERRWARN_" + t, bid(PRIO_HIGH, 0x022), 8, n.name, ctrl, error_warning_module(),
                "Erreurs et warnings %s. Emis toutes les 500 ms tant qu'un evenement est present, puis deux fois a 0. "
                "Une erreur ouvre le contacteur de puissance ; un warning est informatif." % what, 500),
            Msg("PM_CELLV_1_4_" + t, bid(PRIO_LOW, 0x103), 8, n.name, ctrl, cell_voltages(1),
                "Tensions des cellules 1 a 4 %s (sur demande de maintenance)." % what),
            Msg("PM_CELLV_5_8_" + t, bid(PRIO_LOW, 0x104), 8, n.name, ctrl, cell_voltages(5),
                "Tensions des cellules 5 a 8 %s (sur demande de maintenance)." % what),
            Msg("PM_CELLV_9_12_" + t, bid(PRIO_LOW, 0x109), 8, n.name, ctrl, cell_voltages(9),
                "Tensions des cellules 9 a 12 %s (sur demande de maintenance)." % what),
            Msg("PM_CELLV_13_16_" + t, bid(PRIO_LOW, 0x110), 8, n.name, ctrl, cell_voltages(13),
                "Tensions des cellules 13 a 16 %s (sur demande de maintenance)." % what),
            # Le manuel donne 0x1Ayyy209 : le bit 27 (non utilise dans le format d'identifiant) y est a 1.
            Msg("PM_CELL_BALANCING_" + t, bid(PRIO_LOW, 0x209) | (1 << 27), 2, n.name, ctrl, cell_balancing(),
                "Cellules en equilibrage %s. Identifiant 0x1Ayyy209 repris tel quel du manuel (bit 27 a 1)." % what),
            Msg("PM_PARAMETERS_" + t, bid(PRIO_LOW, 0x200), 8, n.name, ctrl, parameters(),
                "Parametres E2PROM %s, en reponse a PM_CMD_REQUEST_PARAM. DLC 3 (un parametre) ou 8 (trois)." % what),
            Msg("PM_CMD_MAINTENANCE_" + t, cid(PRIO_LOW, FR_DREQ, 0x205), 2, ctrl, n.name, maintenance_request(),
                "Demande de maintenance adressee %s. Le manuel indique un DLC de 1, mais les demandes de status "
                "utilisent les bits 12 et 13." % what),
            Msg("PM_CMD_CONTACTOR_" + t, cid(PRIO_HIGH, FR_RX, 0x20C), 1, ctrl, n.name, contactor_command(),
                "Ouverture / fermeture du contacteur de puissance %s. Apres 3 ouvertures sur erreur en 15 minutes, "
                "le contacteur reste ouvert 15 minutes." % what),
            Msg("PM_CMD_HEATER_" + t, cid(PRIO_HIGH, FR_DREQ, 0x211), 4, ctrl, n.name, heater_command(),
                "Gestion du chauffage %s." % what),
            Msg("PM_CMD_SET_PARAM_" + t, can_id(PRIO_LOW, DEV_TOOL, TR_BROADCAST, n.module, n.stack, FR_RX, 0x200), 3,
                ctrl, n.name, set_parameter(), "Ecriture d'un parametre E2PROM %s." % what),
            Msg("PM_CMD_REQUEST_PARAM_" + t, can_id(PRIO_LOW, DEV_TOOL, TR_BROADCAST, n.module, n.stack, FR_DREQ, 0x200),
                0, ctrl, n.name, [], "Demande de tous les parametres E2PROM %s (sans donnees)." % what),
        ]
    nodes = [ctrl] + sorted({m.tx for m in msgs if m.tx != ctrl})
    return msgs, nodes, rx_all


# ---- Écriture DBC -------------------------------------------------------------------------------------
def num(x):
    x = float(x)
    if x == int(x):
        return str(int(x))
    return ("%.6f" % x).rstrip("0").rstrip(".")


def write_dbc(msgs, nodes, args):
    ext = lambda mid: mid | 0x80000000
    topo = ("%d stack(s) de %d slave(s) avec SuperMaster" % (args.stacks, args.modules) if args.supermaster
            else "master + %d slave(s) en parallele, sans SuperMaster" % (args.modules - 1))
    out = ['VERSION ""', "", "", "NS_ :", "\tNS_DESC_", "\tCM_", "\tBA_DEF_", "\tBA_", "\tVAL_", "\tBA_DEF_DEF_",
           "", "BS_:", "", "BU_: " + " ".join(nodes), "", ""]
    for m in msgs:
        out.append("BO_ %d %s: %d %s" % (ext(m.id), m.name, m.dlc, m.tx))
        for s in m.signals:
            out.append(' SG_ %s : %d|%d@1%s (%s,%s) [%s|%s] "%s" %s' % (
                s.name, s.start, s.length, "-" if s.signed else "+", num(s.factor), num(s.offset),
                num(s.min), num(s.max), s.unit, m.rx))
        out.append("")
    out.append("")
    out.append('CM_ "Batteries PowerTech PowerModule - protocole CAN du manuel V0.2 (29/04/26), chapitre 10. '
               'CAN 2.0B, identifiants etendus, format Intel, 250 kbit/s par defaut, regle ici a %d bit/s. Topologie : %s. '
               'FICHIER GENERE par tools/powermodule_dbc.py : ne pas modifier a la main.";' % (args.baudrate, topo))
    out.append('CM_ BU_ CONTROLLER "Controleur externe (outil, device 0b11) : envoie les commandes et lit les status.";')
    for m in msgs:
        if m.comment:
            out.append('CM_ BO_ %d "%s";' % (ext(m.id), m.comment))
        for s in m.signals:
            if s.comment:
                out.append('CM_ SG_ %d %s "%s";' % (ext(m.id), s.name, s.comment))
    out += ['BA_DEF_  "BusType" STRING ;',
            'BA_DEF_  "Baudrate" INT 0 1000000;',
            'BA_DEF_ BO_  "VFrameFormat" ENUM  "StandardCAN","ExtendedCAN","reserved","J1939PG";',
            'BA_DEF_ BO_  "GenMsgCycleTime" INT 0 65535;',
            'BA_DEF_DEF_  "BusType" "CAN";',
            'BA_DEF_DEF_  "Baudrate" 250000;',  # défaut usine [PM]
            'BA_DEF_DEF_  "VFrameFormat" "ExtendedCAN";',
            'BA_DEF_DEF_  "GenMsgCycleTime" 0;',
            'BA_ "Baudrate" %d;' % args.baudrate]
    for m in msgs:
        out.append('BA_ "VFrameFormat" BO_ %d 1;' % ext(m.id))
        if m.cycle:
            out.append('BA_ "GenMsgCycleTime" BO_ %d %d;' % (ext(m.id), m.cycle))
    for m in msgs:
        for s in m.signals:
            if s.values:
                out.append("VAL_ %d %s %s ;" % (ext(m.id), s.name, " ".join('%d "%s"' % v for v in s.values)))
    return ("\r\n".join(out) + "\r\n").encode(ENCODING)


def validate(msgs):
    ids = {}
    for m in msgs:
        if m.id in ids:
            raise SystemExit("identifiant 0x%X en double : %s et %s" % (m.id, ids[m.id], m.name))
        ids[m.id] = m.name
        used = {}
        for s in m.signals:
            for b in range(s.start, s.start + s.length):
                if b >= 8 * m.dlc:
                    raise SystemExit("%s.%s : bit %d hors du DLC %d" % (m.name, s.name, b, m.dlc))
                if b in used:
                    raise SystemExit("%s.%s chevauche %s" % (m.name, s.name, used[b]))
                used[b] = s.name


def main():
    ap = argparse.ArgumentParser(description="Génère la base CAN des batteries PowerModule.")
    ap.add_argument("--supermaster", action="store_true", help="topologie avec un SuperMaster par stack")
    ap.add_argument("--stacks", type=int, default=1, help="nombre de stacks (avec --supermaster)")
    ap.add_argument("--modules", type=int, default=2,
                    help="modules par stack (sans SuperMaster : master compris, modules en parallèle)")
    ap.add_argument("--baudrate", type=int, default=500000, choices=(125000, 250000, 500000, 1000000),
                    help="débit du bus (BOXHY : 500 kbit/s, modules reconfigurés ; usine : 250 kbit/s)")
    ap.add_argument("--output", default=DEFAULT_OUTPUT)
    ap.add_argument("--check", action="store_true", help="vérifie que le fichier est à jour")
    args = ap.parse_args()
    if not 1 <= args.modules <= 31 or not 1 <= args.stacks <= 31:
        ap.error("1 à 31 modules et 1 à 31 stacks")
    if not args.supermaster and args.stacks != 1:
        ap.error("plusieurs stacks nécessitent --supermaster")
    msgs, nodes, _ = build(args)
    validate(msgs)
    content = write_dbc(msgs, nodes, args)
    current = open(args.output, "rb").read() if os.path.exists(args.output) else None
    rel = os.path.relpath(args.output, ROOT)
    nsig = sum(len(m.signals) for m in msgs)
    if args.check:
        if current != content:
            print("powermodule_dbc: %s obsolète ; lancer « python3 tools/powermodule_dbc.py »" % rel, file=sys.stderr)
            return 1
        print("powermodule_dbc: %d messages, %d signaux, %s à jour" % (len(msgs), nsig, rel))
        return 0
    if current != content:
        with open(args.output, "wb") as f:
            f.write(content)
    print("powermodule_dbc: %d messages, %d signaux ; %s %s" % (len(msgs), nsig, rel,
                                                              "écrit" if current != content else "inchangé"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
