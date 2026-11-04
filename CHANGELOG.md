# Changelog

Toutes les évolutions notables de ce projet sont documentées dans ce fichier.
Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
respecte le [Semantic Versioning](https://semver.org/lang/fr/).

Les anomalies connues, ouvertes et corrigées, sont suivies dans [`docs/bugs.md`](docs/bugs.md).

## [Unreleased]

## [2.0.0] – 2026-11-04

Version majeure : la télémétrie CAN change pour l'IHM, avec la correction des bugs 4, 5 et 6.
La structure du code est entièrement revue.

### Ajouté
- Dossier `docs/` : liste des bugs `docs/bugs.md`, ouverts et corrigés, et spécifications `docs/spec/`.
- Spécification d'intégration des batteries PowerTech PowerModule,
  `docs/spec/integration_batteries_powertech.md`, version 0.1 en brouillon. Elle identifie un point bloquant :
  deux identifiants CAN du Master PowerModule sont identiques à des trames de télémétrie de la SMB.
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
Détail des correctifs dans [`docs/bugs.md`](docs/bugs.md).
- Bug 4 : octets de télémétrie tronqués par un cast 8 bits (trames `0x2C00004` et `0xCFF0127`).
- Bug 5 : températures max IBS calculées avec ×255 au lieu de ×256.
- Bug 6 : version émise en télémétrie. Elle vaut désormais 2.0.0, au lieu de 1.2.7.
- Bug 15 : débordement du tampon de réception RS232.
- Bug 16 : octets résiduels dans les commandes FCCB et le keep-alive IBS.
- Bug 17 : courant IBS décodé avec une mantisse tronquée.

### À valider (écarts entre les anciens `.DBF` et le firmware, le firmware fait foi)
- `IBS.DBF` décrivait 0x18FF0CDx comme deux U16 (×0,05, −1600 A) sur 4 octets ; le firmware décode
  deux flottants big-endian sur 8 octets.
- `FCCB_TX_GENERAL` : la SMB lit `ppmh2_error_flag` (octet 1 bit 2) et `short_circuit_ongoing_flag`
  (octet 5 bit 0), absents de l'ancien `BOXHY_FCCB.DBF`.
- `FCCB_RX_ON_OFF` (0x2FC1001) : décrit en octets entiers sur 8 octets. Les commandes de maintenance
  et de RAZ des erreurs utilisent les octets 2, 3 et 7. Arrêt et démarrage restent émis en DLC 3.

### Écarts volontaires par rapport au comportement d'origine
- Drapeaux partagés avec les ISR déclarés `volatile`.
- Les corrections des bugs 15 et 16 ci-dessus.

### Supprimé
- `IBS_BATTERY_V02.C/.H` et `main.c` (squelette MCC) : non compilés, redondants avec le code du main.
- `CTN_SENSOR_V03.C/.H` : repris dans `src/measures/ctn_sensor.c/.h` compilé séparément.
- Variables et messages jamais utilisés (`rsmsgdebug`, `timer()`, champs QPIGS décodés en
  commentaire, boutons gauche/droit filtrés, `battery_hard_overcurrent_timeout`…).

### Anomalies connues
Les bugs 1 à 3 et 7 à 14 restent ouverts et conservés à l'identique. Ils sont repérés par `TODO(bug n)`
dans le code et décrits dans [`docs/bugs.md`](docs/bugs.md).

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

[Unreleased]: https://github.com/neigelmarx-synvectis/boxhy-smb/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/neigelmarx-synvectis/boxhy-smb/compare/v1.3.0...v2.0.0
[1.3.0]: https://github.com/neigelmarx-synvectis/boxhy-smb/releases/tag/v1.3.0
