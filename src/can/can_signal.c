/**
 * @file can_signal.c
 * @brief Couche CAN générique (voir can_signal.h).
 *
 * Rôle dans l'architecture :
 *  - les modules métier (batteries, FCCB, maintenance, télémétrie...) décrivent leurs trames par des
 *    tables de liaisons « signal DBC <-> variable » (sCanBinding / sCanMessage) ;
 *  - ce fichier code et décode ces trames sans rien savoir de leur contenu :
 *      CanMsg_Pack / CanMsg_Send : émission (appelés par can_tx.c toutes les 10 ms environ,
 *                                  et ponctuellement par fcs.c et battery.c pour les commandes) ;
 *      CanMsg_Find / CanMsg_Unpack : réception (appelés par les gestionnaires de trames des modules,
 *                                  depuis CanRx_Process dans la boucle principale).
 *
 * Numérotation des bits (convention DBC, identique pour Intel et Motorola) :
 *      position = numéro d'octet × 8 + numéro de bit dans l'octet (bit 0 = poids faible).
 *      Exemple : le bit 3 de l'octet 2 a la position 19.
 *
 * Les signaux sont parcourus octet par octet (au plus 5 octets pour 32 bits) avec un
 * accumulateur 32 bits : pas d'arithmétique 64 bits, coûteuse sur dsPIC.
 */
#include <string.h>
#include "src/can/can_signal.h"
#include "hal/can_port.h"

/* Taille fixe du champ de données manipulé : les trames CAN 2.0 font au plus 8 octets. */
#define CAN_DATA_SIZE 8u

/* Masque des `length` bits de poids faible d'un mot 32 bits (1..32).
 * Le cas 32 est traité à part : décaler un uint32_t de 32 bits est indéfini en C. */
static uint32_t mask32(uint8_t length)
{
    return (length >= 32u) ? 0xFFFFFFFFUL : (((uint32_t)1 << length) - 1u);
}

/* Masque des `n` bits de poids faible d'un octet (n = 1..8). */
static uint8_t mask8(uint8_t n)
{
    return (uint8_t)((1u << n) - 1u);
}

/*
 * Lecture des bits bruts d'un signal.
 *
 * Intel (little-endian) : start_bit désigne le bit de POIDS FAIBLE du signal. Les bits suivants
 * montent dans l'octet puis continuent au bit 0 de l'octet suivant. Le parcours se fait donc du
 * poids faible vers le poids fort ; chaque morceau lu est placé à gauche de ce qui a déjà été lu
 * (décalage de `done` bits).
 *
 * Motorola (big-endian) : start_bit désigne le bit de POIDS FORT du signal. Les bits suivants
 * descendent dans l'octet puis continuent au bit 7 de l'octet suivant (numérotation « en dents de
 * scie »). Le parcours se fait du poids fort vers le poids faible : l'accumulateur est décalé à
 * gauche puis le nouveau morceau est ajouté à droite.
 *
 * Dans les deux cas, on traite à chaque tour le plus grand morceau contenu dans un seul octet
 * (n bits), ce qui limite la boucle à 5 itérations pour un signal de 32 bits.
 */
uint32_t CanSignal_GetRaw(const uint8_t *data, const sCanSignalLayout *s)
{
    uint32_t raw = 0;
    uint8_t pos = s->start_bit;     /* position (octet × 8 + bit) du prochain bit à lire */
    uint8_t remaining = s->length;  /* bits restant à lire */
    uint8_t done = 0;               /* bits déjà lus (Intel : rang où placer le morceau suivant) */
    uint8_t byte, bit, n;

    if ((s->flags & CAN_MOTOROLA) == 0u)
    {
        /* Intel : du bit de poids faible vers les octets suivants */
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            /* bits disponibles de `bit` jusqu'au bit 7 de l'octet, limités à ce qu'il reste */
            n = (uint8_t)(8u - bit);
            if (n > remaining)
                n = remaining;
            raw |= (uint32_t)((data[byte] >> bit) & mask8(n)) << done;
            done += n;
            remaining -= n;
            pos += n;               /* au besoin, passe au bit 0 de l'octet suivant */
        }
    }
    else
    {
        /* Motorola : du bit de poids fort, bits décroissants dans l'octet puis octet suivant */
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            /* bits disponibles de `bit` jusqu'au bit 0 de l'octet, limités à ce qu'il reste */
            n = (uint8_t)(bit + 1u);
            if (n > remaining)
                n = remaining;
            /* morceau = bits [bit .. bit-n+1], aligné à droite puis ajouté sous l'accumulateur */
            raw = (raw << n) | ((data[byte] >> (bit + 1u - n)) & mask8(n));
            remaining -= n;
            pos = (uint8_t)((byte + 1u) * 8u + 7u); /* suite au bit 7 (poids fort) de l'octet suivant */
        }
    }
    return raw;
}

/*
 * Valeur brute entière. Pour un signal signé de longueur L < 32, le bit L-1 est le bit de signe :
 * s'il vaut 1, tous les bits au-dessus sont mis à 1 (extension de signe en complément à deux).
 * Exemple : 4 bits 0b1011 -> 0xFFFFFFFB = -5. À 32 bits, le mot est déjà complet.
 */
int32_t CanSignal_GetInt(const uint8_t *data, const sCanSignalLayout *s)
{
    uint32_t raw = CanSignal_GetRaw(data, s);

    if ((s->flags & CAN_SIGNED) && (s->length < 32u) && (raw & ((uint32_t)1 << (s->length - 1u))))
        raw |= ~mask32(s->length);
    return (int32_t)raw;
}

/*
 * Signal flottant IEEE 754 32 bits (SIG_VALTYPE_ 1 dans le .dbc) : les 32 bits bruts sont
 * réinterprétés tels quels en float via une union (pas de conversion numérique).
 * Le float de XC16 et celui de gcc sur PC sont tous deux au format IEEE 754 simple précision.
 */
float CanSignal_GetFloat(const uint8_t *data, const sCanSignalLayout *s)
{
    union { uint32_t u; float f; } v;
    v.u = CanSignal_GetRaw(data, s);
    return v.f;
}

/*
 * Écriture des bits d'un signal : même parcours que CanSignal_GetRaw (voir ci-dessus).
 * À chaque morceau, les bits concernés de l'octet sont effacés (masque m) puis remplacés ;
 * les autres bits de l'octet, qui appartiennent à d'autres signaux, sont conservés.
 * La valeur est d'abord tronquée à la longueur du signal (un -1000 sur 16 bits donne 0xFC18).
 */
void CanSignal_SetRaw(uint8_t *data, const sCanSignalLayout *s, uint32_t raw)
{
    uint8_t pos = s->start_bit;     /* position du prochain bit à écrire */
    uint8_t remaining = s->length;  /* bits restant à écrire */
    uint8_t done = 0;               /* Intel : bits de `raw` déjà écrits (en partant du poids faible) */
    uint8_t byte, bit, n, m;

    raw &= mask32(s->length);

    if ((s->flags & CAN_MOTOROLA) == 0u)
    {
        /* Intel : on écrit les bits de `raw` du poids faible vers le poids fort */
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            n = (uint8_t)(8u - bit);
            if (n > remaining)
                n = remaining;
            m = (uint8_t)(mask8(n) << bit);                 /* bits de l'octet occupés par le morceau */
            data[byte] = (uint8_t)((data[byte] & (uint8_t)~m) | ((uint8_t)((raw >> done) & mask8(n)) << bit));
            done += n;
            remaining -= n;
            pos += n;
        }
    }
    else
    {
        /* Motorola : on écrit les bits de `raw` du poids fort vers le poids faible ; le morceau
         * courant correspond aux `n` bits de rang (remaining-1 .. remaining-n) de `raw`. */
        while (remaining > 0u)
        {
            byte = pos >> 3;
            bit = pos & 7u;
            n = (uint8_t)(bit + 1u);
            if (n > remaining)
                n = remaining;
            m = (uint8_t)(mask8(n) << (bit + 1u - n));      /* bits [bit .. bit-n+1] de l'octet */
            data[byte] = (uint8_t)((data[byte] & (uint8_t)~m) |
                                   ((uint8_t)((raw >> (remaining - n)) & mask8(n)) << (bit + 1u - n)));
            remaining -= n;
            pos = (uint8_t)((byte + 1u) * 8u + 7u);
        }
    }
}

/* Écriture d'un signal flottant 32 bits : réinterprétation inverse de CanSignal_GetFloat. */
void CanSignal_SetFloat(uint8_t *data, const sCanSignalLayout *s, float value)
{
    union { uint32_t u; float f; } v;
    v.f = value;
    CanSignal_SetRaw(data, s, v.u);
}

/* ---- Liaisons ------------------------------------------------------------------------------ */
/*
 * Une liaison (sCanBinding) associe un signal à une variable du firmware de type quelconque.
 * Les valeurs transitent toutes par un int32_t, suffisant pour tous les types liés
 * (bool, 8, 16 et 32 bits) ; les deux fonctions ci-dessous font la lecture / l'écriture typée.
 */

/* Lit la variable liée et la renvoie en int32_t (bool -> 0 ou 1). */
static int32_t read_var(const sCanBinding *b)
{
    switch (b->var_type)
    {
    case CAN_VAR_BOOL: return *(const bool *)b->var ? 1 : 0;
    case CAN_VAR_U8:   return *(const uint8_t *)b->var;
    case CAN_VAR_I8:   return *(const int8_t *)b->var;
    case CAN_VAR_U16:  return *(const uint16_t *)b->var;
    case CAN_VAR_I16:  return *(const int16_t *)b->var;
    case CAN_VAR_U32:  return (int32_t)*(const uint32_t *)b->var;   /* motif binaire conservé */
    case CAN_VAR_I32:  return *(const int32_t *)b->var;
    default:           return 0;
    }
}

/* Écrit une valeur dans la variable liée, tronquée à son type C (comme une affectation C). */
static void write_var(const sCanBinding *b, int32_t v)
{
    switch (b->var_type)
    {
    case CAN_VAR_BOOL: *(bool *)b->var = (v != 0); break;
    case CAN_VAR_U8:   *(uint8_t *)b->var = (uint8_t)v; break;
    case CAN_VAR_I8:   *(int8_t *)b->var = (int8_t)v; break;
    case CAN_VAR_U16:  *(uint16_t *)b->var = (uint16_t)v; break;
    case CAN_VAR_I16:  *(int16_t *)b->var = (int16_t)v; break;
    case CAN_VAR_U32:  *(uint32_t *)b->var = (uint32_t)v; break;
    case CAN_VAR_I32:  *(int32_t *)b->var = v; break;
    default: break;
    }
}

/*
 * Codage d'une trame entière à partir des variables liées.
 *
 * Conversion variable -> brut, inverse de celle de la réception :
 *      var = brut × mul / div + offset   donc   brut = (var - offset) × div / mul
 * mul, div et offset sont des entiers précalculés à la compilation par CAN_BIND() à partir du
 * facteur DBC et de l'unité de la variable (voir can_signal.h). Exemples :
 *  - unité de la variable = unité DBC (cas le plus courant) : mul = div = 1, brut = var ;
 *  - température brute ADC émise en centaines de points (facteur 100) : mul = 100, brut = var / 100 ;
 *  - consigne en 0,01 A émise en A (facteur 1, unité 0.01) : mul = 100, brut = var / 100.
 * Les divisions entières tronquent vers zéro, comme le faisait le codage écrit à la main.
 * Les liaisons NONE laissent leurs bits à 0 ; les liaisons CONST émettent la valeur brute rangée
 * dans le champ offset.
 */
void CanMsg_Pack(const sCanMessage *m, uint8_t *data)
{
    uint8_t i;
    const sCanBinding *b;
    int32_t v;

    memset(data, 0, CAN_DATA_SIZE);     /* octets non couverts par un signal : 0 */
    for (i = 0; i < m->nb_bindings; i++)
    {
        b = &m->bindings[i];
        if (b->var_type == CAN_VAR_NONE)
            continue;
        if (b->var_type == CAN_VAR_CONST)
        {
            CanSignal_SetRaw(data, &b->layout, (uint32_t)(int32_t)b->offset);
            continue;
        }
        v = read_var(b) - b->offset;
        if (b->layout.flags & CAN_FLOAT32)
            CanSignal_SetFloat(data, &b->layout, (float)v * b->div / b->mul);
        else if ((b->mul == 1) && (b->div == 1))
            CanSignal_SetRaw(data, &b->layout, (uint32_t)v);   /* chemin rapide : aucune conversion */
        else
            CanSignal_SetRaw(data, &b->layout, (uint32_t)(v * b->div / b->mul));
    }
}

/*
 * Décodage d'une trame entière vers les variables liées : var = brut × mul / div + offset.
 *
 *  - signal entier : brut lu avec extension de signe si besoin, puis conversion entière
 *    (ex. SOC IBS en 0,01 % vers une variable en % : div = 100 ; température IBS en 0,1 °C : div = 10) ;
 *  - signal flottant : le float est multiplié puis tronqué vers zéro
 *    (ex. courant IBS en A, signe inversé, vers une variable en 0,01 A : mul = -100) ;
 *  - bit inversé (error_flag IBS, facteur -1 offset 1) : var = 1 - brut.
 * Les liaisons sans variable (NONE, CONST) sont ignorées.
 */
void CanMsg_Unpack(const sCanMessage *m, const uint8_t *data)
{
    uint8_t i;
    const sCanBinding *b;
    int32_t v;

    for (i = 0; i < m->nb_bindings; i++)
    {
        b = &m->bindings[i];
        if (b->var == NULL)
            continue;
        if (b->layout.flags & CAN_FLOAT32)
            v = (int32_t)(CanSignal_GetFloat(data, &b->layout) * b->mul / b->div); /* troncature vers 0 */
        else
        {
            v = CanSignal_GetInt(data, &b->layout);
            if ((b->mul != 1) || (b->div != 1))
                v = v * b->mul / b->div;
        }
        write_var(b, v + b->offset);
    }
}

/* Recherche linéaire d'un message par identifiant dans une table (au plus une douzaine
 * d'entrées par module : plus simple et plus compact qu'un tri ou un hachage). */
const sCanMessage *CanMsg_Find(const sCanMessage *table, uint8_t count, uint32_t id)
{
    uint8_t i;
    for (i = 0; i < count; i++)
        if (table[i].id == id)
            return &table[i];
    return NULL;
}

/*
 * Code le message puis le dépose dans une boîte d'émission CAN1.
 * Le buffer et l'objet message sont statiques et partagés par tous les émetteurs : c'est sans
 * risque car toutes les émissions se font depuis la boucle principale (jamais sous interruption)
 * et le driver MCC recopie les données dans le buffer matériel avant de rendre la main.
 * Le DLC est un paramètre car certaines commandes FCCB sont émises plus courtes que le DLC DBC.
 */
void CanMsg_Send(const sCanMessage *m, uint8_t dlc, CAN_TX_PRIOIRTY priority)
{
    static uint8_t tx_data[CAN_DATA_SIZE];
    static CAN_MSG_OBJ tx_msg;

    CanMsg_Pack(m, tx_data);
    tx_msg.msgId = m->id;
    tx_msg.field.msgfields = 0;
    tx_msg.field.idType = m->extended ? CAN_FRAME_EXT : CAN_FRAME_STD;
    tx_msg.field.frameType = CAN_FRAME_DATA;
    tx_msg.field.dlc = dlc;
    tx_msg.data = tx_data;
    CanPort_Transmit(priority, &tx_msg);
}
