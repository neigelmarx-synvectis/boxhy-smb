/**
 * @file can_signal.h
 * @brief Couche CAN générique : lecture / écriture de signaux décrits par leurs paramètres DBC,
 *        et codage / décodage de trames entières à partir de tables de liaison signal <-> variable.
 *
 * La disposition des signaux vient de src/can/can_db.h, généré depuis can database/BOXHY.dbc par
 * tools/candb_gen.py. Ce module ne connaît aucune trame particulière.
 *
 * Conventions DBC :
 *  - Intel (little-endian, « @1 ») : start_bit = position du bit de poids faible ;
 *  - Motorola (big-endian, « @0 ») : start_bit = position du bit de poids fort, numérotation
 *    en dents de scie (bit 7 de l'octet 0 = 7, bit 0 de l'octet 1 = 8...) ;
 *  - valeur physique = brut × facteur + offset ; flottant IEEE 754 32 bits si SIG_VALTYPE_ 1.
 *
 * Liaisons : une variable du firmware est exprimée dans sa propre unité (VAR_UNIT, valeur physique
 * d'un LSB). La conversion var = brut × mul / div + offset est précalculée à la compilation par
 * CAN_BIND() à partir du facteur DBC : aucune multiplication flottante par 0,01 à l'exécution.
 *
 * Utilisation type (voir can_tx.c, battery.c, fcs.c, maintenance.c, can_rx.c) :
 * @code
 *   static const sCanBinding currents[] CAN_ROM = {
 *       CAN_BIND(CANDB_SMB_TX_CURRENTS__FCS_OUTPUT_CURRENT, &fcs.output_current, CAN_VAR_I16, 0.01),
 *       ...                                   // un élément par signal du message dans le .dbc
 *   };
 *   CAN_CHECK_BINDINGS(CANDB_SMB_TX_CURRENTS, currents);       // oubli d'un signal = erreur de compilation
 *   static const sCanMessage msg CAN_ROM = CAN_MESSAGE(CANDB_SMB_TX_CURRENTS, currents);
 *
 *   CanMsg_Send(&msg, msg.dlc, CAN_PRIORITY_LOW);               // émission
 *   CanMsg_Unpack(CanMsg_Find(table, n, id), data);             // réception
 * @endcode
 */
#ifndef CAN_SIGNAL_H
#define CAN_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mcc_generated_files/can_types.h"

/* ---- Disposition d'un signal ------------------------------------------------------------ */
/* Drapeaux de sCanSignalLayout.flags : un ordre d'octets OU un type de valeur. */
#define CAN_INTEL      0x00u
#define CAN_MOTOROLA   0x01u
#define CAN_UNSIGNED   0x00u
#define CAN_SIGNED     0x02u
#define CAN_FLOAT32    0x04u

/** Disposition d'un signal dans les 8 octets de données, telle que décrite dans le .dbc.
 *  Les valeurs sont générées dans can_db.h (macros CANDB_<MSG>__<SIG>_LAYOUT). */
typedef struct
{
    uint8_t start_bit; /**< bit de départ DBC (voir conventions ci-dessus) */
    uint8_t length;    /**< 1..32 bits */
    uint8_t flags;     /**< CAN_INTEL|CAN_MOTOROLA, CAN_UNSIGNED|CAN_SIGNED|CAN_FLOAT32 */
} sCanSignalLayout;

/** Initialiseur d'un sCanSignalLayout, utilisé par le fichier généré can_db.h. */
#define CAN_LAYOUT(start, length, flags) { (start), (length), (flags) }

/** Bits bruts du signal (non signés, alignés à droite). */
uint32_t CanSignal_GetRaw(const uint8_t *data, const sCanSignalLayout *s);
/** Valeur brute entière, avec extension de signe si le signal est signé. */
int32_t CanSignal_GetInt(const uint8_t *data, const sCanSignalLayout *s);
/** Valeur brute d'un signal flottant 32 bits. */
float CanSignal_GetFloat(const uint8_t *data, const sCanSignalLayout *s);
/** Écrit les bits du signal (valeur masquée à sa longueur) sans modifier les autres bits. */
void CanSignal_SetRaw(uint8_t *data, const sCanSignalLayout *s, uint32_t raw);
/** Écrit un signal flottant 32 bits. */
void CanSignal_SetFloat(uint8_t *data, const sCanSignalLayout *s, float value);

/* ---- Liaisons signal <-> variable ------------------------------------------------------- */
/* Type C de la variable liée (sCanBinding.var_type) : la couche générique lit / écrit la
 * variable avec exactement cette taille et ce signe. */
#define CAN_VAR_NONE   0u /**< signal ignoré (réception) / laissé à 0 (émission) */
#define CAN_VAR_CONST  1u /**< émission d'une valeur brute constante (champ offset) */
#define CAN_VAR_BOOL   2u
#define CAN_VAR_U8     3u
#define CAN_VAR_I8     4u
#define CAN_VAR_U16    5u
#define CAN_VAR_I16    6u
#define CAN_VAR_U32    7u
#define CAN_VAR_I32    8u

/** Liaison d'un signal à une variable du firmware. Construite par CAN_BIND / CAN_BIND_CONST /
 *  CAN_BIND_NONE, jamais à la main, pour que mul / div / offset restent cohérents avec le .dbc. */
typedef struct
{
    sCanSignalLayout layout;
    uint8_t var_type; /**< CAN_VAR_* */
    void *var;        /**< variable liée (NULL pour NONE et CONST) */
    int16_t mul;      /**< var = brut × mul / div + offset */
    int16_t div;
    int16_t offset;   /**< pour CAN_VAR_CONST : valeur brute émise */
} sCanBinding;

/** Message CAN : identifiant, format, DLC et table de liaisons (une par signal du .dbc).
 *  Construit par CAN_MESSAGE / CAN_MESSAGE_EMPTY. */
typedef struct
{
    uint32_t id;
    uint8_t extended;
    uint8_t dlc;
    uint8_t nb_bindings;
    const sCanBinding *bindings;
} sCanMessage;

/* Tables en mémoire programme (lues via la fenêtre PSV) : avec -mconst-in-data, un simple
 * « const » serait recopié en RAM. Sur PC (tests), attribut vide.
 * Sur dsPIC33E, la fenêtre PSV présente la flash dans l'espace données à partir de 0x8000 ;
 * le code de démarrage (__psv_init) règle DSRPAG sur la page des tables, et les pointeurs
 * ordinaires vers ces tables fonctionnent donc sans code particulier. */
#if defined(__XC16__)
#define CAN_ROM __attribute__((space(auto_psv)))
#else
#define CAN_ROM
#endif

/* Conversion précalculée : ratio = facteur DBC / unité de la variable.
 * |ratio| >= 1 -> mul = ratio, div = 1 ; |ratio| < 1 -> mul = ±1, div = 1 / |ratio|.
 * Ces expressions flottantes sont évaluées par le compilateur (initialiseurs statiques) : seuls
 * les entiers arrondis mul / div / offset sont rangés dans les tables.
 * Exemples : facteur 0.01, variable en 0.01 -> 1 / 1 ; facteur 0.01 (SOC en 0,01 %), variable en %
 * -> mul 1, div 100 ; facteur 100 (points ADC / 100), variable en points -> mul 100, div 1 ;
 * facteur -1 (courant IBS en A, signe inversé), variable en 0,01 A -> mul -100, div 1.
 * Le ratio doit être entier ou l'inverse d'un entier, sinon l'arrondi fausse la conversion. */
#define CAN_RATIO_(f, u)   ((double)(f) / (double)(u))
#define CAN_ABS_(x)        ((x) < 0 ? -(x) : (x))
#define CAN_ROUND16_(x)    ((int16_t)((x) >= 0 ? (x) + 0.5 : (x) - 0.5))
#define CAN_MUL_(f, u)     (CAN_ABS_(CAN_RATIO_(f, u)) >= 1.0 ? CAN_ROUND16_(CAN_RATIO_(f, u)) \
                                                              : (int16_t)(CAN_RATIO_(f, u) < 0 ? -1 : 1))
#define CAN_DIV_(f, u)     (CAN_ABS_(CAN_RATIO_(f, u)) >= 1.0 ? (int16_t)1 \
                                                              : CAN_ROUND16_(1.0 / CAN_ABS_(CAN_RATIO_(f, u))))
#define CAN_OFS_(o, u)     CAN_ROUND16_((double)(o) / (double)(u))

/**
 * Lie le signal SIG (préfixe généré dans can_db.h, par ex. CANDB_SMB_TX_GENERAL__STEP) à une variable.
 * @param var_type CAN_VAR_* correspondant au type C de la variable
 * @param var_unit valeur physique (unité DBC) d'un LSB de la variable, par ex. 0.01 pour des 0,01 A
 */
#define CAN_BIND(SIG, ptr, var_type, var_unit) \
    { SIG##_LAYOUT, (var_type), (void *)(ptr), CAN_MUL_(SIG##_FACTOR, var_unit), \
      CAN_DIV_(SIG##_FACTOR, var_unit), CAN_OFS_(SIG##_OFFSET, var_unit) }
/** Émet toujours la valeur brute `raw`. */
#define CAN_BIND_CONST(SIG, raw)  { SIG##_LAYOUT, CAN_VAR_CONST, NULL, 1, 1, (int16_t)(raw) }
/** Signal non utilisé par le firmware (ignoré en réception, 0 en émission). */
#define CAN_BIND_NONE(SIG)        { SIG##_LAYOUT, CAN_VAR_NONE, NULL, 1, 1, 0 }

/** Nombre d'éléments d'un tableau. */
#define CAN_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Les macros publiques passent par une macro interne « _ » pour que MSG soit développé
 * (par ex. un alias local #define M CANDB_...) avant le collage de jetons. */

/** Description d'un message à partir de sa table de liaisons. */
#define CAN_MESSAGE(MSG, table)  CAN_MESSAGE_(MSG, table)
#define CAN_MESSAGE_(MSG, table) \
    { MSG##_ID, MSG##_EXTENDED, MSG##_DLC, (uint8_t)CAN_ARRAY_SIZE(table), (table) }
#define CAN_MESSAGE_EMPTY(MSG)   CAN_MESSAGE_EMPTY_(MSG)
#define CAN_MESSAGE_EMPTY_(MSG) \
    { MSG##_ID, MSG##_EXTENDED, MSG##_DLC, 0u, NULL }

/** Contrôle à la compilation : chaque signal du .dbc a exactement une liaison.
 *  Si la condition est fausse, le typedef déclare un tableau de taille -1 : erreur de compilation
 *  dont le message cite « can_static_assert_<MESSAGE> ». (Forme compatible C99 / XC16, sans
 *  _Static_assert.) Le contrôle porte sur le nombre de liaisons, pas sur un doublon éventuel. */
#define CAN_STATIC_ASSERT(cond, tag)  CAN_STATIC_ASSERT_(cond, tag)
#define CAN_STATIC_ASSERT_(cond, tag) typedef char can_static_assert_##tag[(cond) ? 1 : -1]
#define CAN_CHECK_BINDINGS(MSG, table)  CAN_CHECK_BINDINGS_(MSG, table)
#define CAN_CHECK_BINDINGS_(MSG, table) \
    CAN_STATIC_ASSERT(CAN_ARRAY_SIZE(table) == MSG##_NB_SIGNALS, MSG)
#define CAN_CHECK_EMPTY(MSG)  CAN_CHECK_EMPTY_(MSG)
#define CAN_CHECK_EMPTY_(MSG) CAN_STATIC_ASSERT(MSG##_NB_SIGNALS == 0u, MSG)

/* ---- Messages ---------------------------------------------------------------------------- */
/** Met les 8 octets à 0 puis écrit chaque signal lié à partir de sa variable. */
void CanMsg_Pack(const sCanMessage *m, uint8_t *data);
/** Lit chaque signal lié et met à jour sa variable. */
void CanMsg_Unpack(const sCanMessage *m, const uint8_t *data);
/** Message de la table portant cet identifiant, NULL sinon. */
const sCanMessage *CanMsg_Find(const sCanMessage *table, uint8_t count, uint32_t id);
/** Code le message et l'émet (DLC explicite : certaines commandes sont plus courtes que le DLC DBC). */
void CanMsg_Send(const sCanMessage *m, uint8_t dlc, CAN_TX_PRIOIRTY priority);

#endif /* CAN_SIGNAL_H */
