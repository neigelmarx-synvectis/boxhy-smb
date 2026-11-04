/**
 * @file generic.h
 * @brief Fonctions génériques réutilisées dans le firmware : purement logicielles,
 *        sans accès matériel, testables sur PC.
 *
 * Briques utilisées par plusieurs modules :
 *  - arithmétique flottante (min, max, saturation, limiteur de pente, régulateur PI) : cooling.c, ems.c,
 *    battery.c ;
 *  - conversion ASCII -> chiffre : décodage des réponses RS232 de l'onduleur (inverter.c) ;
 *  - anti-rebond et détection d'appui long : boutons de l'IHM (hmi.c) ;
 *  - drapeau d'erreur temporisé : watchdogs et supervisions (battery.c, fcs.c, hmi.c) ;
 *  - moyenne par blocs : courants moyens (ems.c) ;
 *  - motif de double bip : avertissements sonores (hmi.c).
 * Toutes ces fonctions sont appelées depuis la boucle principale, jamais en interruption.
 */
#ifndef GENERIC_H
#define GENERIC_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Arithmétique ---------------------------------------------------------- */
/** Minimum de deux flottants. */
float Generic_MinF(float a, float b);
/** Maximum de deux flottants (aussi utilisé pour des entiers, convertis en float). */
float Generic_MaxF(float a, float b);
/** Borne value dans [min, max]. */
float Generic_Saturation(float min, float value, float max);
/** Rapproche limitedValue de setPoint d'au plus ramp par appel. */
float Generic_DynamicsLimiter(float limitedValue, float setPoint, float ramp);
/**
 * Régulateur PI (Kd inutilisé) avec saturation de l'intégrale et sortie 0..100.
 * Le signe est celui d'un refroidissement : la sortie augmente quand la mesure dépasse la consigne.
 * L'intégrale est bornée à [-20, 100] / Ki pour limiter l'emballement (anti-windup).
 * @param int_part terme intégral conservé entre les appels
 * @return commande en %, tronquée à l'entier inférieur
 */
uint8_t Generic_PidController(float Kp, float Ki, float Kd, float *int_part, float target, float measurement);
/** Convertit un caractère ASCII '0'..'9' en valeur 0..9 ; 0x0F sinon. */
uint8_t Generic_CharToDec(char input_char);

/* ---- Anti-rebond ------------------------------------------------------------ */
/** État d'un filtre anti-rebond ou d'un détecteur d'appui maintenu. */
typedef struct
{
    bool state;   /**< état filtré */
    uint8_t cnt;  /**< nombre d'échantillons consécutifs différents de l'état */
} sDebounce;

/**
 * Anti-rebond symétrique : l'état bascule quand l'entrée brute diffère de
 * l'état pendant plus de threshold échantillons consécutifs.
 * @return true si l'état a basculé lors de cet appel
 */
bool Debounce_Update(sDebounce *d, bool raw, uint8_t threshold);

/**
 * Détection d'appui maintenu : state passe à 1 quand raw est vrai pendant plus
 * de threshold échantillons consécutifs. state n'est jamais remis à 0 ici
 * (c'est le consommateur qui l'acquitte).
 * @return true si state a été armé lors de cet appel
 */
bool HoldDetect_Update(sDebounce *d, bool raw, uint8_t threshold);

/* ---- Drapeau d'erreur temporisé ---------------------------------------------- */
/**
 * Compteur 8 bits : incrémenté tant que la condition est vraie, remis à zéro sinon ;
 * l'erreur est verrouillée (et le compteur remis à zéro) quand cnt > timeout.
 * Le compteur 8 bits boucle au-delà de 255 (comportement d'origine conservé).
 * L'erreur n'est jamais effacée par la mise à jour : c'est le module propriétaire qui l'acquitte.
 */
typedef struct
{
    uint8_t cnt;  /**< durée écoulée avec la condition vraie, en appels */
    bool error;   /**< erreur verrouillée */
} sTimedFlag8;

/** Variante 16 bits de sTimedFlag8, pour les temporisations supérieures à 255 appels. */
typedef struct
{
    uint16_t cnt;
    bool error;
} sTimedFlag16;

/** @return l'état (verrouillé) de l'erreur après mise à jour */
bool TimedFlag8_Update(sTimedFlag8 *tf, bool condition, uint16_t timeout);
/** @return l'état (verrouillé) de l'erreur après mise à jour */
bool TimedFlag16_Update(sTimedFlag16 *tf, bool condition, uint16_t timeout);

/* ---- Moyenne par blocs ------------------------------------------------------ */
/** Accumule des échantillons signés et fournit la moyenne tous les `size` échantillons. */
typedef struct
{
    int32_t sum;    /**< somme des échantillons du bloc en cours */
    uint8_t ind;    /**< nombre d'échantillons du bloc en cours */
    int16_t value;  /**< dernière moyenne calculée (conservée entre deux blocs) */
} sBlockAverage;

/** @return true quand une nouvelle moyenne vient d'être calculée dans a->value */
bool BlockAverage_Push(sBlockAverage *a, int16_t sample, uint8_t size);

/* ---- Motif sonore ------------------------------------------------------------ */
#define BEEP_UNCHANGED (-1)  /**< ne pas toucher au buzzer */
#define BEEP_OFF        0    /**< couper le buzzer */
#define BEEP_ON         1    /**< activer le buzzer */
/**
 * Double bip : avance le compteur de période et renvoie la commande du buzzer.
 * Le buzzer est activé au pas 10 et 14, coupé au pas 12 et 16 ; quand le compteur
 * atteint `period` il repart à 0. Entre ces pas la commande est BEEP_UNCHANGED.
 * Le compteur est 8 bits : une période > 255 ne provoque jamais de remise à zéro.
 * Appelée toutes les 100 ms : deux bips de 200 ms espacés de 200 ms,
 * répétés toutes les `period` × 100 ms.
 */
int8_t BeepPattern_Update(uint8_t *cpt, uint16_t period);

#endif /* GENERIC_H */
