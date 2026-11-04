# BOXHY SMB – Firmware v1.3.0

Firmware de la carte **SMB** (System Management Board) du système **BOXHY** (H2SYS).
La SMB pilote la logique de haut niveau du système : gestion de la pile à combustible
(via la carte FCCB), de la batterie (IBS), de l'onduleur DC/AC, des relais de puissance
et de l'IHM (boutons, LEDs, buzzer).

## Matériel et outils

| Élément | Valeur |
|---|---|
| Microcontrôleur | dsPIC33EV32GM106 (FCY = 6 MHz) |
| Compilateur | Microchip XC16 v2.10 |
| IDE | MPLAB X (projet `nbproject/`) ou VS Code + extension MPLAB (`.vscode/*.mplab.json`) |
| Device Family Pack | dsPIC33EV-GM-00X-10X_DFP 1.3.79 |
| Programmateur | PICkit 4 |
| Générateur de périphériques | MPLAB Code Configurator (MCC) – projet `test_dspic2.mc3` |

## Organisation du dépôt

| Chemin | Rôle |
|---|---|
| `smb_v1_3_0_main.c` | Application principale : machine d'états SMB, communication CAN/RS232, régulation, IHM |
| `CTN_SENSOR_V03.C/.H` | Lecture et surveillance des sondes de température CTN (seuils, timeouts, flags d'erreur) |
| `IBS_BATTERY_V02.C/.H` | Décodage des trames CAN batterie IBS et keep-alive |
| `mcc_generated_files/` | Drivers générés par MCC (horloge, ADC1, CAN1, UART1, TMR1/TMR5, DMA, pin manager, traps) |
| `main.c` | Squelette généré par MCC, **non compilé** (exclu du projet) |
| `can database/` | Bases CAN (`.DBF`) BOXHY_SMB, BOXHY_FCCB, IBS et configuration `.cfx` |
| `nbproject/` | Projet MPLAB X et makefiles générés |
| `cmake/`, `.vscode/` | Projet pour l'extension MPLAB de VS Code |
| `Makefile` | Point d'entrée `make` du projet MPLAB X |

Les dossiers de sortie (`build/`, `dist/`, `debug/`, `_build/`, `out/`), les fichiers
privés MPLAB (`nbproject/private/`) et les sauvegardes MCC (`*.mc3.bak*`) sont ignorés par git.

## Fonctionnement

### Machine d'états SMB

`INIT → AUTOCHECK → STANDBY → DCAC_START_1/2/3 → BATTERY_MODE → FCS_START_1/2 → HYBRID_MODE`,
avec les états de sortie `FC_STOP_1/2`, `DCAC_STOP`, `STOPPING`, et les états particuliers
`MAINTENANCE` et `EMERGENCY`. Le passage en mode batterie ou hybride se fait par les boutons
`BATTERY_MODE_BUTTON` et `HYBRID_MODE_BUTTON`.

### Interfaces

- **CAN1** : échange avec la carte FCCB (états `FCCB_*`, commandes `FCS_Start/Stop/MaintenanceMode/CriticalStop/ResetErrors/SetCurrentLimit`) et avec la batterie IBS (trames `0x18FFxxD0/D1`, keep-alive). Les bases CAN sont dans `can database/`.
- **UART1 (RS232)** : interrogation de l'onduleur avec les commandes `QPIGS`, `QMOD`, `QPIWS` et décodage de son mode (fault, standby, AC source, battery, power saving…).
- **ADC1** : 4 voies moyennées sur 50 échantillons : température 1, température 2, tension d'alimentation, tension de bus.
- **Sorties** : relais `DCAC_INPUT_RELAY`, `DCAC_PRECHARGE_RELAY`, `FCS_OUTPUT_RELAY`, `SUPPLY_PRECHARGE_RELAY`, PWM ventilateurs et électrovanne réservoir, buzzer.
- **IHM** : trois boutons avec LED (`IHM_LEFT/MIDDLE/RIGHT_BUTTON`).

### Régulation

Un régulateur PID (`PID_controller`) avec saturation et limiteur de dynamique (`dynamicsLimiter`)
pilote la consigne de courant de la pile en fonction du SOC batterie. Les seuils de SOC
(très bas 10 %, bas 25 %, haut 70 %, très haut 85 %, très très haut 95 %) sont définis en
constantes en tête de `smb_v1_3_0_main.c`.

### Base de temps

`TMR1` cadence la boucle principale (compteurs en 0,1 s, utilitaire `timer()`), `TMR5` sert
aux PWM. La surveillance des sondes CTN est appelée toutes les 100 ms.

## Compiler

### MPLAB X
Ouvrir le dossier comme projet MPLAB X et lancer *Build* (configuration `default`).
En ligne de commande :

```sh
make build CONF=default
```

Sortie : `dist/default/production/BOXHY_SMB_V1_3_0.X.production.hex`.

### VS Code
Ouvrir le dossier avec l'extension MPLAB installée ; le projet `.vscode/BOXHY_SMB_V1_3_0.mplab.json`
génère les fichiers CMake dans `cmake/` et produit `out/BOXHY_SMB_V1_3_0/default.hex`.

## Remarques

- Les sources sont encodées en ISO-8859-1 avec fins de ligne CRLF (héritage MPLAB X sous Windows).
- `CTN_SENSOR_V03.C` est inclus directement (`#include`) dans le fichier principal ; `IBS_BATTERY_V02.C` est compilé séparément.
- Toute modification de périphérique doit passer par MCC (`test_dspic2.mc3`) puis régénération de `mcc_generated_files/`.

## Auteurs

Julian VERNIER, Neigel MARX, Gael GRZELAK.
