/**
 * @file generic.c
 * @brief Fonctions génériques réutilisées dans le firmware (voir generic.h).
 *
 * Aucune dépendance matérielle : ce fichier est compilé tel quel dans les tests sur PC.
 */
#include "src/utils/generic.h"

/* Minimum de deux flottants (en cas d'égalité, renvoie a). */
float Generic_MinF(float a, float b)
{
    if (a > b)
        return b;
    else
        return a;
}

/* Maximum de deux flottants (en cas d'égalité, renvoie a). */
float Generic_MaxF(float a, float b)
{
    if (a < b)
        return b;
    else
        return a;
}

/* Borne value dans [min, max] ; min et max sont supposés ordonnés. */
float Generic_Saturation(float min, float value, float max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

/* Limiteur de pente : l'écart à la consigne est saturé à ±ramp avant d'être appliqué, la valeur
 * converge donc vers setPoint d'au plus ramp par appel. Conservé de l'origine (régulation dynamique
 * de la consigne pile, aujourd'hui désactivée). */
float Generic_DynamicsLimiter(float limitedValue, float setPoint, float ramp)
{
    limitedValue += Generic_Saturation(-ramp, setPoint - limitedValue, +ramp);
    return limitedValue;
}

/* Régulateur PI de refroidissement (voir generic.h) : erreur = mesure - consigne. */
uint8_t Generic_PidController(float Kp, float Ki, float Kd, float *int_part, float target, float measurement)
{
    float output;
    (void)Kd; /* terme dérivé non implémenté (code d'origine) */

    *int_part = *int_part + Ki * (measurement - target);        /* intégrale */
    /* Saturation de l'intégrale : contribution bornée entre -20 % et +100 % de la sortie */
    *int_part = Generic_Saturation(-20.0f / Ki, *int_part, 100.0f / Ki); /* saturation intégrale */

    output = Kp * (measurement - target) + (*int_part);
    output = Generic_Saturation(0.0f, output, 100.0f); /* commande en % */

    return (uint8_t)output;
}

/* Chiffre ASCII -> valeur. 0x0F signale un caractère non numérique (espace, point...), valeur reprise
 * telle quelle dans les calculs d'origine. */
uint8_t Generic_CharToDec(char input_char)
{
    if ((input_char > 0x39) || (input_char < 0x30))  /* hors '0'..'9' */
        return 0x0F;
    else
        return (uint8_t)(input_char & 0x0F);
}

/* Anti-rebond symétrique (enfoncement et relâchement) : il faut threshold + 1 échantillons
 * consécutifs différents de l'état filtré pour le faire basculer ; un seul échantillon égal
 * à l'état remet le compteur à zéro. */
bool Debounce_Update(sDebounce *d, bool raw, uint8_t threshold)
{
    if (raw != d->state)
    {
        d->cnt++;
        if (d->cnt > threshold)
        {
            d->state = raw;
            d->cnt = 0;
            return true;
        }
    }
    else
    {
        d->cnt = 0;
    }
    return false;
}

/* Appui maintenu : arme state après threshold + 1 échantillons à 1 consécutifs. Le relâchement
 * remet seulement le compteur à zéro ; state reste armé jusqu'à son acquittement externe. */
bool HoldDetect_Update(sDebounce *d, bool raw, uint8_t threshold)
{
    if (raw)
    {
        d->cnt++;
        if (d->cnt > threshold)
        {
            d->state = true;
            d->cnt = 0;
            return true;
        }
    }
    else
    {
        d->cnt = 0;
    }
    return false;
}

/* Erreur temporisée sur compteur 8 bits. Avec timeout >= 255 le compteur boucle avant d'atteindre
 * le seuil et l'erreur ne se déclenche jamais (TODO(bug 9) et TODO(bug 10), comportement conservé). */
bool TimedFlag8_Update(sTimedFlag8 *tf, bool condition, uint16_t timeout)
{
    if (condition)
    {
        tf->cnt++;
        if (tf->cnt > timeout)
        {
            tf->error = true;   /* verrouillée : seul le propriétaire l'efface */
            tf->cnt = 0;
        }
    }
    else
    {
        tf->cnt = 0;            /* la condition doit être vraie sans interruption */
    }
    return tf->error;
}

/* Erreur temporisée sur compteur 16 bits (même logique que TimedFlag8_Update). */
bool TimedFlag16_Update(sTimedFlag16 *tf, bool condition, uint16_t timeout)
{
    if (condition)
    {
        tf->cnt++;
        if (tf->cnt > timeout)
        {
            tf->error = true;
            tf->cnt = 0;
        }
    }
    else
    {
        tf->cnt = 0;
    }
    return tf->error;
}

/* Moyenne par blocs disjoints (pas une moyenne glissante) : la valeur n'est mise à jour qu'une fois
 * tous les `size` échantillons. Division entière : troncature vers zéro. */
bool BlockAverage_Push(sBlockAverage *a, int16_t sample, uint8_t size)
{
    a->sum += sample;
    a->ind++;
    if (a->ind >= size)
    {
        a->value = (int16_t)(a->sum / a->ind);
        a->sum = 0;
        a->ind = 0;
        return true;
    }
    return false;
}

/* Double bip (voir generic.h). Les pas 10 à 16 sont testés avant la fin de période, si bien qu'une
 * période inférieure à 17 n'interrompt pas le motif. */
int8_t BeepPattern_Update(uint8_t *cpt, uint16_t period)
{
    (*cpt)++;
    if (*cpt == 10)
        return BEEP_ON;
    else if (*cpt == 12)
        return BEEP_OFF;
    else if (*cpt == 14)
        return BEEP_ON;
    else if (*cpt == 16)
        return BEEP_OFF;
    else if (*cpt >= period)
        *cpt = 0;
    return BEEP_UNCHANGED;
}
