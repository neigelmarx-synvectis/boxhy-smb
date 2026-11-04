# Analyse du firmware SMB 2.2.1 triphasé (L1, L2, L3)

| | |
|---|---|
| Date | 2026-11-04 |
| Sources analysées | `assets/old versions/BOXHY_SMB_V2_2_1_L1.X.zip`, `..._L2.X.zip`, `..._L3.X.zip` |
| Référence de comparaison | Firmware 1.3.0 d'origine (`main`, `smb_v1_3_0_main.c`) |
| Objet | Recenser les écarts de la 2.2.1 avec la 1.3.0, en particulier ce qui a été mis en place pour le fonctionnement triphasé, et en tirer les conséquences pour la spécification [`spec/integration_batteries_powertech.md`](spec/integration_batteries_powertech.md) |

## 1. Synthèse

- **Trois firmwares distincts, un par BOXHY.** L1, L2 et L3 sont trois projets identiques, à deux écarts près :
  - l'identifiant `boxhy_ID`, codé en dur à 0, 1 ou 2 ;
  - un oubli dans L3 (§ 4).
- **Un autre microcontrôleur.** La configuration active vise le **dsPIC33EV128GM106**, et non le 32GM106. MCC a été
  régénéré avec une horloge abaissée à 15 MIPS. Le code 2.2.1 **ne tient pas dans le 32GM106** : 34 911 octets
  de programme.
- **Nouveau plan d'identifiants CAN, de type J1939.** L'identifiant de la BOXHY y est porté par l'adresse source :
  `0x10 + ID` pour la SMB, `0x00 + ID` pour la FCCB. Toutes les trames SMB, FCCB et IHM changent d'identifiant.
  Ce plan n'entre en collision avec aucune trame PowerModule.
- **Architecture pair à pair, sans maître.**
  - Les trois BOXHY partagent le bus 48 V et voient toutes les batteries IBS, jusqu'à 16, découvertes par leur
    adresse source.
  - Chaque SMB calcule le SOC et le courant sur l'ensemble des batteries et pilote sa propre pile.
  - Il n'y a ni autorisation de marche, ni limite de puissance.
  - La seule coordination est la diffusion de l'état de chaque SMB et la propagation du mode défaut.
- **Améliorations non liées au triphasé, à reprendre :**
  - la pile ne démarre pas, ou s'arrête, quand l'onduleur est sur source AC ;
  - l'erreur pile mineure n'envoie plus en défaut : elle bloque le démarrage et arrête la pile ;
  - les bugs 3, 4 et 6 sont corrigés, le bug 2 l'est en partie.
- **Anomalies nouvelles, dont trois graves** (§ 7) :
  - aucune erreur critique n'est détectée tant qu'aucune batterie n'a été vue ;
  - un défaut sur une BOXHY bloque les autres en défaut sans retour possible ;
  - la température max batterie ne redescend jamais.

## 2. Généalogie

L'en-tête de `main.c` (autrice : Aurelie JOANNES, 2023-11-10) donne l'historique suivant :

| Version | Contenu |
|---|---|
| 2.0.0 | Adaptations pour l'utilisation triphasée, sur une base 1.2.6 |
| 2.0.1 | Correction du format CAN des signaux de la trame FF26 |
| 2.1.0 | « RETEX Revolt » : intégration des évolutions 1.2.7 et 1.3.0 |
| 2.2.0 | `fcs_minor_error` empêche le passage en FCS_START_1 |
| 2.2.1 | Version émise (constantes `version` / `subversion` / `subsubversion`), sans entrée d'historique |

Points de vérification :

- **La 2.2.1 contient bien la 1.3.0.** Les lignes de la 1.3.0 absentes de la 2.2.1 sont des commentaires, la
  variable `step` remplacée par `boxhy_state[]`, et deux réglages modifiés volontairement (§ 6).
- **Fichiers non compilés.** `smb_v1_2_6_main.c`, présent dans les archives, n'est pas compilé : c'est un reste.
  Le fichier compilé est `main.c`.
- **Fichiers inchangés par rapport à la 1.3.0** : `CTN_SENSOR_V03.C/.H`, `IBS_BATTERY_V02.C/.H` et la base CAN (`.DBF`,
  `.cfx`). **La base CAN n'a donc pas été mise à jour** pour les nouveaux identifiants.

## 3. Matériel, horloge et build

| Élément | 1.3.0 | 2.2.1 |
|---|---|---|
| Microcontrôleur (configuration active) | dsPIC33EV32GM106 | **dsPIC33EV128GM106** (configuration `128GM106`, image « newPIC ») |
| Horloge (quartz 12 MHz) | N1 = 2, M = 32 : Fosc 48 MHz, **FCY 24 MHz** | N1 = 5, M = 50 : Fosc 30 MHz, **FCY 15 MHz** |
| TMR1 (10 ms), TMR5 (100 µs) | PR1 = 29 999, PR5 = 2 399 | PR1 = 18 749, PR5 = 1 499 (périodes conservées) |
| UART1 (onduleur, 2 400 bauds) | BRG = 2 499 | BRG = 1 562 (débit conservé) |
| CAN | 500 kbit/s, 12 TQ, échantillonnage à 75 % | 500 kbit/s, 15 TQ, échantillonnage à 80 % |
| Filtre de réception CAN | `C1RXF0` = 0x0BF / 0x04 | 0x63F / 0x415, même masque : seuls les bits 24 et 26 sont comparés, donc même acceptation |
| Broche OSC2 | sortie horloge | E/S numérique (`OSCIOFNC = ON`) ; sans effet avec un quartz HS |
| Modèle de constantes | `-mconst-in-data` | par défaut : constantes en flash |
| Occupation mémoire | 1.3.0 : tient dans 32 Ko ; 2.0.0 refactorisée : 26 856 octets | **34 911 octets de programme, 1 748 octets de données** : édition de liens impossible sur le 32GM106 |

`#define FCY 6000000` dans `main.c` est inchangé et faux dans les deux versions. Il ne sert qu'aux délais de
`libpic30.h`.

## 4. Écarts entre L1, L2 et L3

| Élément | L1 | L2 | L3 |
|---|---|---|---|
| `boxhy_ID` | 0 | 1 | 2 |
| `other_boxhy_ID` (valeur initiale) | 1 | 2 | 3 |
| `dcac_recieve_cnt++` dans le watchdog onduleur | présent | présent | **absent** : l'indicateur `dcac_recieve_flag` (télémétrie) n'est jamais levé |

Le reste se limite à des espaces, un commentaire et les fichiers de projet MPLAB.

## 5. Ce qui a été mis en place pour le triphasé

### 5.1 Identifiant de BOXHY

- `boxhy_ID` (0 à 2) est une **constante de compilation**, d'où les trois projets.
- Une lecture dynamique était prévue puis désactivée : trame PGN 0xFF65 émise par la FCCB (adresses source 0x00 à
  0x02), avec la ligne `//boxhy_ID = input_msg.data[0];` commentée.
- La variable `step` devient `boxhy_state[boxhy_ID]`, un tableau de 8 dont seule la case de la BOXHY sert.
  `nb_boxhy` est déclaré mais inutilisé.

### 5.2 Plan d'identifiants CAN de type J1939

Identifiant = priorité | PGN 0xFFxx << 8 | adresse source.

- La réception compare le PGN (`msgId >> 8 & 0xFFFF`) et l'adresse source, sans tenir compte de la priorité.
- Les adresses sources :
  - **FCCB** : `0x00 + ID` ;
  - **SMB** : `0x10 + ID` ;
  - **IHM** : `0x10` ;
  - **IBS** : `0xD0` à `0xDF`.

| Trame (nom 1.3.0) | 1.3.0 | 2.2.1 |
|---|---|---|
| `SMB_TX_GENERAL` | 0x02C00000 | 0x0CFF0610 + ID |
| `SMB_TX_RAW1` | 0x02C00001 | 0x18FF0610 + ID |
| `SMB_TX_TEMPERATURE` | 0x02C00002 | 0x18FF1610 + ID |
| `SMB_TX_CURRENTS` | 0x02C00003 | 0x18FF2610 + ID |
| `SMB_TX_VERSION` | 0x02C00004 | 0x18FF3610 + ID |
| `SMB_TX_INV_FLAGS` | 0x02AC0000 | 0x18FF0110 + ID |
| `SMB_TX_INV_GENERAL` | 0x02AC0001 | 0x18FF1110 + ID |
| `SMB_TX_BAT_GENERAL` | 0x02BA0000 | 0x18FF0310 + ID |
| `SMB_TX_BAT_DETAILS` | 0x02BA0001 | 0x18FF2310 + ID |
| `SMB_TX_BAT_RESERVED` | 0x02BA0003 | 0x18FF1310 + ID |
| `SMB_FAN_CONTROL` | 0x0CFF0027 | 0x18FF4610 + ID |
| `SMB_ENERGY_MANAGEMENT` | 0x0CFF0127 | 0x18FF5610 + ID |
| Nouvelle : SOC min et max, nombre de batteries, équilibrage | — | 0x18FF3310 + ID |
| Nouvelle : identifiants `boxhy_ID` / `other_boxhy_ID` | — | 0x18FF7610 + ID |
| Nouvelle : état de la SMB, diffusé aux autres BOXHY | — | 0x18FF9710 + ID |
| Nouvelle : trame de test (`fcs_test_received`, `bool_test`) | — | 0x888 + ID |
| `FCCB_RX_KEEP_ALIVE` | 0x02FC1000 | 0x18FF0200 + ID |
| `FCCB_RX_ON_OFF` (marche, arrêt, maintenance, RAZ) | 0x02FC1001 | 0x18FF1200 + ID |
| `FCCB_RX_OUPUT_CURRENT_LIMIT` | 0x02FC1002 | 0x18FF2200 + ID |
| `FCCB_TX_GENERAL` (reçue) | 0x02FC0000 | PGN 0xFF05, source 0x00 + ID |
| `FCCB_TX_TEMPERATURE` (reçue) | 0x02FC0003 | PGN 0xFF25, source 0x00 + ID |
| `FCCB_TX_PURGE_CC_INFOS` (reçue) | 0x02FC0004 | PGN 0xFF35, source 0x00 + ID |
| Commandes manuelles IHM (0x02C01D00 à 08) | 0x02C01D0x | PGN 0xFF07, 0xFF17 … 0xFF87, source 0x10 |
| `SMB_RX_MAINTENANCE_REQUEST` | 0x02C0100F | PGN 0xFFF7, source 0x10 |
| `SMB_RX_RESET_ORDER` | 0x02C01009 | PGN 0xFF66, source 0x10 |
| `SMB_RX_IHM_ERRORS_ACK` | 0x025C1000 | PGN 0xFF04, source 0x10 |
| IBS (reçues) | 0x18FF0xD0 / D1 | PGN inchangés, source 0xD0 à 0xDF |
| Keep-alive IBS | 0x180AD000 / 0x180AD100 | 0x180AD000 à 0x180AD500, soit 6 batteries |

Conséquences :

- **Firmwares à faire évoluer.** La FCCB et l'IHM des BOXHY triphasées utilisent forcément ce plan d'identifiants.
- **Collisions.** Ce plan n'entre en collision avec aucune trame PowerModule : c'est vérifié pour 1 à 31 modules,
  et pour 16 stacks de 8 modules avec SuperMaster. Le filtre de réception le laisse passer.

### 5.3 Batteries partagées

- **Découverte des batteries.** `sIBS ibs_battery[16]` remplace `ibs_battery_0/1`. Toute trame IBS d'adresse source
  `0xDn` est rangée dans `ibs_battery[n]`. `nb_batteries` vaut le plus grand indice vu, plus un.
- **Données agrégées sur toutes les batteries du bus**, donc sur les trois BOXHY :
  - SOC moyen ;
  - courant total ;
  - températures max ;
  - SOP ;
  - surintensité ;
  - watchdogs ;
  - erreurs ;
  - limite de charge.

  Chaque SMB prend donc ses décisions de mode hybride et de défaut sur l'état global de la batterie.
- **Télémétrie par BOXHY.** Les trames batterie de la SMB n'exposent que ses deux batteries, d'indices
  `2 × ID` et `2 × ID + 1`.
- **Keep-alive.** Chaque SMB émet les keep-alive des six batteries D0 à D5. Ils sont envoyés dans l'ordonnanceur
  CAN (créneaux 17 à 19, toutes les ~220 ms), et l'alternance de l'index se fait toutes les 300 ms.
- **Limite de charge pile.** Elle part de la plus grande limite de charge vue, × 0,7 × 2, quel que soit le nombre
  de batteries.

### 5.4 Coordination entre BOXHY

- **Diffusion de l'état.** Chaque SMB diffuse son état (PGN 0xFF97). Les autres le rangent dans
  `other_boxhy_state[]`.
- **Propagation du défaut.** Si la dernière BOXHY entendue est en défaut, `other_boxhy_emergency_state` est levé.
  Il compte comme une erreur critique, ce qui fait passer la BOXHY en défaut. Il est aussi émis en télémétrie
  (`SMB_TX_INV_FLAGS`, bit 6).
- **Pas de maître, pas d'autorisation, pas de limite de puissance par BOXHY.** Le démarrage et l'arrêt restent
  locaux, commandés par les boutons de chaque BOXHY.

### 5.5 État d'équilibrage batterie (99)

- **Nouvel état.** `SMB_BATTERY_BALANCING_MODE` (99) s'insère entre `SMB_DCAC_START_3` et `SMB_BATTERY_MODE`.
- **Comportement.** Tant que `balancing_mode` est vrai, la SMB reste en 99 : onduleur alimenté, pile interdite.
- **Sortie.** `balancing_mode` passe à faux dès qu'une batterie n'indique plus d'équilibrage. La SMB émet alors
  deux bips et passe en `SMB_BATTERY_MODE`.
- **Effet de bord.** `balancing_mode` n'est jamais remis à vrai : l'attente n'a lieu qu'une fois après la mise
  sous tension.
- **Buzzer.** Les alarmes sonores sont actives à partir de l'état 99, et non plus de l'état 100.

## 6. Autres évolutions, hors triphasé

| Évolution | Détail | Intérêt pour la 2.x |
|---|---|---|
| Onduleur sur source AC | La pile ne démarre pas si l'onduleur est en `INVERTER_AC_SOURCE_MODE`, et s'arrête s'il y passe (FCS_START_1/2, HYBRID, FC_STOP_1/2) | À reprendre |
| Erreur pile mineure (2.2.0) | `fcs_minor_error_flag` sort de `fcs_general_error_flag` : plus de passage en défaut. Elle interdit le démarrage de la pile et l'arrête en mode hybride | À reprendre |
| Température max de charge batterie | 45 → 50 °C. Le commentaire est resté à 45 °C ; la sécurité IBS est indiquée à 50 °C | Sans objet avec les PowerModule |
| Bug 2 (en partie) | Compteur `uint8_t` au lieu de `bool` : un SOC > 95 % pendant 5 s lève l'erreur critique. Le seuil d'avertissement reste à 95 % | Changement de comportement à valider |
| Bug 3 | Réception restructurée : chaque commande a son `break` | Corrigé aussi en 2.0.0 (restructuration) |
| Bug 4 | Parenthèses ajoutées (`buzzer_5_minutes_cnt >> 8`, consignes / 100) | Corrigé aussi en 2.0.0 |
| Bug 6 | Version émise cohérente (2.2.1) | Corrigé aussi en 2.0.0 |
| Télémétrie | Ajouts : `smb_reset_order_flag` et `imh_middle_button_flag` dans `SMB_TX_GENERAL` (octet 3, bits 2 et 3), état défaut d'une autre BOXHY, trame SOC min et max | Le bouton central et l'ordre de RAZ sont utiles au diagnostic |
| Ordonnanceur CAN | 22 créneaux au lieu de 25, numérotation compactée | — |
| Cast signé (`int8_t`) des courants émis | Sans effet sur les octets | — |

Les bugs 1, 5, 7 à 17 de [`bugs.md`](bugs.md) sont toujours présents dans la 2.2.1 : le code concerné n'a pas
changé.

## 7. Anomalies relevées dans la 2.2.1

| N° | Gravité | Anomalie | Conséquence |
|---|---|---|---|
| A1 | Haute | Le test d'erreur critique est placé **dans** la boucle sur les batteries | Tant qu'aucune trame IBS n'a été reçue (`nb_batteries` = 0), aucune erreur critique n'est détectée : logiciel, H2, SOC, températures, défaut d'une autre BOXHY. La perte de toutes les batteries dès le démarrage n'est jamais vue, car il n'y a aucun watchdog |
| A2 | Haute | `other_boxhy_emergency_state` n'est jamais remis à faux, et seule la dernière BOXHY entendue est examinée | Une BOXHY en défaut entraîne les autres en défaut **sans retour possible** : chacune y retombe après acquittement, puisque les autres sont en défaut. Il faut couper l'alimentation des trois |
| A3 | Haute | `battery_max_temperature` n'est jamais remise à zéro. De plus, la température carte est comparée à la mauvaise variable | La température max ne redescend jamais avant redémarrage. Un pic à plus de 50 °C provoque une erreur de surchauffe à chaque recharge suivante |
| A4 | Moyenne | Surintensité et SOP : la remise à zéro est dans la boucle sur les batteries | La dernière batterie saine efface l'état des précédentes (la surintensité est de plus inopérante, bug 9) |
| A5 | Moyenne | Commandes IHM : la trame doit avoir l'adresse source 0x10 **et** `(msgId & 0x0F) == boxhy_ID` | Seule la BOXHY d'ID 0 accepte la maintenance, l'ordre de RAZ et l'acquittement IHM. Sur L2 et L3, seul le bouton central permet de sortir du défaut |
| A6 | Moyenne | Les trois SMB émettent les mêmes keep-alive IBS, même identifiant, avec un contenu qui dépend du tampon d'émission | Si deux SMB émettent en même temps avec des données différentes, l'arbitrage CAN produit des erreurs de bus |
| A7 | Faible | Courant des deux batteries additionné octet par octet dans `SMB_TX_BAT_GENERAL` | La retenue est perdue : valeur fausse en télémétrie |
| A8 | Faible | SOC min et max comparés à la batterie 0 au lieu du minimum et du maximum courants | Télémétrie fausse dès 3 batteries |
| A9 | Faible | Limite de charge : maximum comparé à la batterie 0, multiplié par 0,7 × 2 quel que soit le nombre de batteries | Consigne pile non proportionnée au parc de batteries |
| A10 | Faible | `nb_batteries` n'est remis à zéro que dans `SMB_DCAC_STOP`, inatteignable (bug 14). `balancing_mode` n'est jamais réarmé | Une batterie retirée reste comptée et finit en défaut de watchdog |
| A11 | Faible | L3 : `dcac_recieve_cnt++` absent | Indicateur de réception onduleur jamais levé sur L3 |
| A12 | Faible | Trame de test 0x888 + ID et variables `fcs_test_received` / `bool_test` | Code de mise au point resté en production |
| A13 | Faible | La base CAN (`.DBF`, `.cfx`) n'a pas été mise à jour | BUSMASTER ne décode pas les trames 2.2.1 |

## 8. Conséquences pour la spécification PowerModule et triphasée

1. **Identifiant de BOXHY : deux schémas en concurrence.**
   - La spec v0.6 (§ 6.10) propose un champ sur les bits 14 et 15 des identifiants actuels.
   - La 2.2.1 utilise un plan J1939 : identifiant dans l'adresse source. Les FCCB et IHM triphasées existantes le
     comprennent déjà, et il est lui aussi sans collision avec les PowerModule.

   Il faut choisir entre les deux (Q25).
2. **Architecture.**
   - La 2.2.1 est pair à pair : toutes les BOXHY voient toutes les batteries, et il n'y a pas d'autorisation.
   - La spec v0.5 retient un maître qui pilote la batterie et autorise les esclaves.

   Avec les PowerModule, une seule BOXHY commande la batterie. Les esclaves peuvent cependant lire les Global
   Status sur le bus commun, comme la 2.2.1 lisait toutes les IBS.
3. **Identifiant fixé à la compilation.**
   - La 2.2.1 en fait trois binaires, ce qui contredit la décision Q17 : un seul logiciel, configuré par paramètres.
   - La piste d'un identifiant fourni par la FCCB (PGN 0xFF65), qui dispose d'une EEPROM, est une option
     intéressante pour Q21 et Q24.
4. **Microcontrôleur.** Les BOXHY triphasées sont-elles équipées du 128GM106 ? Le firmware 2.0.0 cible le 32GM106,
   et la configuration MCC n'est pas la même (horloge à 15 MIPS). Il faut savoir quel composant équipe chaque carte
   pour décider de la ou des cibles du firmware unique (Q26).
5. **Améliorations à reporter dans la 2.x :**
   - pas de pile quand l'onduleur est sur source AC ;
   - erreur pile mineure qui ne passe plus en défaut ;
   - indicateurs de diagnostic en télémétrie.
6. **Écueils à ne pas reproduire :**
   - propagation de défaut sans retour (A2) ;
   - erreurs critiques conditionnées à la présence de batteries (A1) ;
   - maximum jamais réinitialisé (A3) ;
   - keep-alive émis par plusieurs nœuds (A6).
