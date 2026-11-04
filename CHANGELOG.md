# Changelog

Toutes les évolutions notables de ce projet sont documentées dans ce fichier.
Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
respecte le [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

### Ajouté
- Mise sous git du projet (dépôt `boxhy-smb`), `.gitignore` MPLAB X / XC16, README et CHANGELOG.
- Tests unitaires hôte (`tests/`, `make -C tests`) : cadre de test maison, doubles de la couche
  HAL, une suite par module (15 suites, 203 tests). Les tests figent le comportement actuel.
- Base CAN de référence `can database/BOXHY.dbc` (format Vector, 60 messages, 311 signaux) et
  générateur `tools/candb_gen.py` (Python standard). Le générateur produit `src/can/can_db.h` et les `.DBF`
  BUSMASTER, et valide la base (signal hors DLC, chevauchement, flottant). `make -C tests` vérifie
  que les fichiers générés sont à jour.
- Couche CAN générique `src/can/can_signal.c/.h` : lecture et écriture d'un signal d'après ses paramètres
  DBC (Intel / Motorola, signé, flottant 32 bits, facteur, offset). Codage et décodage d'une trame
  entière par tables de liaison signal / variable, placées en mémoire programme. Le nombre de liaisons est
  contrôlé à la compilation.

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
- Sources de `src/` regroupées par fonction dans des sous-dossiers : `system/`, `can/`, `devices/`,
  `measures/`, `control/`, `hmi/` et `utils/`. `main.c` et `smb_config.h` restent à la racine de `src/`.
  Les `#include` sont écrits depuis la racine du dépôt. Les projets MPLAB X et VS Code, le Makefile de
  tests et le générateur CAN sont mis à jour. Le firmware produit est identique octet pour octet.
- Base CAN des batteries PowerTech PowerModule, `can database/POWERMODULE.dbc`, d'après le chapitre 10
  du manuel V0.2 du 29/04/26. Elle est générée pour une topologie donnée par `tools/powermodule_dbc.py`,
  car les identifiants encodent l'émetteur, le module et le stack. Par défaut : master + 1 slave,
  30 messages, 221 signaux. `make -C tests` vérifie qu'elle est à jour. Elle n'est pas utilisée par le firmware.
- README : arborescence commentée du dépôt, rapport mémoire de l'éditeur de liens XC16 tel qu'affiché,
  sortie des tests unitaires et couverture de chaque suite.
- Commentaires ajoutés dans tous les fichiers de `src/` et `hal/` : rôle de chaque module, cadence
  d'appel, unités, durée réelle des temporisations, rôle et relais attendus de chaque état de la
  machine d'états, algorithmes de la couche CAN. `can_db.h` reprend les commentaires du `.dbc`.
  Le code est inchangé (firmware identique octet pour octet).
- Toutes les trames CAN lues ou émises par la SMB passent par la couche générique : télémétrie
  (`can_tx.c`), batteries IBS, FCCB (états et commandes), maintenance, ordres IHM. Les trames
  restent identiques octet pour octet, sauf les corrections ci-dessous. Flash 26 559 → 26 856 octets,
  RAM 1 254 → 1 204 octets.
- `.DBF` BUSMASTER régénérés depuis le `.dbc`, avec les trames réellement émises par le firmware :
  SOP dans `SMB_TX_BAT_GENERAL`, octets 5 à 7 de `SMB_TX_GENERAL`, DLC 7 de `SMB_ENERGY_MANAGEMENT`,
  ID `0x2C01D01` de `SMB_RX_DCAC_INPUT_RELAY`, valeurs d'état SMB et FCCB du code (STANDBY = 30).

### Corrigé
- Trame `0x2C00004` : l'octet 4 contient maintenant le poids fort du compteur buzzer 5 min, au lieu de 0 (bug 4).
- Trame `0xCFF0127` : l'octet 2 contient la consigne pile en A (`/100`), au lieu de `(uint8_t)x / 100` (bug 4).
- Températures max IBS (cellules et carte) : mot 16 bits big-endian, au lieu d'un poids fort ×255 (bug 5).
- Courant et limites de courant IBS : décodage complet du flottant IEEE big-endian. L'origine ignorait les
  12 bits de poids faible de la mantisse, d'où un écart pouvant atteindre quelques 0,01 A.

### À valider (écarts entre les anciens `.DBF` et le firmware, le firmware fait foi)
- `IBS.DBF` décrivait 0x18FF0CDx comme deux U16 (×0,05, −1600 A) sur 4 octets ; le firmware décode
  deux flottants big-endian sur 8 octets.
- `FCCB_TX_GENERAL` : la SMB lit `ppmh2_error_flag` (octet 1 bit 2) et `short_circuit_ongoing_flag`
  (octet 5 bit 0), absents de l'ancien `BOXHY_FCCB.DBF`.
- `FCCB_RX_ON_OFF` (0x2FC1001) : décrit en octets entiers sur 8 octets. Les commandes de maintenance
  et de RAZ des erreurs utilisent les octets 2, 3 et 7. Arrêt et démarrage restent émis en DLC 3.

### Écarts volontaires par rapport au comportement d'origine
- Drapeaux partagés avec les ISR déclarés `volatile`.
- Les commandes CAN vers la FCCB (`0x2FC1001`) et le keep-alive IBS utilisent des buffers dédiés :
  les octets non renseignés valent 0 au lieu du contenu résiduel d'un buffer partagé.
- Réception RS232 : écriture bornée au tableau de 120 octets (l'origine débordait sur une trame
  longue sans terminateur).

### Supprimé
- `IBS_BATTERY_V02.C/.H` et `main.c` (squelette MCC) : non compilés, redondants avec le code du main.
- `CTN_SENSOR_V03.C/.H` : repris dans `src/measures/ctn_sensor.c/.h` compilé séparément.
- Variables et messages jamais utilisés (`rsmsgdebug`, `timer()`, champs QPIGS décodés en
  commentaire, boutons gauche/droit filtrés, `battery_hard_overcurrent_timeout`…).

### Anomalies connues, conservées à l'identique (à traiter séparément)
Repérées par `TODO(bug n)` dans le code et figées par des tests `current_behaviour`.
1. `STOPPING` : affectation `fcs_step = FCCB_OPERATING` au lieu d'une comparaison ; état FCCB écrasé.
2. Compteur SOC "très très haut" déclaré `bool` : l'erreur ne se déclenche jamais ; l'avertissement
   SOC haut n'apparaît qu'au-dessus de 95 % (85 % probablement visé).
3. Réception CAN : `break` manquants après `0x2C01009` et `0x25C1000`.
4. ~~Trames `0x2C00004` (octet 4) et `0xCFF0127` (octet 2) : cast 8 bits avant décalage / division.~~ Corrigé.
5. ~~Températures max IBS : poids fort multiplié par 255 au lieu de 256.~~ Corrigé.
6. Version télémétrie 1.2.7 alors que le firmware est 1.3.0.
7. `FCS_START_2` et `FC_STOP_1` : erreur critique non évaluée tant que la déconnexion SOC est active.
8. Compteurs watchdog sans protection ISR (sans impact aujourd'hui).
9. Surintensité batterie : compteur 8 bits et timeout 450 ticks : l'erreur ne se déclenche jamais.
10. Courant pile bas : compteur 8 bits et timeout 600 ticks : l'erreur ne se déclenche jamais.
11. Moyenne de puissance AC sommée sur 16 bits : fausse au-delà de 6553 VA de moyenne.
12. Acquittement du mode défaut : `ctnSensorInit()` remplace les seuils applicatifs des sondes
    (75 / 0 °C et 90 / 0 °C) par les seuils par défaut du module (70 / -5 °C).
13. Démarrage pile : après une première expiration du délai de 60 s en `FCS_START_2`, la nouvelle
    tentative passe en défaut dès le retour en `FCS_START_2` si la FCCB n'est pas déjà `OPERATING`,
    sans laisser à la pile un second délai de 60 s.
14. L'état `DCAC_STOP` (210) est inatteignable : la seule branche qui y menait reprenait la condition
    d'arrêt déjà traitée par `STOPPING`.

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
