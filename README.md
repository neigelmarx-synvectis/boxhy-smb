# BOXHY SMB – Firmware v1.3.0

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

## Organisation du dépôt

| Chemin | Rôle |
|---|---|
| `src/main.c` | Point d'entrée : initialisation des modules, enregistrement des ISR, boucle principale (ordonnanceur à drapeaux) |
| `src/smb_config.h` | Toutes les constantes "métier" : version, seuils, temporisations |
| `src/generic.c/.h` | Fonctions génériques pures : min/max/saturation, PID, anti-rebond, drapeau temporisé, moyenne par blocs, motif sonore |
| `src/timebase.c/.h` | Base de temps TMR1 : drapeaux 100 ms / 300 ms / 1 s, chronomètre de la machine d'états |
| `src/smb_state_machine.c/.h` | Machine d'états SMB (une fonction par état), agrégation des erreurs, acquittement du mode défaut |
| `src/battery.c/.h` | Batteries IBS : décodage CAN, keep-alive, watchdogs, agrégation SOC/courant, supervisions (SOC, surintensité, surchauffe, surtension, SOP) |
| `src/fcs.c/.h` | Pile à combustible via FCCB : états, décodage CAN, commandes, watchdog, temporisations de démarrage |
| `src/ems.c/.h` | Gestion énergétique : courants moyens, courant système, consigne de courant pile |
| `src/inverter.c/.h` | Onduleur RS232 (QMOD / QPIWS / QPIGS) : réception, décodage, polling, watchdog, puissance de sortie, déconnexion sur SOC bas |
| `src/measures.c/.h` | Mesures ADC (4 voies moyennées sur 50 échantillons) et sondes CTN |
| `src/ctn_sensor.c/.h` | Sonde CTN USP10976 : conversion et surveillance de seuils (ex-`CTN_SENSOR_V03`) |
| `src/cooling.c/.h` | Ventilateurs AC : trois régulateurs PI, PWM |
| `src/hmi.c/.h` | Boutons filtrés, motifs du buzzer, limite 5 min, drapeau RAZ IHM |
| `src/maintenance.c/.h` | Mode maintenance : demande et commandes manuelles par CAN |
| `src/can_rx.c/.h` | Réception CAN : aiguillage d'un message vers le module concerné |
| `src/can_tx.c/.h` | Télémétrie CAN : une fonction par trame, cycle de 25 créneaux |
| `hal/` | Couche d'abstraction matérielle (GPIO/PWM, CAN, UART, ADC) : `static inline` sur les drivers MCC, coût nul, substituable par des doubles de test |
| `mcc_generated_files/` | Drivers générés par MCC (horloge, ADC1, CAN1, UART1, TMR1/TMR5, DMA, pin manager, traps) |
| `tests/` | Tests unitaires hôte : `test_framework.h` (cadre maison), `mocks/hal/` (doubles de la HAL), un `test_<module>.c` par module, `Makefile` |
| `can database/` | Bases CAN (`.DBF`) BOXHY_SMB, BOXHY_FCCB, IBS et configuration `.cfx` |
| `nbproject/` | Projet MPLAB X et makefiles générés |
| `cmake/`, `.vscode/` | Projet pour l'extension MPLAB de VS Code |
| `Makefile` | Point d'entrée `make` du projet MPLAB X |

Les dossiers de sortie (`build/`, `dist/`, `debug/`, `_build/`, `out/`, `tests/build/`), les fichiers
privés MPLAB (`nbproject/private/`) et les sauvegardes MCC (`*.mc3.bak*`) sont ignorés par git.

## Architecture logicielle

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

### Machine d'états SMB

`INIT → AUTOCHECK → STANDBY → DCAC_START_1/2/3 → BATTERY_MODE → FCS_START_1/2 → HYBRID_MODE`,
avec les états de sortie `FC_STOP_1/2`, `DCAC_STOP`, `STOPPING`, et les états particuliers
`MAINTENANCE` et `EMERGENCY`. Le passage en mode batterie ou hybride se fait par les boutons
`BATTERY_MODE_BUTTON` et `HYBRID_MODE_BUTTON`.

### Interfaces

- **CAN1** : échange avec la carte FCCB (états `FCCB_*`, commandes `Fcs_Send*`) et avec la batterie IBS (trames `0x18FFxxD0/D1`, keep-alive). Les bases CAN sont dans `can database/`.
- **UART1 (RS232)** : interrogation de l'onduleur avec les commandes `QPIGS`, `QMOD`, `QPIWS` et décodage de son mode (fault, standby, AC source, battery, power saving…).
- **ADC1** : 4 voies moyennées sur 50 échantillons : température 1, température 2, tension d'alimentation, tension de bus.
- **Sorties** : relais `DCAC_INPUT_RELAY`, `DCAC_PRECHARGE_RELAY`, `FCS_OUTPUT_RELAY`, `SUPPLY_PRECHARGE_RELAY`, PWM ventilateurs, buzzer.
- **IHM** : trois boutons avec LED (`IHM_LEFT/MIDDLE/RIGHT_BUTTON`).

### Régulation et seuils

Trois régulateurs PI (`Generic_PidController`) pilotent la PWM des ventilateurs ; la consigne de
courant pile est calculée dans `ems.c` à partir du courant système et de la capacité de charge des
batteries. Tous les seuils (SOC 10 / 25 / 70 / 85 / 95 %, puissance AC, températures, timeouts…)
sont dans `src/smb_config.h`.

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
(fileSet `src/*`, `hal/*`, `mcc_generated_files/*`) génère les fichiers CMake dans `cmake/` et produit
`out/BOXHY_SMB_V1_3_0/default.hex`.

### Occupation mémoire (build debug PICkit 4)

| | v1.3.0 fichier unique | v1.3.0 modulaire |
|---|---|---|
| Flash | 31 659 octets (96 %) | 26 559 octets (80 %) |
| RAM | 1 414 octets | 1 254 octets |

Le gain de flash vient presque entièrement de la suppression de `printf` : les requêtes RS232
sont désormais des tableaux d'octets constants envoyés par `UART1_Write`. Le linker n'embarque
plus le moteur de formatage de la libc (`__printf_core`, `__fmt_x/u/d/o/s`, division 64 bits,
`atoi`, `strlen`…), soit environ 5 000 octets. Le code applicatif lui-même a la même taille
qu'avant le découpage (écart de quelques dizaines d'octets).
Le gain de RAM vient des constantes devenues `#define` : les `const` (placées en RAM par
`-mconst-in-data`) et les variables initialisées jamais modifiées (seuils, timeouts). La suppression
des variables globales inutilisées y contribue aussi.

## Tests unitaires

```sh
make -C tests          # compile et exécute toutes les suites
make -C tests clean
```

Les modules sont compilés avec `gcc -std=c99 -Wall -Wextra -Werror` et les doubles de la HAL
(`tests/mocks/hal/`) à la place de l'implémentation cible. Chaque suite `tests/test_<module>.c`
fige le comportement du firmware d'origine, y compris les anomalies connues (tests nommés
`..._current_behaviour_...`, voir CHANGELOG). Le cadre de test `tests/test_framework.h` est
autonome (pas de dépendance externe).

## Remarques

- Sources en UTF-8 avec fins de ligne LF ; les deux projets (MPLAB X et VS Code) sont déclarés en UTF-8.
- Toute modification de périphérique doit passer par MCC (`test_dspic2.mc3`) puis régénération de `mcc_generated_files/`.
- Les anomalies du firmware d'origine ont été volontairement **conservées** lors du découpage
  (comportement identique) ; elles sont repérées par des commentaires `TODO(bug n)` dans le code
  et listées dans le CHANGELOG.

## Auteurs

Julian VERNIER, Neigel MARX, Gael GRZELAK.
