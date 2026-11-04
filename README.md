# BOXHY SMB – Firmware v2.0.0

Firmware de la carte **SMB** (System Management Board) du système **BOXHY** (H2SYS).
La SMB pilote la logique de haut niveau du système : gestion de la pile à combustible
(via la carte FCCB), de la batterie (IBS), de l'onduleur DC/AC, des relais de puissance
et de l'IHM (boutons, LEDs, buzzer).

## Matériel et outils

| Élément | Valeur |
|---|---|
| Microcontrôleur | dsPIC33EV32GM106 (FCY = 6 MHz, 32 Ko flash, 4 Ko RAM) |
| Compilateur | Microchip XC16 v2.10 |
| IDE | MPLAB X (projet `nbproject/`) ou VS Code + extension MPLAB (`.vscode/*.mplab.json`) |
| Device Family Pack | dsPIC33EV-GM-00X-10X_DFP 1.3.79 |
| Programmateur | PICkit 4 |
| Générateur de périphériques | MPLAB Code Configurator (MCC) – projet `test_dspic2.mc3` |
| Tests unitaires | gcc (hôte Linux), `make -C tests` |
| Base CAN | Vector DBC (`can database/BOXHY.dbc`), générateur Python 3 sans dépendance |

## Organisation du dépôt

```
.
├── src/                          Firmware applicatif
│   ├── main.c                    Point d'entrée : initialisation, ISR, boucle principale (ordonnanceur)
│   ├── smb_config.h              Constantes métier : version, seuils, temporisations
│   ├── system/                   Cœur du système
│   │   ├── smb_state_machine.c/h   Machine d'états SMB (une fonction par état), agrégation des erreurs
│   │   └── timebase.c/h            Base de temps TMR1 : drapeaux 100 ms / 300 ms / 1 s, chronomètre
│   ├── can/                      Bus CAN
│   │   ├── can_signal.c/h          Couche générique : signaux DBC, tables de liaison, pack / unpack
│   │   ├── can_db.h                GÉNÉRÉ depuis BOXHY.dbc : ID, DLC, disposition des signaux
│   │   ├── can_rx.c/h              Aiguillage des trames reçues, ordres IHM
│   │   └── can_tx.c/h              Télémétrie : 13 trames, cycle de 25 créneaux
│   ├── devices/                  Équipements pilotés
│   │   ├── battery.c/h             Batteries IBS : trames, keep-alive, watchdogs, supervisions
│   │   ├── fcs.c/h                 Pile à combustible via la FCCB : états, commandes, watchdog
│   │   └── inverter.c/h            Onduleur RS232 (QMOD / QPIWS / QPIGS) : polling, décodage, protections
│   ├── measures/                 Mesures
│   │   ├── measures.c/h            4 voies ADC moyennées sur 50 échantillons
│   │   └── ctn_sensor.c/h          Sonde CTN USP10976 : conversion et seuils
│   ├── control/                  Régulations
│   │   ├── ems.c/h                 Gestion énergie : courants moyens, consigne de courant pile
│   │   └── cooling.c/h             Ventilateurs AC : trois régulateurs PI, PWM
│   ├── hmi/                      Interfaces opérateur
│   │   ├── hmi.c/h                 Boutons filtrés, motifs du buzzer, limite 5 min
│   │   └── maintenance.c/h         Mode maintenance : commandes manuelles par CAN
│   └── utils/
│       └── generic.c/h             Fonctions pures : saturation, PID, anti-rebond, drapeau temporisé…
├── hal/                          Abstraction matérielle (static inline sur MCC, coût nul)
│   ├── board_io.c/h                Relais, buzzer, LEDs, boutons, PWM ventilateurs
│   ├── can_port.h                  CAN1
│   ├── uart_port.h                 UART1 (RS232 onduleur)
│   └── adc_port.h                  ADC1 : index applicatifs des voies -> voies MCC
├── mcc_generated_files/          Drivers générés par MCC (horloge, ADC1, CAN1, UART1, TMR1/5, DMA…)
├── tests/                        Tests unitaires sur PC
│   ├── Makefile                    make -C tests : contrôle de la base CAN puis 15 suites
│   ├── test_framework.h            Cadre de test maison (aucune dépendance)
│   ├── mocks/hal/                  Doubles de test de la HAL (remplacent hal/ à la compilation)
│   └── test_<module>.c             Une suite par module
├── docs/
│   ├── bugs.md                   Bugs ouverts et corrigés (avec correctif et version de correction)
│   ├── analyse_smb_v2_2_1_triphase.md   Analyse du firmware 2.2.1 triphasé (écarts, anomalies)
│   └── spec/
│       └── integration_batteries_powertech.md   Spécification d'intégration des PowerModule (brouillon)
├── can database/
│   ├── BOXHY.dbc                   BASE CAN DE RÉFÉRENCE : 60 messages, 311 signaux
│   ├── BOXHY_SMB.DBF, BOXHY_FCCB.DBF, IBS.DBF   GÉNÉRÉS pour BUSMASTER
│   ├── BOXHY_CONFIG_1.1.0.cfx      Configuration BUSMASTER (charge les trois .DBF)
│   └── POWERMODULE.dbc             GÉNÉRÉ : batteries PowerTech PowerModule (manuel V0.2)
├── tools/
│   ├── candb_gen.py              Générateur : .dbc -> src/can/can_db.h + .DBF (option --check)
│   └── powermodule_dbc.py        Générateur du .dbc PowerModule selon la topologie (option --check)
├── nbproject/, Makefile          Projet MPLAB X
├── cmake/, .vscode/              Projet pour l'extension MPLAB de VS Code
├── test_dspic2.mc3               Projet MCC
├── README.md
└── CHANGELOG.md                  Historique des versions
```

Les dossiers de sortie (`build/`, `dist/`, `debug/`, `_build/`, `out/`, `tests/build/`), les fichiers
privés MPLAB (`nbproject/private/`) et les sauvegardes MCC (`*.mc3.bak*`) sont ignorés par git.

## Architecture logicielle

- Les `#include` sont écrits depuis la racine du dépôt (`"src/devices/battery.h"`, `"hal/can_port.h"`,
  `"mcc_generated_files/..."`) : la seule racine d'inclusion des projets est le dossier du projet.
- Seuls `hal/*` et `src/main.c` incluent des en-têtes `mcc_generated_files/` (hormis `can_types.h`,
  portable). Les modules applicatifs ne voient le matériel qu'à travers `hal/board_io.h`,
  `hal/can_port.h`, `hal/uart_port.h`, `hal/adc_port.h`.
- Chaque module possède une structure publique unique (`battery`, `fcs`, `inverter`, `measures`,
  `hmi`, `ems`, `cooling`, `maintenance`, `smb`, `timebase`) lisible par les autres modules ;
  l'écriture externe passe par des fonctions, sauf quelques drapeaux de dialogue avec la machine
  d'états, documentés dans les en-têtes.
- Les ISR (`Timebase_OnTick`, `Measures_OnConversionDone`, `Inverter_OnRxInterrupt`) sont des
  fonctions ordinaires enregistrées par `main.c` auprès des drivers MCC.
- Boucle principale : `Smb_Run()` à chaque tour, puis une seule tâche "événement" (réception CAN,
  conversion ADC, moyennes ADC), puis une seule tâche "périodique" (100 ms, 300 ms, 1 s, trame
  RS232 reçue, émission CAN), dans cet ordre de priorité.
- Chaque fichier est commenté : rôle du module, cadence d'appel, unités, durée réelle des
  temporisations ; pour la machine d'états, rôle, relais attendus et sorties de chaque état.

### Machine d'états SMB

`INIT → AUTOCHECK → STANDBY → DCAC_START_1/2/3 → BATTERY_MODE → FCS_START_1/2 → HYBRID_MODE`,
avec les états de sortie `FC_STOP_1/2`, `STOPPING`, et les états particuliers `MAINTENANCE` et
`EMERGENCY`. Le passage en mode batterie ou hybride se fait par les boutons `BATTERY_MODE_BUTTON` et
`HYBRID_MODE_BUTTON`. L'état `DCAC_STOP` existe mais n'est jamais atteint (bug 14, voir [`docs/bugs.md`](docs/bugs.md)).

### Interfaces

- **CAN1** : échange avec la carte FCCB (états `FCCB_*`, commandes `Fcs_Send*`), avec les batteries IBS (trames `0x18FFxxD0/D1`, keep-alive) et avec l'IHM (télémétrie, maintenance). Toutes les trames sont décrites dans `can database/BOXHY.dbc` (voir « Base CAN »).
- **UART1 (RS232)** : interrogation de l'onduleur avec les commandes `QPIGS`, `QMOD`, `QPIWS` et décodage de son mode (fault, standby, AC source, battery, power saving…).
- **ADC1** : 4 voies moyennées sur 50 échantillons : température 1, température 2, tension d'alimentation, tension de bus. Les conversions sont déclenchées par TMR5 toutes les 100 µs.
- **Sorties** : relais `DCAC_INPUT_RELAY`, `DCAC_PRECHARGE_RELAY`, `FCS_OUTPUT_RELAY`, `SUPPLY_PRECHARGE_RELAY`, PWM ventilateurs, buzzer.
- **IHM** : trois boutons avec LED (`IHM_LEFT/MIDDLE/RIGHT_BUTTON`).

### Régulation et seuils

Trois régulateurs PI (`Generic_PidController`) pilotent la PWM des ventilateurs ; la consigne de
courant pile est calculée dans `ems.c` à partir du courant système et de la capacité de charge des
batteries. Tous les seuils (SOC 10 / 25 / 70 / 85 / 95 %, puissance AC, températures, timeouts…)
sont dans `src/smb_config.h`.

## Base CAN

Le fichier `can database/BOXHY.dbc` est la seule référence pour la disposition des trames.
Le firmware ne contient aucune position de bit écrite à la main : chaque trame lue ou émise
est décrite par une **table de liaisons** qui associe un signal du `.dbc` à une variable du firmware.

```c
static const sCanBinding smb_currents[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_CURRENTS__FCS_OUTPUT_CURRENT, &fcs.output_current, CAN_VAR_I16, 0.01),
    ...
};
CAN_CHECK_BINDINGS(CANDB_SMB_TX_CURRENTS, smb_currents);
```

- `CAN_BIND(signal, &variable, type, unité)` : l'unité est la valeur physique d'un LSB de la variable.
  La conversion avec le facteur et l'offset DBC est précalculée en entiers à la compilation.
  `CAN_BIND_CONST` émet une valeur fixe, et `CAN_BIND_NONE` ignore un signal.
- `CanMsg_Pack()` / `CanMsg_Unpack()` codent ou décodent une trame entière. Les fonctions
  `CanSignal_Get*` / `CanSignal_Set*` traitent un signal isolé : Intel ou Motorola, signé, flottant 32 bits.
- `CAN_CHECK_BINDINGS` fait échouer la compilation si un signal du `.dbc` n'a pas de liaison.
- Les tables sont placées en mémoire programme (`CAN_ROM`, attribut `space(auto_psv)`) : elles ne
  consomment pas de RAM malgré `-mconst-in-data`. C'est la section `.const` du rapport mémoire.

Pour modifier une trame :

1. Modifier `can database/BOXHY.dbc`, avec CANdb++, un éditeur de texte ou cantools.
2. Lancer `python3 tools/candb_gen.py`, qui régénère `src/can/can_db.h` et les trois `.DBF` BUSMASTER.
3. Mettre à jour la table de liaisons du module concerné. La compilation signale un signal oublié.
4. `make -C tests` vérifie que les fichiers générés sont à jour, puis lance les tests.

Les `.DBF` sont répartis par préfixe de nom de message : `SMB_`, `FCCB_` et `IBS_`.

### Batteries PowerModule

`can database/POWERMODULE.dbc` décrit le protocole CAN des batteries PowerTech PowerModule, d'après le
chapitre 10 du manuel « PowerModule – Installation et manuel utilisateur » V0.2 du 29/04/26. Il est
indépendant de `BOXHY.dbc` et n'est pas utilisé par le firmware.

Les identifiants PowerModule ne sont pas fixes. Ils encodent l'émetteur (master, slave ou SuperMaster),
le numéro de module (1 à 31) et le numéro de stack (1 à 31). Le fichier est donc généré pour une
topologie donnée par `tools/powermodule_dbc.py`. Par défaut, c'est la configuration BOXHY : un master et un
slave en parallèle, sans SuperMaster, à 500 kbit/s. Le fichier contient 30 messages et 221 signaux et couvre
aussi le cas d'un module seul. Voir la spécification d'intégration
[`docs/spec/integration_batteries_powertech.md`](docs/spec/integration_batteries_powertech.md).

```sh
python3 tools/powermodule_dbc.py                                   # master + 1 slave, 500 kbit/s (défaut)
python3 tools/powermodule_dbc.py --baudrate 250000 --output pm_usine.dbc  # débit usine des modules
python3 tools/powermodule_dbc.py --modules 6                       # master + 5 slaves en parallèle
python3 tools/powermodule_dbc.py --supermaster --stacks 3 --modules 4 --output pm_3s4p.dbc
```

Pour chaque module, le fichier contient Status 1 et 2, Error/Warning, les tensions de cellules,
l'équilibrage et les paramètres. Il contient aussi les commandes du contrôleur externe : maintenance,
contacteur, chauffage, lecture et écriture des paramètres. S'y ajoutent les Global Status 1 et 2 du master
ou du MainMaster et, avec SuperMaster, les Status 1 et 2 et l'Error/Warning de chaque SuperMaster.
Les messages internes non décrits par le manuel (Master command, Numbering, Total slave, mises à jour)
ne sont pas inclus.

Points du manuel à faire confirmer par PowerTech :

- **Unités variables de l'IMR et de l'IMD (Global Status 1) :** un bit d'unité multiplie la valeur par 10.
  Un `.dbc` ne sait pas exprimer ce facteur variable. Les signaux sont décrits dans l'unité fine,
  et le commentaire du signal rappelle la correction.
- **Cell balancing :** le manuel donne l'identifiant `0x1Ayyy209`, avec le bit 27 à 1 alors qu'il n'est pas
  utilisé dans le format d'identifiant. Il est repris tel quel.
- **Maintenance :** le manuel indique un DLC de 1, mais les demandes de status utilisent les bits 12
  et 13. Le DLC est porté à 2.
- **Exemple de maintenance du §10.2.4.2 (`0x13021605`) :** le type de trame y vaut « Rx », alors que
  le §10.2.4.1 utilise « Dreq » (`0x13041A05`). Le fichier suit le §10.2.4.1.
- **Exemple de décodage du §10.2.3.3 :** le SOC annoncé (49,78 %) est une erreur de calcul. 0x130E
  vaut 4878, soit 48,78 %. Les autres valeurs de l'exemple sont retrouvées.

## Compiler

### MPLAB X
Ouvrir le dossier comme projet MPLAB X et lancer *Build* (configuration `default`).
À la première ouverture après le découpage en modules, MPLAB X régénère `nbproject/Makefile-default.mk`
à partir de `nbproject/configurations.xml` (le makefile commité référence encore l'ancien fichier unique).

```sh
make build CONF=default
```

Sortie : `dist/default/production/BOXHY_SMB_V1_3_0.X.production.hex`.

### VS Code
Ouvrir le dossier avec l'extension MPLAB installée ; le projet `.vscode/BOXHY_SMB_V1_3_0.mplab.json`
(fileSet `src/*` et ses sous-dossiers, `hal/*`, `mcc_generated_files/*`) génère les fichiers CMake dans
`cmake/` et produit `out/BOXHY_SMB_V1_3_0/default.hex`.

### Résultat du build

Build du 4 novembre 2026, configuration `default` (debug PICkit 4), avec les options du projet :
`-O1 -mlarge-code -msmall-data -msmall-scalar -mconst-in-data -msmart-io=1 -Wall`, édition des liens
avec `--gc-sections --report-mem`. Aucune erreur, aucun warning dans le code du projet (seul reste
l'avertissement de MCC sur `adc1.h`, variable `result` possiblement non initialisée).

Rapport mémoire de l'éditeur de liens, tel qu'affiché par `xc16-ld` :

```
xc16-ld 2.10 (B)

Default Code Model: Small
Default Data Model: Small
Default Scalar Model: Small

"program" Memory  [Origin = 0x200, Length = 0x5580]

section                    address   length (PC units)   length (bytes) (dec)
-------                    -------   -----------------   --------------------
.text                        0x200               0xc34          0x124e  (4686)
.const                       0xe34               0xd02          0x1383  (4995)
.text                       0x1b36              0x2958          0x3e04  (15876)
.dinit                      0x448e                0x9e            0xed  (237)
.text                       0x452c               0x2c4           0x426  (1062)

                 Total "program" memory used (bytes):         0x68e8  (26856) 81%


"data" Memory  [Origin = 0x1000, Length = 0x1000]

section                    address      alignment gaps    total length  (dec)
-------                    -------      --------------    -------------------
reserve_data_0x1000         0x1000                0x1c            0x1c  (28)
reserve_data_0x101c         0x101c                 0x2             0x2  (2)
reserve_data_0x101e         0x101e                 0x2             0x2  (2)
reserve_data_0x1020         0x1020                 0x2             0x2  (2)
reserve_data_0x1022         0x1022                 0x2             0x2  (2)
reserve_data_0x1024         0x1024                 0x4             0x4  (4)
reserve_data_0x1028         0x1028                0x28            0x28  (40)
.nbss                       0x1050                   0           0x19c  (412)
.nbss                       0x11ec                   0            0x12  (18)
.ndata                      0x11fe                   0             0x2  (2)
_0x71df968110a06aeb5a1d     0x1200                   0           0x200  (512)
.nbss                       0x1400                   0            0x3a  (58)
.ndata                      0x143a                   0            0x18  (24)
.nbss                       0x1452                   0            0x5c  (92)
.nbss                       0x14ae                   0             0x6  (6)

                 Total "data" memory used (bytes):          0x4b4  (1204) 29%


Dynamic Memory Usage

region                     address                      maximum length  (dec)
------                     -------                      ---------------------
heap                             0                                   0  (0)
stack                       0x14b4                               0xb4c  (2892)

                 Maximum dynamic memory (bytes):          0xb4c  (2892)
```

Lecture du rapport :

| Section | Contenu |
|---|---|
| `.text` (×3) | Code exécutable : application, drivers MCC, code de démarrage et bibliothèques C |
| `.const` | Tables de liaison CAN, lues via la fenêtre PSV (3 330 octets de données, 4 995 octets de flash) |
| `.dinit` | Valeurs initiales des variables, recopiées en RAM au démarrage |
| `reserve_data_*` | RAM réservée au débogueur PICkit 4 (build debug uniquement) |
| `.nbss` / `.ndata` | Variables globales et statiques (zéro / initialisées) en mémoire proche |
| `_0x71df…` (512 octets) | Tampons de messages CAN1 (`can1msgBuf`) du driver MCC, alignés pour le DMA |
| `stack` | RAM restante, entièrement disponible pour la pile (2 892 octets) |

Une unité PC (« PC units ») vaut 1,5 octet de flash : une instruction de 24 bits occupe 2 unités d'adresse.

### Évolution de l'occupation mémoire

| | v1.3.0 d'origine | Découpage en modules | v2.0.0 (CAN générique) |
|---|---|---|---|
| Flash | 31 659 octets (96 %) | 26 559 octets (80 %) | 26 856 octets (81 %) |
| RAM | 1 414 octets | 1 254 octets | 1 204 octets (29 %) |

Le gain de flash vient presque entièrement de la suppression de `printf` : les requêtes RS232
sont désormais des tableaux d'octets constants envoyés par `UART1_Write`. Le linker n'embarque
plus le moteur de formatage de la libc (`__printf_core`, `__fmt_x/u/d/o/s`, division 64 bits,
`atoi`, `strlen`…), soit environ 5 000 octets. Le code applicatif lui-même a la même taille
qu'avant le découpage (écart de quelques dizaines d'octets).
Le gain de RAM vient des constantes devenues `#define` : les `const` (placées en RAM par
`-mconst-in-data`) et les variables initialisées jamais modifiées (seuils, timeouts). La suppression
des variables globales inutilisées y contribue aussi.
Le passage au CAN générique coûte 297 octets de flash au total. Les tables de liaison (section
`.const`) occupent 3,3 Ko de données lues via la fenêtre PSV, soit environ 5 Ko de mémoire programme.
Elles remplacent le codage écrit à la main et l'ancien décodage IBS, qui utilisait `powf` de la libm.
Le passage libère aussi 50 octets de RAM, car les buffers CAN propres à chaque module ont disparu.
Le regroupement des sources en dossiers et l'ajout des commentaires n'ont pas modifié le binaire
(fichier `.hex` identique octet pour octet).

## Tests unitaires

```sh
make -C tests          # vérifie les bases CAN générées, compile et exécute toutes les suites
make -C tests clean
```

Les modules sont compilés avec `gcc -std=c99 -Wall -Wextra -Werror` et les doubles de la HAL
(`tests/mocks/hal/`) à la place de l'implémentation cible. Chaque suite `tests/test_<module>.c`
fige le comportement du firmware d'origine, y compris les anomalies connues (tests nommés
`..._current_behaviour_...`, voir [`docs/bugs.md`](docs/bugs.md)). Le cadre de test `tests/test_framework.h` est
autonome (pas de dépendance externe).

### Résultat

Sortie de `make -s -C tests` du 4 novembre 2026 (gcc 13.3, Linux) :

```
candb_gen: 60 messages, 311 signaux, fichiers générés à jour
== test_can_signal.c ==
   20 test(s), 80 assertion(s), 0 echec(s)
== test_generic.c ==
   19 test(s), 820 assertion(s), 0 echec(s)
== test_timebase.c ==
   5 test(s), 15 assertion(s), 0 echec(s)
== test_ctn_sensor.c ==
   10 test(s), 55 assertion(s), 0 echec(s)
== test_measures.c ==
   8 test(s), 639 assertion(s), 0 echec(s)
== test_cooling.c ==
   5 test(s), 21 assertion(s), 0 echec(s)
== test_fcs.c ==
   11 test(s), 91 assertion(s), 0 echec(s)
== test_battery.c ==
   20 test(s), 91 assertion(s), 0 echec(s)
== test_ems.c ==
   8 test(s), 22 assertion(s), 0 echec(s)
== test_inverter.c ==
   24 test(s), 103 assertion(s), 0 echec(s)
== test_hmi.c ==
   12 test(s), 41 assertion(s), 0 echec(s)
== test_maintenance.c ==
   6 test(s), 30 assertion(s), 0 echec(s)
== test_can_rx.c ==
   9 test(s), 19 assertion(s), 0 echec(s)
== test_can_tx.c ==
   17 test(s), 120 assertion(s), 0 echec(s)
== test_smb_state_machine.c ==
   29 test(s), 151 assertion(s), 0 echec(s)
```

Total : 15 suites, 203 tests, 2 298 assertions, 0 échec. Une suite en échec arrête `make` avec un
code de retour non nul et affiche le fichier, la ligne, la valeur attendue et la valeur obtenue.

### Couverture par suite

| Suite | Ce qui est vérifié |
|---|---|
| `test_can_signal` | Lecture / écriture Intel et Motorola, signe, flottant 32 bits, conversions d'unités ; les 311 signaux de la base comparés à une implémentation de référence bit par bit |
| `test_generic` | Saturation, PID, anti-rebond, drapeaux temporisés, moyenne par blocs, motif sonore |
| `test_timebase` | Drapeaux 100 ms, 300 ms, 1 s et chronomètre à partir des ticks de 10 ms |
| `test_ctn_sensor` | Table de conversion USP10976, seuils, perte de capteur |
| `test_measures` | Rotation des voies ADC, moyennage sur 50 échantillons, conversions en tensions et températures |
| `test_cooling` | Régulateurs PI des ventilateurs, choix de la demande maximale, saturation 15–100 % |
| `test_fcs` | Décodage des trames FCCB, octets et DLC des commandes, watchdog, temporisations de démarrage |
| `test_battery` | Décodage IBS (flottant big-endian, mots Motorola, bit inversé), keep-alive, watchdogs, supervisions |
| `test_ems` | Moyennes de courant, consigne de courant pile et ses bornes |
| `test_inverter` | Assemblage des trames RS232, décodage QMOD / QPIWS / QPIGS, polling, surcharge, déconnexion sur SOC bas |
| `test_hmi` | Anti-rebond des boutons, motifs du buzzer, limite de 5 min en mode défaut |
| `test_maintenance` | Entrée / sortie du mode maintenance, commandes manuelles, clé 0x82 |
| `test_can_rx` | Aiguillage des trames reçues vers les modules, ordres de l'IHM |
| `test_can_tx` | Les 13 trames de télémétrie octet par octet, créneaux d'émission |
| `test_smb_state_machine` | Transitions de tous les états, relais et buzzer attendus, acquittement du mode défaut |

## Remarques

- Sources en UTF-8 avec fins de ligne LF ; les deux projets (MPLAB X et VS Code) sont déclarés en UTF-8.
- Toute modification de périphérique doit passer par MCC (`test_dspic2.mc3`) puis régénération de `mcc_generated_files/`.
- Les anomalies connues sont suivies dans [`docs/bugs.md`](docs/bugs.md). Les bugs ouverts sont repérés dans
  le code par `TODO(bug n)` et figés par des tests `current_behaviour`. Les bugs corrigés y restent, avec le
  détail du correctif et la version de correction.
- La version du firmware est définie dans `src/smb_config.h` (`SMB_VERSION`, `SMB_SUBVERSION`,
  `SMB_SUBSUBVERSION`) et émise dans la trame 0x2C00004. Elle se met à jour avec le CHANGELOG.
  Les noms de projet IDE (`BOXHY_SMB_V1_3_0`) sont conservés, pour ne pas casser les projets MPLAB X et VS Code.

## Auteurs

Julian VERNIER, Neigel MARX, Gael GRZELAK.
