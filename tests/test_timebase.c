/**
 * @file test_timebase.c
 * @brief Tests de la base de temps (src/timebase.c).
 */
#include "test_framework.h"
#include "timebase.h"

static void test_setup(void) { Timebase_Init(); }

static void ticks(int n)
{
    while (n--)
        Timebase_OnTick();
}

TEST(can_ready_a_chaque_tick)
{
    ASSERT_FALSE(timebase.can_ready);
    Timebase_OnTick();
    ASSERT_TRUE(timebase.can_ready);
    timebase.can_ready = false;
    Timebase_OnTick();
    ASSERT_TRUE(timebase.can_ready);
}

TEST(flag_100ms_au_dixieme_tick)
{
    ticks(9);
    ASSERT_FALSE(timebase.flag_100ms);
    ASSERT_EQ_INT(0, timebase.ticks_100ms);
    Timebase_OnTick();
    ASSERT_TRUE(timebase.flag_100ms);
    ASSERT_EQ_INT(1, timebase.ticks_100ms);
}

TEST(flag_300ms_tous_les_3_pas_de_100ms)
{
    ticks(20);
    ASSERT_FALSE(timebase.flag_300ms);
    ticks(10);
    ASSERT_TRUE(timebase.flag_300ms);
    timebase.flag_300ms = false;
    ticks(30);
    ASSERT_TRUE(timebase.flag_300ms);
}

TEST(flag_1s_tous_les_10_pas_de_100ms)
{
    ticks(90);
    ASSERT_FALSE(timebase.flag_1s);
    ticks(10);
    ASSERT_TRUE(timebase.flag_1s);
    ASSERT_EQ_INT(10, timebase.ticks_100ms);
}

TEST(ticks_100ms_compte_librement)
{
    ticks(1500);
    ASSERT_EQ_INT(150, timebase.ticks_100ms);
    timebase.ticks_100ms = 0;
    ticks(10);
    ASSERT_EQ_INT(1, timebase.ticks_100ms);
}

TEST_MAIN_BEGIN()
    RUN_TEST(can_ready_a_chaque_tick);
    RUN_TEST(flag_100ms_au_dixieme_tick);
    RUN_TEST(flag_300ms_tous_les_3_pas_de_100ms);
    RUN_TEST(flag_1s_tous_les_10_pas_de_100ms);
    RUN_TEST(ticks_100ms_compte_librement);
TEST_MAIN_END()
