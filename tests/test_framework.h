/**
 * @file test_framework.h
 * @brief Mini-cadre de tests unitaires sans dépendance externe.
 *
 * Usage :
 *   static void test_setup(void) { ... remise à zéro des modules et des mocks ... }
 *   TEST(mon_test) { ASSERT_EQ_INT(3, f(1, 2)); }
 *   TEST_MAIN_BEGIN()
 *       RUN_TEST(mon_test);
 *   TEST_MAIN_END()
 * Le programme renvoie 1 si au moins une assertion a échoué.
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

static int tf_tests_run = 0;
static int tf_tests_failed = 0;
static int tf_asserts = 0;
static int tf_current_failed = 0;

static void test_setup(void);

#define TEST(name) static void name(void)

#define RUN_TEST(name)                                   \
    do {                                                 \
        tf_current_failed = 0;                           \
        tf_tests_run++;                                  \
        test_setup();                                    \
        name();                                          \
        if (tf_current_failed) {                         \
            tf_tests_failed++;                           \
            printf("  ECHEC  %s\n", #name);              \
        }                                                \
    } while (0)

#define TF_FAIL(fmt, ...)                                                      \
    do {                                                                       \
        printf("%s:%d: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__);           \
        tf_current_failed = 1;                                                 \
    } while (0)

#define ASSERT_TRUE(cond)                                                      \
    do { tf_asserts++; if (!(cond)) TF_FAIL("ASSERT_TRUE(%s)", #cond); } while (0)

#define ASSERT_FALSE(cond)                                                     \
    do { tf_asserts++; if (cond) TF_FAIL("ASSERT_FALSE(%s)", #cond); } while (0)

#define ASSERT_EQ_INT(expected, actual)                                        \
    do {                                                                       \
        long _e = (long)(expected), _a = (long)(actual);                       \
        tf_asserts++;                                                          \
        if (_e != _a) TF_FAIL("ASSERT_EQ_INT(%s, %s) : attendu %ld, obtenu %ld", #expected, #actual, _e, _a); \
    } while (0)

#define ASSERT_EQ_UINT(expected, actual)                                       \
    do {                                                                       \
        unsigned long _e = (unsigned long)(expected), _a = (unsigned long)(actual); \
        tf_asserts++;                                                          \
        if (_e != _a) TF_FAIL("ASSERT_EQ_UINT(%s, %s) : attendu %lu, obtenu %lu", #expected, #actual, _e, _a); \
    } while (0)

#define ASSERT_EQ_HEX(expected, actual)                                        \
    do {                                                                       \
        unsigned long _e = (unsigned long)(expected), _a = (unsigned long)(actual); \
        tf_asserts++;                                                          \
        if (_e != _a) TF_FAIL("ASSERT_EQ_HEX(%s, %s) : attendu 0x%lX, obtenu 0x%lX", #expected, #actual, _e, _a); \
    } while (0)

#define ASSERT_FLOAT_WITHIN(delta, expected, actual)                           \
    do {                                                                       \
        double _d = (double)(delta), _e = (double)(expected), _a = (double)(actual); \
        tf_asserts++;                                                          \
        if (fabs(_e - _a) > _d) TF_FAIL("ASSERT_FLOAT_WITHIN(%s, %s, %s) : attendu %g, obtenu %g", #delta, #expected, #actual, _e, _a); \
    } while (0)

#define ASSERT_EQ_MEM(expected, actual, len)                                   \
    do {                                                                       \
        const uint8_t *_e = (const uint8_t *)(expected), *_a = (const uint8_t *)(actual); \
        size_t _n = (size_t)(len), _i;                                         \
        tf_asserts++;                                                          \
        for (_i = 0; _i < _n; _i++) {                                          \
            if (_e[_i] != _a[_i]) {                                            \
                TF_FAIL("ASSERT_EQ_MEM(%s, %s) : octet %u attendu 0x%02X, obtenu 0x%02X", #expected, #actual, (unsigned)_i, _e[_i], _a[_i]); \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    } while (0)

#define TEST_MAIN_BEGIN()                                                      \
    int main(void)                                                             \
    {                                                                          \
        printf("== %s ==\n", __FILE__);

#define TEST_MAIN_END()                                                        \
        printf("   %d test(s), %d assertion(s), %d echec(s)\n",               \
               tf_tests_run, tf_asserts, tf_tests_failed);                     \
        return tf_tests_failed ? 1 : 0;                                        \
    }

#endif /* TEST_FRAMEWORK_H */
