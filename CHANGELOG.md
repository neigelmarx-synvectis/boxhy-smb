# Changelog

Toutes les évolutions notables de ce projet sont documentées dans ce fichier.
Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
respecte le [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

### Ajouté
- Mise sous git du projet (dépôt `boxhy-smb`), `.gitignore` MPLAB X / XC16, README et CHANGELOG.
- Tests unitaires hôte (`tests/`, `make -C tests`) : cadre de test maison, doubles de la couche
  HAL, une suite par module (14 suites, 182 tests). Les tests figent le comportement actuel.

### Modifié
- Découpage du fichier unique `smb_v1_3_0_main.c` (3027 lignes) en modules `src/` par
  fonctionnalité (machine d'états, batteries, pile, onduleur, mesures, gestion énergie, IHM,
  refroidissement, maintenance, réception et émission CAN, base de temps, fonctions génériques)
  et couche d'abstraction matérielle `hal/` (GPIO/PWM, CAN, UART, ADC) en `static inline` sur les
  drivers MCC. Comportement fonctionnel identique (mêmes trames CAN octet pour octet, mêmes
  temporisations, mêmes transitions), aux écarts volontaires près listés ci-dessous.
- Constantes "métier" regroupées dans `src/smb_config.h` (`#define` au lieu de `const` : plus de
  RAM consommée par `-mconst-in-data`). RAM : 1 414 → 1 254 octets.
- Requêtes RS232 `QMOD` / `QPIWS` / `QPIGS` émises comme tableaux d'octets constants via UART1 au
  lieu de `printf` : octets identiques sur la ligne. Le moteur de formatage de la libc n'est plus
  lié, ce qui libère ~5 Ko de flash (31 659 → 26 559 octets, 96 % → 80 %). Le code applicatif
  garde la même taille après découpage.
- Sources converties en UTF-8 / LF ; encodage des projets MPLAB X et VS Code passé à UTF-8.
- Descripteurs de projet (`nbproject/configurations.xml`, `.vscode/BOXHY_SMB_V1_3_0.mplab.json`)
  mis à jour avec les nouveaux fichiers.

### Écarts volontaires par rapport au comportement d'origine
- Drapeaux partagés avec les ISR déclarés `volatile`.
- Les commandes CAN vers la FCCB (`0x2FC1001`) et le keep-alive IBS utilisent des buffers dédiés :
  les octets non renseignés valent 0 au lieu du contenu résiduel d'un buffer partagé.
- Réception RS232 : écriture bornée au tableau de 120 octets (l'origine débordait sur une trame
  longue sans terminateur).

### Supprimé
- `IBS_BATTERY_V02.C/.H` et `main.c` (squelette MCC) : non compilés, redondants avec le code du main.
- `CTN_SENSOR_V03.C/.H` : repris dans `src/ctn_sensor.c/.h` compilé séparément.
- Variables et messages jamais utilisés (`rsmsgdebug`, `timer()`, champs QPIGS décodés en
  commentaire, boutons gauche/droit filtrés, `battery_hard_overcurrent_timeout`…).

### Anomalies connues, conservées à l'identique (à traiter séparément)
Repérées par `TODO(bug n)` dans le code et figées par des tests `current_behaviour`.
1. `STOPPING` : affectation `fcs_step = FCCB_OPERATING` au lieu d'une comparaison ; état FCCB écrasé.
2. Compteur SOC "très très haut" déclaré `bool` : l'erreur ne se déclenche jamais ; l'avertissement
   SOC haut n'apparaît qu'au-dessus de 95 % (85 % probablement visé).
3. Réception CAN : `break` manquants après `0x2C01009` et `0x25C1000`.
4. Trames `0x2C00004` (octet 4) et `0xCFF0127` (octet 2) : cast 8 bits avant décalage / division.
5. Températures max IBS : poids fort multiplié par 255 au lieu de 256.
6. Version télémétrie 1.2.7 alors que le firmware est 1.3.0.
7. `FCS_START_2` et `FC_STOP_1` : erreur critique non évaluée tant que la déconnexion SOC est active.
8. Compteurs watchdog sans protection ISR (sans impact aujourd'hui).
9. Surintensité batterie : compteur 8 bits et timeout 450 ticks : l'erreur ne se déclenche jamais.
10. Courant pile bas : compteur 8 bits et timeout 600 ticks : l'erreur ne se déclenche jamais.
11. Moyenne de puissance AC sommée sur 16 bits : fausse au-delà de 6553 VA de moyenne.
12. Acquittement du mode défaut : `ctnSensorInit()` remplace les seuils applicatifs des sondes
    (75 / 0 °C et 90 / 0 °C) par les seuils par défaut du module (70 / -5 °C).

## [1.3.0] – 2026-11-04

Version courante du firmware (`smb_v1_3_0_main.c`, projet `BOXHY_SMB_V1_3_0`).
Première version versionnée sous git ; l'historique détaillé des changements fonctionnels
antérieurs n'a pas été conservé.

### Contenu
- Machine d'états SMB (init, autocheck, standby, maintenance, démarrage DC/AC, mode batterie,
  démarrage pile, mode hybride, arrêts, urgence).
- Communication CAN avec la carte FCCB et la batterie IBS.
- Communication RS232 avec l'onduleur (`QPIGS`, `QMOD`, `QPIWS`).
- Régulation PID de la consigne de courant pile selon le SOC batterie.
- Gestion des relais de puissance, PWM ventilateurs/électrovanne, buzzer et IHM 3 boutons.
- Module `CTN_SENSOR` V03 et module `IBS_BATTERY` V02.

## Versions antérieures

Versions connues par les artefacts de build mais non documentées : 1.2.5, 1.2.6, 1.2.7.
L'en-tête du fichier principal mentionne la version 1.2.3 datée du 2022-10-06.

### Historique des modules

**CTN_SENSOR**
- V03 – 2022-01-24 (JV) : refonte et simplification.
- V02 – 2021-12-09 (JV) : corrections.
- V01 – 2021-08-23 (JV) : version initiale.

**IBS_BATTERY**
- V02 – 2022-02-07 (JV) : adaptation pour XC16.
- V01 – 2021-08-23 (JV) : version initiale.

[Unreleased]: https://github.com/neigelmarx-synvectis/boxhy-smb/compare/v1.3.0...HEAD
[1.3.0]: https://github.com/neigelmarx-synvectis/boxhy-smb/releases/tag/v1.3.0
