# Changelog

Toutes les évolutions notables de ce projet sont documentées dans ce fichier.
Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
respecte le [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

### Ajouté
- Mise sous git du projet (dépôt `boxhy-smb`), `.gitignore` MPLAB X / XC16, README et CHANGELOG.

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
