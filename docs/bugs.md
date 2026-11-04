# Bugs – firmware SMB BOXHY

Liste des anomalies connues du firmware SMB, ouvertes et corrigées. Elle est mise à jour à chaque
découverte et à chaque correction.

## Conventions

- **Numéro** : le bug *n* est repéré dans le code par un commentaire `TODO(bug n)` tant qu'il est ouvert.
  Un numéro n'est jamais réutilisé.
- **Origine** : version dans laquelle le défaut existe. « ≤ 1.3.0 » signifie qu'il est présent dans le firmware
  d'origine, avant la mise sous git.
- **Gravité** :
  - *Haute* : une protection ne fonctionne pas ou une erreur critique peut être ignorée.
  - *Moyenne* : le comportement s'écarte de l'intention, sans risque immédiat.
  - *Faible* : effet limité, cosmétique ou sans impact actuel.
- **Test** : la suite de `tests/` qui fige le comportement. Tant que le bug est ouvert, le test vérifie le
  comportement actuel, défaut compris, et son nom contient `current_behaviour`. Une fois le bug corrigé, il
  vérifie le comportement corrigé.
- Un bug corrigé passe dans la section « Bugs corrigés ». On y précise la version de correction, le commit
  et le détail du correctif. Il n'est jamais supprimé de ce fichier.

## Synthèse

| N° | Titre | Gravité | Statut |
|---|---|---|---|
| 1 | Affectation au lieu d'une comparaison dans l'état STOPPING | Moyenne | Ouvert |
| 2 | Erreur « SOC très très haut » jamais levée, avertissement SOC haut au mauvais seuil | Moyenne | Ouvert |
| 3 | `break` manquant dans la réception des ordres IHM | Faible | Ouvert |
| 4 | Octets de télémétrie tronqués par un cast 8 bits | Faible | Corrigé en 2.0.0 |
| 5 | Températures max IBS calculées avec ×255 au lieu de ×256 | Moyenne | Corrigé en 2.0.0 |
| 6 | Version télémétrie 1.2.7 pour un firmware 1.3.0 | Faible | Corrigé en 2.0.0 |
| 7 | Erreur critique ignorée pendant la déconnexion onduleur sur SOC bas | Haute | Ouvert |
| 8 | Compteurs de watchdog sans protection contre les interruptions | Faible | Ouvert |
| 9 | Protection de surintensité batterie inopérante | Haute | Ouvert |
| 10 | Arrêt sur courant pile bas inopérant | Moyenne | Ouvert |
| 11 | Moyenne de puissance AC fausse au-delà de 6553 VA | Haute | Ouvert |
| 12 | Seuils des sondes CTN modifiés après acquittement du défaut | Moyenne | Ouvert |
| 13 | Nouvelle tentative de démarrage pile sans second délai | Moyenne | Ouvert |
| 14 | État DCAC_STOP inatteignable | Faible | Ouvert |
| 15 | Débordement du tampon de réception RS232 | Haute | Corrigé en 2.0.0 |
| 16 | Octets résiduels dans les commandes FCCB et le keep-alive IBS | Faible | Corrigé en 2.0.0 |
| 17 | Courant IBS décodé avec une mantisse tronquée | Faible | Corrigé en 2.0.0 |

## Bugs ouverts

### Bug 1 – Affectation au lieu d'une comparaison dans l'état STOPPING

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Moyenne |
| Emplacement | `src/system/smb_state_machine.c`, `state_stopping()` |
| Test | `test_smb_state_machine` : `arret_general_sequence_current_behaviour` |

**Description.** Le code d'origine contient `(fcs_step = FCCB_OPERATING)` dans une condition, soit une
affectation au lieu d'une comparaison. La condition est donc toujours vraie.

**Impact.** À l'arrêt général, la commande d'arrêt est envoyée à la FCCB même si la pile ne fonctionne pas.
De plus, l'état FCCB mémorisé est forcé à OPERATING jusqu'à la trame d'état suivante de la FCCB.

**Correction proposée.** Comparer avec `==`. Il faut d'abord décider si l'arrêt doit être envoyé dans tous les
cas, ce qui est le comportement actuel, ou seulement si la pile fonctionne.

### Bug 2 – Erreur « SOC très très haut » jamais levée, avertissement SOC haut au mauvais seuil

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Moyenne |
| Emplacement | `src/devices/battery.c`, `supervise_soc()` |
| Test | `test_battery` : `soc_haut_current_behaviour_erreur_jamais_levee` |

**Description.** Dans l'origine, le compteur de temporisation est déclaré `bool`. Il ne dépasse donc jamais 1 et
n'atteint jamais le seuil de 50. Par ailleurs, les deux tests de niveau portent sur 95 %, alors que
l'avertissement visait probablement 85 % (`BATTERY_SOC_VERY_HIGH_LEVEL`).

**Impact.** Aucune erreur SMB sur un SOC trop haut. L'avertissement SOC haut, avec son bip, n'apparaît qu'au-delà
de 95 %.

**Correction proposée.** Compteur `uint8_t`. Avertissement à 85 %, erreur à 95 % après 5 s. Les seuils sont à
valider.

### Bug 3 – `break` manquant dans la réception des ordres IHM

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Faible |
| Emplacement | `src/can/can_rx.c`, `CanRx_Process()` |
| Test | `test_can_rx` : `current_behaviour_0x2C01009_avec_octet_1_efface_aussi_raz_ihm` |

**Description.** Après le traitement de l'ordre d'acquittement (0x2C01009), l'exécution continue dans le
traitement de 0x25C1000.

**Impact.** Une trame 0x2C01009 dont l'octet 0 vaut 1 efface aussi le drapeau de RAZ des erreurs IHM.

**Correction proposée.** Ajouter le `break`.

### Bug 7 – Erreur critique ignorée pendant la déconnexion onduleur sur SOC bas

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Haute |
| Emplacement | `src/system/smb_state_machine.c`, `state_fcs_start_2()` et `state_fc_stop_1()` |
| Test | `test_smb_state_machine` : `etape_2_current_behaviour_deconnexion_soc_masque_les_transitions` |

**Description.** La protection de SOC bas est testée par un `if` suivi d'un `else if` sur l'erreur critique.
Tant que la protection est active, aucune autre transition n'est évaluée, y compris le passage en défaut.

**Impact.** Dans ces deux états, une erreur critique n'est traitée qu'une fois le SOC remonté. Exemples : une
fuite H2, la perte d'une batterie ou une surtension.

**Correction proposée.** Séparer l'ouverture du relais onduleur (`if` indépendant) et l'évaluation des
transitions, comme dans les autres états.

### Bug 8 – Compteurs de watchdog sans protection contre les interruptions

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Faible |
| Emplacement | `src/devices/battery.c`, `src/devices/fcs.c` (compteurs de watchdog) |
| Test | Aucun |

**Description.** Les compteurs sont modifiés sans `volatile` ni section critique.

**Impact.** Aucun aujourd'hui : ils ne sont manipulés que dans la boucle principale. À reprendre si une
interruption venait à les modifier.

### Bug 9 – Protection de surintensité batterie inopérante

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Haute |
| Emplacement | `src/devices/battery.c`, `supervise_overcurrent()` |
| Test | `test_battery` : `surintensite_avertissement_et_current_behaviour_erreur_jamais_levee` |

**Description.** Le compteur est sur 8 bits (255 au maximum), alors que la temporisation vaut 450 ticks.
Le seuil n'est jamais atteint.

**Impact.** L'avertissement est émis, mais l'erreur de surintensité n'est jamais levée. Seule reste la
protection interne des batteries.

**Correction proposée.** Compteur sur 16 bits (`sTimedFlag16`), et revoir la temporisation de 45 s.

### Bug 10 – Arrêt sur courant pile bas inopérant

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Moyenne |
| Emplacement | `src/devices/fcs.c`, `Fcs_Update100ms()` |
| Test | `test_fcs` : `timeout_courant_bas_current_behaviour_jamais_atteint` |

**Description.** Le compteur est sur 8 bits, alors que la temporisation vaut 600 ticks.

**Impact.** Le mode hybride n'est jamais quitté sur courant pile durablement bas (< 10 A pendant 60 s).

**Correction proposée.** Compteur sur 16 bits.

### Bug 11 – Moyenne de puissance AC fausse au-delà de 6553 VA

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Haute |
| Emplacement | `src/devices/inverter.c`, `Inverter_UpdatePowerAverage100ms()` |
| Test | `test_inverter` : `moyenne_puissance_current_behaviour_somme_16_bits` |

**Description.** Les dix échantillons sont sommés sur 16 bits. La somme déborde dès que la moyenne dépasse
6553 VA. Par exemple, 10 × 7000 VA donne une moyenne de 446 VA.

**Impact.** Une forte surcharge peut faire chuter la moyenne calculée. La protection de surcharge critique
(7200 VA) ne se déclenche alors pas.

**Correction proposée.** Somme sur 32 bits.

### Bug 12 – Seuils des sondes CTN modifiés après acquittement du défaut

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Moyenne |
| Emplacement | `src/measures/measures.c`, `Measures_ResetSensorErrors()` |
| Test | `test_measures` : `reset_sensor_errors_current_behaviour_reapplique_les_seuils_module` |

**Description.** L'acquittement appelle `ctnSensorInit()`, qui applique les seuils par défaut du module CTN
(70 °C / -5 °C) au lieu des seuils applicatifs. Ceux-ci valent 75 °C / 0 °C pour l'ambiance et 90 °C / 0 °C
pour la sonde 2.

**Impact.** Après le premier acquittement, la sonde 2 passe en erreur à 70 °C au lieu de 90 °C, et la sonde
d'ambiance à 70 °C au lieu de 75 °C.

**Correction proposée.** Effacer seulement les drapeaux et compteurs d'erreur, ou réappliquer les seuils de
`smb_config.h`.

### Bug 13 – Nouvelle tentative de démarrage pile sans second délai

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Moyenne |
| Emplacement | `src/system/smb_state_machine.c`, `state_fcs_start_2()` |
| Test | `test_smb_state_machine` : `demarrage_pile_etape_2_timeout_puis_retry_puis_urgence` |

**Description.** Après une première expiration du délai de 60 s, la SMB relance le démarrage. Mais dès le
retour en FCS_START_2, tout état FCCB autre que OPERATING envoie en défaut.

**Impact.** La seconde tentative échoue presque toujours, car la pile n'a pas le temps de démarrer.

**Correction proposée.** N'aller en défaut qu'à la seconde expiration du délai.

### Bug 14 – État DCAC_STOP inatteignable

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Gravité | Faible |
| Emplacement | `src/system/smb_state_machine.c`, `state_battery_mode()` et `state_dcac_stop()` |
| Test | Aucun (état non atteignable) |

**Description.** La seule branche qui menait à DCAC_STOP reprenait la condition déjà traitée par STOPPING.
Elle n'est jamais exécutée.

**Impact.** Code mort. L'arrêt de l'onduleur seul n'existe pas en pratique.

**Correction proposée.** Supprimer l'état, ou définir sa condition d'entrée.

## Bugs corrigés

### Bug 4 – Octets de télémétrie tronqués par un cast 8 bits

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Corrigé en | 2.0.0 (commit 472a8eb « CAN générique piloté par une base .dbc ») |
| Test | `test_can_tx` : `trame_0x2C00004_version`, `trame_0xCFF0127_gestion_energie` |

**Description.** `(uint8_t) x >> 8` et `(uint8_t) x / 100` appliquaient le cast avant l'opération. L'octet 4
de 0x2C00004 (poids fort du compteur buzzer 5 min) valait toujours 0. L'octet 2 de 0xCFF0127 valait
`(x mod 256) / 100`.

**Correctif.** Les trames sont codées par la couche générique à partir de `BOXHY.dbc`. Le compteur buzzer est un
signal de 16 bits. La consigne de courant pile est un signal en ampères, converti par division entière avant
d'être tronqué à 8 bits. Exemple : 5500 (0,01 A) donne 55.

### Bug 5 – Températures max IBS calculées avec ×255 au lieu de ×256

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Corrigé en | 2.0.0 (commit 472a8eb) |
| Test | `test_battery` : `trame_temperatures_cellules`, `trame_temperature_carte` |

**Description.** Le poids fort des mots de température max (cellules et carte) était multiplié par 255.
Par exemple, 40,0 °C était lu 39 °C.

**Correctif.** Ces signaux sont décrits dans `BOXHY.dbc` comme des mots de 16 bits big-endian signés en 0,1 °C,
et décodés par la couche générique.

### Bug 6 – Version télémétrie 1.2.7 pour un firmware 1.3.0

| | |
|---|---|
| Origine | 1.3.0 |
| Corrigé en | 2.0.0 (commit « Passage en version 2.0.0 ») |
| Test | `test_can_tx` : `trame_0x2C00004_version` |

**Description.** Les constantes de version émises dans la trame 0x2C00004 n'avaient pas suivi le numéro du
firmware.

**Correctif.** `SMB_VERSION`, `SMB_SUBVERSION` et `SMB_SUBSUBVERSION` (`src/smb_config.h`) valent 2.0.0. Le
commentaire rappelle de les mettre à jour à chaque version, en même temps que le CHANGELOG.

### Bug 15 – Débordement du tampon de réception RS232

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Corrigé en | 2.0.0 (commit 428c822 « Découpage du firmware SMB en modules… ») |
| Test | `test_inverter` : `reception_longue_sans_fin_ne_deborde_pas` |

**Description.** L'interruption UART écrivait chaque octet reçu dans un tableau de 120 octets sans vérifier
l'indice. Une réponse longue, sans terminateur ou bruitée, écrasait la RAM voisine.

**Correctif.** L'écriture est bornée à la taille du tableau dans `Inverter_OnRxByte()`. Le comptage des octets,
qui déclenche la fin de trame, est inchangé.

### Bug 16 – Octets résiduels dans les commandes FCCB et le keep-alive IBS

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Corrigé en | 2.0.0 (commits 428c822 puis 472a8eb) |
| Test | `test_fcs` : `commandes` ; `test_battery` : `keep_alive_alterne_les_ids` |

**Description.** Les commandes vers la FCCB et le keep-alive IBS partageaient le tampon des trames émises et
reçues. Les octets non renseignés contenaient les restes de la trame précédente. C'était par exemple le cas
des octets 4 à 6 des commandes longues et de l'octet 0 du keep-alive.

**Correctif.** Chaque trame est codée dans un tampon remis à zéro par `CanMsg_Pack()`. Les octets non utilisés
valent 0.

### Bug 17 – Courant IBS décodé avec une mantisse tronquée

| | |
|---|---|
| Origine | ≤ 1.3.0 |
| Corrigé en | 2.0.0 (commit 472a8eb) |
| Test | `test_battery` : `decode_courant_ibs_mantisse_complete` |

**Description.** Le courant et les limites de courant IBS, des flottants IEEE 754 big-endian, étaient décodés
bit par bit en ignorant les 12 bits de poids faible de la mantisse. L'erreur atteignait quelques 0,01 A.
Exemple : -99,9999 A était lu 99,96 A au lieu de 99,99 A.

**Correctif.** Ce sont des signaux `float32` dans `BOXHY.dbc`, décodés entièrement par
`CanSignal_GetFloat()`. Cela supprime aussi l'appel à `pow()`.
