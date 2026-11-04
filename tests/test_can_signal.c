/**
 * @file test_can_signal.c
 * @brief Tests de la couche CAN générique (src/can_signal.c) et de la base générée (src/can_db.h).
 */
#define CANDB_WITH_SIGNAL_LIST
#include <stdlib.h>
#include "test_framework.h"
#include "can_signal.h"
#include "can_db.h"
#include "hal/can_port.h"

static void test_setup(void) { MockCan_Reset(); }

/* ---- Implémentation de référence, bit par bit, d'après la définition DBC ---------------- */
static int ref_positions(const sCanSignalLayout *s, int *pos) /* du MSB au LSB */
{
    int i, p = s->start_bit;
    for (i = 0; i < s->length; i++)
    {
        if (s->flags & CAN_MOTOROLA)
        {
            pos[i] = p;
            p = (p % 8 == 0) ? p + 15 : p - 1;
        }
        else
            pos[i] = s->start_bit + s->length - 1 - i;
    }
    return s->length;
}

static uint32_t ref_get(const uint8_t *d, const sCanSignalLayout *s)
{
    int pos[32], n = ref_positions(s, pos), i;
    uint32_t v = 0;
    for (i = 0; i < n; i++)
        v = (v << 1) | ((d[pos[i] / 8] >> (pos[i] % 8)) & 1u);
    return v;
}

static void ref_set(uint8_t *d, const sCanSignalLayout *s, uint32_t v)
{
    int pos[32], n = ref_positions(s, pos), i;
    for (i = 0; i < n; i++)
    {
        uint8_t bit = (uint8_t)((v >> (n - 1 - i)) & 1u);
        d[pos[i] / 8] = (uint8_t)((d[pos[i] / 8] & ~(1u << (pos[i] % 8))) | (bit << (pos[i] % 8)));
    }
}

static uint32_t rnd32(void)
{
    return ((uint32_t)(rand() & 0xFFFF) << 16) ^ (uint32_t)(rand() & 0xFFFF);
}

/* ---- Vecteurs connus ---------------------------------------------------------------------- */
TEST(intel_octet_et_mot_16_bits)
{
    const uint8_t d[8] = {0x2A, 0, 0, 0, 0, 0, 0x34, 0x12};
    const sCanSignalLayout b0 = CAN_LAYOUT(0, 8, CAN_INTEL | CAN_UNSIGNED);
    const sCanSignalLayout w = CAN_LAYOUT(48, 16, CAN_INTEL | CAN_UNSIGNED);
    ASSERT_EQ_HEX(0x2A, CanSignal_GetRaw(d, &b0));
    ASSERT_EQ_HEX(0x1234, CanSignal_GetRaw(d, &w));
}

TEST(intel_a_cheval_sur_deux_octets)
{
    const uint8_t d[8] = {0xAB, 0xCD};
    const sCanSignalLayout s = CAN_LAYOUT(4, 12, CAN_INTEL | CAN_UNSIGNED);
    ASSERT_EQ_HEX(0xCDA, CanSignal_GetRaw(d, &s));
}

TEST(intel_signe)
{
    const uint8_t d[8] = {0x18, 0xFC, 0xFB};
    const sCanSignalLayout s16 = CAN_LAYOUT(0, 16, CAN_INTEL | CAN_SIGNED);
    const sCanSignalLayout s8 = CAN_LAYOUT(16, 8, CAN_INTEL | CAN_SIGNED);
    const sCanSignalLayout s4 = CAN_LAYOUT(16, 4, CAN_INTEL | CAN_SIGNED); /* 0xB sur 4 bits = -5 */
    ASSERT_EQ_INT(-1000, CanSignal_GetInt(d, &s16));
    ASSERT_EQ_INT(-5, CanSignal_GetInt(d, &s8));
    ASSERT_EQ_INT(-5, CanSignal_GetInt(d, &s4));
    ASSERT_EQ_HEX(0xFC18, CanSignal_GetRaw(d, &s16)); /* les bits bruts restent non signés */
}

TEST(intel_booleens)
{
    const uint8_t d[8] = {0, 0x05};
    const sCanSignalLayout b8 = CAN_LAYOUT(8, 1, CAN_INTEL | CAN_UNSIGNED);
    const sCanSignalLayout b9 = CAN_LAYOUT(9, 1, CAN_INTEL | CAN_UNSIGNED);
    const sCanSignalLayout b10 = CAN_LAYOUT(10, 1, CAN_INTEL | CAN_UNSIGNED);
    ASSERT_EQ_INT(1, CanSignal_GetRaw(d, &b8));
    ASSERT_EQ_INT(0, CanSignal_GetRaw(d, &b9));
    ASSERT_EQ_INT(1, CanSignal_GetRaw(d, &b10));
}

TEST(motorola_mots_16_bits)
{
    const uint8_t d[8] = {0x27, 0x10, 0x01, 0x90, 0xFF, 0x38};
    const sCanSignalLayout w0 = CAN_LAYOUT(7, 16, CAN_MOTOROLA | CAN_UNSIGNED);
    const sCanSignalLayout w1 = CAN_LAYOUT(23, 16, CAN_MOTOROLA | CAN_SIGNED);
    const sCanSignalLayout w2 = CAN_LAYOUT(39, 16, CAN_MOTOROLA | CAN_SIGNED);
    ASSERT_EQ_INT(10000, CanSignal_GetRaw(d, &w0));
    ASSERT_EQ_INT(400, CanSignal_GetInt(d, &w1));
    ASSERT_EQ_INT(-200, CanSignal_GetInt(d, &w2));
}

TEST(motorola_partiel)
{
    /* 12 bits, MSB = octet 0 bit 3 : bits 3..0 de l'octet 0 puis octet 1 */
    const uint8_t d[8] = {0xA5, 0x3C};
    const sCanSignalLayout s = CAN_LAYOUT(3, 12, CAN_MOTOROLA | CAN_UNSIGNED);
    ASSERT_EQ_HEX(0x53C, CanSignal_GetRaw(d, &s));
}

TEST(motorola_float32)
{
    const uint8_t d[8] = {0xBF, 0x80, 0x00, 0x00, 0x41, 0x24, 0x00, 0x00}; /* -1.0 et 10.25 */
    const sCanSignalLayout f0 = CAN_LAYOUT(7, 32, CAN_MOTOROLA | CAN_FLOAT32);
    const sCanSignalLayout f1 = CAN_LAYOUT(39, 32, CAN_MOTOROLA | CAN_FLOAT32);
    ASSERT_FLOAT_WITHIN(0.0, -1.0, CanSignal_GetFloat(d, &f0));
    ASSERT_FLOAT_WITHIN(0.0, 10.25, CanSignal_GetFloat(d, &f1));
}

TEST(set_ne_modifie_que_ses_bits)
{
    uint8_t d[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const sCanSignalLayout s = CAN_LAYOUT(4, 12, CAN_INTEL | CAN_UNSIGNED);
    const sCanSignalLayout m = CAN_LAYOUT(43, 6, CAN_MOTOROLA | CAN_UNSIGNED); /* octet 5 bits 3..0, octet 6 bits 7..6 */
    CanSignal_SetRaw(d, &s, 0x000);
    ASSERT_EQ_HEX(0x0F, d[0]);
    ASSERT_EQ_HEX(0x00, d[1]);
    ASSERT_EQ_HEX(0xFF, d[2]);
    CanSignal_SetRaw(d, &m, 0x00);
    ASSERT_EQ_HEX(0xF0, d[5]);
    ASSERT_EQ_HEX(0x3F, d[6]);
    ASSERT_EQ_HEX(0xFF, d[4]);
    ASSERT_EQ_HEX(0xFF, d[7]);
}

TEST(set_masque_les_valeurs_hors_plage)
{
    uint8_t d[8] = {0};
    const sCanSignalLayout s = CAN_LAYOUT(8, 4, CAN_INTEL | CAN_UNSIGNED);
    const sCanSignalLayout s16 = CAN_LAYOUT(16, 16, CAN_INTEL | CAN_SIGNED);
    CanSignal_SetRaw(d, &s, 0xFFF3);
    ASSERT_EQ_HEX(0x03, d[1]);
    ASSERT_EQ_HEX(0x00, d[0]);
    CanSignal_SetRaw(d, &s16, (uint32_t)(int32_t)-1000);
    ASSERT_EQ_HEX(0x18, d[2]);
    ASSERT_EQ_HEX(0xFC, d[3]);
}

TEST(signal_32_bits_intel)
{
    uint8_t d[8] = {0};
    const sCanSignalLayout s = CAN_LAYOUT(32, 32, CAN_INTEL | CAN_UNSIGNED);
    CanSignal_SetRaw(d, &s, 0xDEADBEEFUL);
    ASSERT_EQ_HEX(0xEF, d[4]);
    ASSERT_EQ_HEX(0xDE, d[7]);
    ASSERT_EQ_HEX(0xDEADBEEFUL, CanSignal_GetRaw(d, &s));
}

/* ---- Comparaison à la référence sur toute la base ----------------------------------------- */
TEST(tous_les_signaux_de_la_base_lecture_conforme_a_la_reference)
{
    size_t i;
    int k, j;
    uint8_t d[8];
    srand(1234);
    for (i = 0; i < CANDB_SIGNAL_LIST_COUNT; i++)
    {
        for (k = 0; k < 50; k++)
        {
            for (j = 0; j < 8; j++)
                d[j] = (uint8_t)rand();
            if (CanSignal_GetRaw(d, &candb_signal_list[i].layout) != ref_get(d, &candb_signal_list[i].layout))
            {
                ASSERT_TRUE(0 && "GetRaw differe de la reference");
                printf("    signal %s.%s\n", candb_signal_list[i].message, candb_signal_list[i].signal);
                return;
            }
        }
    }
    ASSERT_TRUE(CANDB_SIGNAL_LIST_COUNT > 300);
}

TEST(tous_les_signaux_de_la_base_ecriture_conforme_a_la_reference)
{
    size_t i;
    int k, j;
    uint8_t a[8], b[8];
    uint32_t v;
    srand(4321);
    for (i = 0; i < CANDB_SIGNAL_LIST_COUNT; i++)
    {
        const sCanSignalLayout *s = &candb_signal_list[i].layout;
        for (k = 0; k < 50; k++)
        {
            for (j = 0; j < 8; j++)
                a[j] = b[j] = (uint8_t)rand();
            v = rnd32();
            CanSignal_SetRaw(a, s, v);
            ref_set(b, s, s->length >= 32 ? v : (v & ((1UL << s->length) - 1u)));
            if (memcmp(a, b, 8) != 0 || CanSignal_GetRaw(a, s) != ref_get(b, s))
            {
                ASSERT_TRUE(0 && "SetRaw differe de la reference");
                printf("    signal %s.%s\n", candb_signal_list[i].message, candb_signal_list[i].signal);
                return;
            }
        }
    }
}

TEST(tous_les_signaux_tiennent_dans_leur_dlc)
{
    size_t i;
    int pos[32], n, k;
    for (i = 0; i < CANDB_SIGNAL_LIST_COUNT; i++)
    {
        n = ref_positions(&candb_signal_list[i].layout, pos);
        for (k = 0; k < n; k++)
            if (pos[k] < 0 || pos[k] >= 8 * candb_signal_list[i].dlc)
            {
                ASSERT_TRUE(0 && "signal hors DLC");
                printf("    signal %s.%s\n", candb_signal_list[i].message, candb_signal_list[i].signal);
                return;
            }
    }
}

/* ---- Liaisons et conversions ---------------------------------------------------------------- */
/* Signaux de test, au format des macros générées */
#define T__U8_LAYOUT    CAN_LAYOUT(0u, 8u, CAN_INTEL | CAN_UNSIGNED)
#define T__U8_FACTOR    1.0
#define T__U8_OFFSET    0.0
#define T__CENT_LAYOUT  CAN_LAYOUT(8u, 16u, CAN_INTEL | CAN_UNSIGNED)  /* 0,01 % */
#define T__CENT_FACTOR  0.01
#define T__CENT_OFFSET  0.0
#define T__HUND_LAYOUT  CAN_LAYOUT(24u, 8u, CAN_INTEL | CAN_UNSIGNED)  /* 100 points */
#define T__HUND_FACTOR  100.0
#define T__HUND_OFFSET  0.0
#define T__INV_LAYOUT   CAN_LAYOUT(32u, 1u, CAN_INTEL | CAN_UNSIGNED)  /* bit inversé */
#define T__INV_FACTOR   -1.0
#define T__INV_OFFSET   1.0
#define T__S16_LAYOUT   CAN_LAYOUT(40u, 16u, CAN_INTEL | CAN_SIGNED)   /* 0,1 °C */
#define T__S16_FACTOR   0.1
#define T__S16_OFFSET   0.0
#define T__K_LAYOUT     CAN_LAYOUT(56u, 8u, CAN_INTEL | CAN_UNSIGNED)
#define T__K_FACTOR     1.0
#define T__K_OFFSET     0.0
#define TF__F_LAYOUT    CAN_LAYOUT(7u, 32u, CAN_MOTOROLA | CAN_FLOAT32)
#define TF__F_FACTOR    -1.0
#define TF__F_OFFSET    0.0
#define TF__N_LAYOUT    CAN_LAYOUT(32u, 8u, CAN_INTEL | CAN_UNSIGNED)
#define TF__N_FACTOR    1.0
#define TF__N_OFFSET    0.0

static uint8_t v_u8;
static uint8_t v_percent;  /* 1 % */
static uint16_t v_points;  /* 1 point */
static bool v_error;
static int8_t v_degc;      /* 1 °C */
static int16_t v_current;  /* 0,01 A */

static const sCanBinding t_bindings[] CAN_ROM = {
    CAN_BIND(T__U8, &v_u8, CAN_VAR_U8, 1),
    CAN_BIND(T__CENT, &v_percent, CAN_VAR_U8, 1),
    CAN_BIND(T__HUND, &v_points, CAN_VAR_U16, 1),
    CAN_BIND(T__INV, &v_error, CAN_VAR_BOOL, 1),
    CAN_BIND(T__S16, &v_degc, CAN_VAR_I8, 1),
    CAN_BIND_CONST(T__K, 0xA5),
};
static const sCanBinding tf_bindings[] CAN_ROM = {
    CAN_BIND(TF__F, &v_current, CAN_VAR_I16, 0.01),
    CAN_BIND_NONE(TF__N),
};
static const sCanMessage t_msg CAN_ROM = {0x123, 1, 8, CAN_ARRAY_SIZE(t_bindings), t_bindings};
static const sCanMessage tf_msg CAN_ROM = {0x456, 0, 8, CAN_ARRAY_SIZE(tf_bindings), tf_bindings};

TEST(conversions_precalculees)
{
    ASSERT_EQ_INT(1, t_bindings[0].mul);   ASSERT_EQ_INT(1, t_bindings[0].div);
    ASSERT_EQ_INT(1, t_bindings[1].mul);   ASSERT_EQ_INT(100, t_bindings[1].div);
    ASSERT_EQ_INT(100, t_bindings[2].mul); ASSERT_EQ_INT(1, t_bindings[2].div);
    ASSERT_EQ_INT(-1, t_bindings[3].mul);  ASSERT_EQ_INT(1, t_bindings[3].div); ASSERT_EQ_INT(1, t_bindings[3].offset);
    ASSERT_EQ_INT(1, t_bindings[4].mul);   ASSERT_EQ_INT(10, t_bindings[4].div);
    ASSERT_EQ_INT(-100, tf_bindings[0].mul); ASSERT_EQ_INT(1, tf_bindings[0].div);
}

TEST(unpack_conversions)
{
    /* u8 = 42 ; cent = 9999 (99,99 %) ; hund = 7 ; inv = 0 (erreur) ; s16 = -205 (-20,5 °C) */
    const uint8_t d[8] = {42, 0x0F, 0x27, 7, 0x00, 0x33, 0xFF, 0x55};
    CanMsg_Unpack(&t_msg, d);
    ASSERT_EQ_INT(42, v_u8);
    ASSERT_EQ_INT(99, v_percent);      /* troncature, comme une division entière */
    ASSERT_EQ_INT(700, v_points);
    ASSERT_TRUE(v_error);
    ASSERT_EQ_INT(-20, v_degc);        /* troncature vers zéro */
}

TEST(unpack_division_entiere_exacte)
{
    /* 5000 x 0,01 en flottant donnerait 49,99... : le précalcul entier donne bien 50 */
    uint8_t d[8] = {0, 0x88, 0x13, 0, 0x01};
    CanMsg_Unpack(&t_msg, d);
    ASSERT_EQ_INT(50, v_percent);
    ASSERT_FALSE(v_error);
}

TEST(pack_conversions_et_constante)
{
    uint8_t d[8];
    v_u8 = 200;
    v_percent = 55;     /* -> 5500 */
    v_points = 2050;    /* -> 20 (troncature) */
    v_error = true;     /* -> bit 0 */
    v_degc = -3;        /* -> -30 */
    memset(d, 0xEE, 8);
    CanMsg_Pack(&t_msg, d);
    ASSERT_EQ_INT(200, d[0]);
    ASSERT_EQ_HEX(0x157C, d[1] | (d[2] << 8));
    ASSERT_EQ_INT(20, d[3]);
    ASSERT_EQ_INT(0, d[4]);
    ASSERT_EQ_HEX(0xFFE2, d[5] | (d[6] << 8));
    ASSERT_EQ_HEX(0xA5, d[7]);
    v_error = false;
    CanMsg_Pack(&t_msg, d);
    ASSERT_EQ_INT(1, d[4]);
}

TEST(flottant_signe_inverse_et_signal_ignore)
{
    uint8_t d[8] = {0xC1, 0x24, 0x00, 0x00, 0x99}; /* -10.25 A -> +1025 (0,01 A) */
    v_current = 0;
    CanMsg_Unpack(&tf_msg, d);
    ASSERT_EQ_INT(1025, v_current);
    d[0] = 0x41; /* +10.25 -> -1025 */
    CanMsg_Unpack(&tf_msg, d);
    ASSERT_EQ_INT(-1025, v_current);
    /* mantisse complète : -10.2549996 A -> 1025 (troncature) ; 0x4124_1062 = 10.254 */
    d[0] = 0xC1; d[1] = 0x24; d[2] = 0x10; d[3] = 0x62;
    CanMsg_Unpack(&tf_msg, d);
    ASSERT_EQ_INT(1025, v_current);
    /* pack : le signal NONE reste à 0, le flottant est recodé */
    v_current = 150;
    CanMsg_Pack(&tf_msg, d);
    ASSERT_EQ_HEX(0xBF, d[0]); ASSERT_EQ_HEX(0xC0, d[1]); ASSERT_EQ_HEX(0x00, d[2]);
    ASSERT_EQ_INT(0, d[4]);
}

TEST(find_et_send)
{
    static const sCanMessage table[2] = {{0x10, 1, 2, 0, NULL}, {0x20, 0, 3, 0, NULL}};
    ASSERT_TRUE(CanMsg_Find(table, 2, 0x20) == &table[1]);
    ASSERT_TRUE(CanMsg_Find(table, 2, 0x30) == NULL);
    v_u8 = 9;
    CanMsg_Send(&t_msg, 3, CAN_PRIORITY_HIGH);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_HEX(0x123, MockCan_LastTx()->id);
    ASSERT_EQ_INT(3, MockCan_LastTx()->dlc);
    ASSERT_EQ_INT(CAN_FRAME_EXT, MockCan_LastTx()->idType);
    ASSERT_EQ_INT(CAN_PRIORITY_HIGH, MockCan_LastTx()->priority);
    ASSERT_EQ_INT(9, MockCan_LastTx()->data[0]);
    CanMsg_Send(&tf_msg, 8, CAN_PRIORITY_LOW);
    ASSERT_EQ_INT(CAN_FRAME_STD, MockCan_LastTx()->idType);
}

TEST(base_generee_identifiants_et_dlc)
{
    ASSERT_EQ_HEX(0x2C00000, CANDB_SMB_TX_GENERAL_ID);
    ASSERT_EQ_INT(8, CANDB_SMB_TX_GENERAL_DLC);
    ASSERT_EQ_INT(1, CANDB_SMB_TX_GENERAL_EXTENDED);
    ASSERT_EQ_HEX(0x18FF02D1, CANDB_IBS_CURRENT_1_ID);
    ASSERT_EQ_INT(30, CANDB_SMB_TX_GENERAL__STEP__STANDBY);
    ASSERT_EQ_INT(250, CANDB_FCCB_TX_GENERAL__STEP__EMERGENCY);
}

TEST_MAIN_BEGIN()
    RUN_TEST(intel_octet_et_mot_16_bits);
    RUN_TEST(intel_a_cheval_sur_deux_octets);
    RUN_TEST(intel_signe);
    RUN_TEST(intel_booleens);
    RUN_TEST(motorola_mots_16_bits);
    RUN_TEST(motorola_partiel);
    RUN_TEST(motorola_float32);
    RUN_TEST(set_ne_modifie_que_ses_bits);
    RUN_TEST(set_masque_les_valeurs_hors_plage);
    RUN_TEST(signal_32_bits_intel);
    RUN_TEST(tous_les_signaux_de_la_base_lecture_conforme_a_la_reference);
    RUN_TEST(tous_les_signaux_de_la_base_ecriture_conforme_a_la_reference);
    RUN_TEST(tous_les_signaux_tiennent_dans_leur_dlc);
    RUN_TEST(conversions_precalculees);
    RUN_TEST(unpack_conversions);
    RUN_TEST(unpack_division_entiere_exacte);
    RUN_TEST(pack_conversions_et_constante);
    RUN_TEST(flottant_signe_inverse_et_signal_ignore);
    RUN_TEST(find_et_send);
    RUN_TEST(base_generee_identifiants_et_dlc);
TEST_MAIN_END()
