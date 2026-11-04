# Spécification d'intégration des batteries PowerTech PowerModule dans la BOXHY

| | |
|---|---|
| Version | 0.1 – brouillon de travail |
| Date | 2026-11-04 |
| Statut | En cours de rédaction : exigences proposées, questions ouvertes à trancher (§ 9) |
| Firmware de départ | SMB BOXHY 2.0.0 |

## 1. Objet

Ce document spécifie le remplacement des batteries IBS de la BOXHY par des batteries PowerTech PowerModule,
du point de vue de la carte SMB. Il couvre :

- le raccordement électrique et la commande de mise sous tension ;
- la communication CAN et les données échangées ;
- les supervisions et la sécurité ;
- la gestion de la charge par la pile à combustible ;
- la télémétrie vers l'IHM ;
- les impacts sur le firmware SMB et la manière de les vérifier.

Il ne couvre pas le dimensionnement électrique de la batterie, le câblage de puissance interne ni la
configuration des chargeurs externes.

## 2. Documents de référence

| Réf. | Document |
|---|---|
| [PM] | PowerTech Systems, « PowerModule – Installation et manuel utilisateur », V0.2 du 29/04/26 |
| [PM-DBC] | `can database/POWERMODULE.dbc`, généré par `tools/powermodule_dbc.py` d'après le chapitre 10 de [PM] |
| [BOXHY-DBC] | `can database/BOXHY.dbc` : trames actuelles de la SMB, de la FCCB, des IBS et de l'IHM |
| [BUGS] | `docs/bugs.md` : anomalies connues du firmware SMB |
| [FW] | Firmware SMB 2.0.0, en particulier `src/devices/battery.c`, `src/control/ems.c` et `src/system/smb_state_machine.c` |

## 3. Situation actuelle : batteries IBS

### 3.1 Architecture

La BOXHY utilise deux batteries IBS en parallèle. Leurs données sont lues sur le bus CAN unique de la SMB,
qui le partage avec la FCCB et l'IHM. La SMB envoie chaque seconde un keep-alive à chaque batterie, puis agrège
les données des deux packs.

### 3.2 Données batterie utilisées par la SMB

| Donnée SMB (`battery.*`) | Source IBS | Utilisation dans la SMB |
|---|---|---|
| `soc` (%) | moyenne des SOC des deux packs | Entrée et sortie du mode hybride (≤ 70 % / ≥ 85 %), alarmes SOC bas et haut, bips, déconnexion de l'onduleur sur SOC bas, télémétrie |
| `output_current` (0,01 A, positif en décharge) | somme des courants des deux packs | Supervision de surchauffe (charge ou décharge), bip SOC bas, moyenne pour la gestion énergie, télémétrie |
| `ibs[n].max_cell_temperature` (°C) | température max des cellules par pack | Surchauffe en charge (> 45 °C) et en décharge (> 55 °C) |
| `ibs[n].sop` (%) | état de puissance par pack | Avertissement (< 30 %) et erreur critique (< 20 %) |
| `ibs[n].max_charge_current` (0,01 A) | limite de charge par pack | Consigne de courant pile (`ems.c`), surintensité en charge |
| `ibs[n].error_flag`, `ibs[n].watchdog` | état et présence de chaque pack | Erreur critique : passage en défaut |
| autres (température moyenne et carte, drapeaux running / balancing / SOC bas, limite de décharge) | par pack | Télémétrie uniquement |

## 4. Le PowerModule en bref

Les données ci-dessous sont tirées de [PM].

| Caractéristique | Standard | Solid State |
|---|---|---|
| Tension min / nominale / max | 48,0 / 51,2 / 58,4 V | 48,0 / 51,2 / 58,4 V |
| Capacité nominale | 105 Ah (5,37 kWh) | 106 Ah (5,43 kWh) |
| Décharge continue / pic 30 s | 125 A / 250 A | 200 A / 310 A |
| Charge standard / rapide | 50 A / 100 A | 100 A / 200 A |
| Tension de charge recommandée | 57,0 V | 57,6 V |
| Fusible interne | 250 A | 325 A |

- **Mise sous tension :** entrée ON/OFF isolée sur le connecteur M12 OUT (broches 1 et 2), alimentée en 5 à 12 V.
  En configuration parallèle, le module qui reçoit l'ordre devient Master et met en marche les autres.
- **CAN :** CAN 2.0B isolé, identifiants étendus, données au format Intel. Le débit vaut 250 kbit/s par défaut
  et peut être réglé à 125, 500 kbit/s ou 1 Mbit/s. Il faut une terminaison de 120 Ω à chaque extrémité.
- **Protection autonome :** chaque module coupe son contacteur de puissance interne sur erreur. Après trois
  ouvertures en 15 minutes, il reste ouvert 15 minutes. Un warning est purement informatif.
- **Données système :** le Master, ou le MainMaster en présence de SuperMaster, publie chaque seconde
  le Global Status 1 et le Global Status 2.
  - Global Status 1 contient le SOC, l'IMR (courant de charge maximal), l'IMD (courant de décharge maximal),
    l'état, ainsi que les indicateurs d'erreur, de warning et de fin de charge.
  - Global Status 2 contient le courant, la tension et l'état de santé.
- **Charge :** chaque charge doit se terminer par « End of charge » pour équilibrer les cellules. [PM] recommande
  que le chargeur suive l'IMR. Le floating se fait à 53,4 V.
- **Froid :** le module passe en erreur au-dessous de 0 °C en charge et de -20 °C en décharge. Un chauffage
  interne se commande par CAN.

## 5. Points bloquants identifiés

### 5.1 Collision d'identifiants CAN

En configuration Master, sans SuperMaster, les trames globales du PowerModule utilisent les mêmes
identifiants que deux trames de télémétrie déjà émises par la SMB :

| Identifiant | PowerModule (émetteur : Master) | BOXHY (émetteur : SMB) |
|---|---|---|
| 0x02C00002 | Global Status 1 | `SMB_TX_TEMPERATURE` |
| 0x02C00003 | Global Status 2 | `SMB_TX_CURRENTS` |

Deux émetteurs sur un même identifiant provoquent des erreurs de bus CAN. La SMB ne pourrait pas non plus
distinguer les trames. La vérification a été faite par comparaison de [BOXHY-DBC] avec [PM-DBC], pour des
topologies Master de 2 à 31 modules : seules ces deux collisions existent. Avec SuperMaster, jusqu'à
16 stacks de 8 modules, il n'y en a aucune, car les messages globaux sont alors en 0x024000xx.

Solutions possibles :

1. **Changer les identifiants des deux trames SMB** dans `BOXHY.dbc`, et dans l'IHM. C'est un faible coût côté
   SMB, puisqu'il suffit de modifier le `.dbc` et de régénérer. Cela suppose de mettre à jour l'IHM et les outils.
2. **Adopter une topologie avec SuperMaster.** Cela ajoute du matériel, mais permet la précharge et le
   monitoring distant.
3. **Mettre les batteries sur un bus CAN séparé.** Le dsPIC33EV32GM106 de la SMB n'a qu'un contrôleur CAN :
   il faudrait modifier la carte.

Proposition : solution 1, sauf si la question Q2 conduit à retenir un SuperMaster.

### 5.2 Débit CAN

D'après la configuration MCC, le bus BOXHY tourne à 500 kbit/s : FCAN 24 MHz, BRP 1, 12 TQ par bit. Les
PowerModule sont à 250 kbit/s par défaut. Il faut donc reconfigurer les modules à 500 kbit/s, avec la commande
« Bit rate CAN bus » 0x20F, dont [PM] ne détaille pas le contenu, ou les commander ainsi à PowerTech.
L'autre option, passer tout le bus BOXHY à 250 kbit/s, impose de vérifier la charge du bus et la FCCB.

### 5.3 Commande de mise sous tension

La SMB n'a pas de sortie libre identifiée dans la configuration MCC pour fournir les 5 à 12 V isolés de
l'entrée ON/OFF. Trois pistes : un interrupteur en façade, une sortie de la SMB à ajouter, ou une alimentation
permanente de l'entrée. Voir Q4.

## 6. Exigences

Les exigences sont proposées et restent à valider en revue. Identifiants : `PM-<domaine>-<n>`.

### 6.1 Topologie et matériel

| Id | Exigence | Statut |
|---|---|---|
| PM-HW-1 | La batterie est constituée de N PowerModule en parallèle (1SxP, 51,2 V nominal), N et la version Standard ou Solid State étant à définir (Q1). | À valider |
| PM-HW-2 | Le bus CAN des PowerModule est raccordé au bus CAN de la SMB, avec une terminaison de 120 Ω à chaque extrémité du bus complet. | Proposée |
| PM-HW-3 | La mise sous tension des PowerModule est commandée selon la décision Q4. | À définir |
| PM-HW-4 | Un fusible de calibre inférieur à N × fusible interne protège chaque pôle de la batterie, conformément à [PM] § 4. | Proposée |
| PM-HW-5 | Les câbles de puissance entre modules et hubs ont tous la même longueur, conformément à [PM] § 4.1. | Proposée |

### 6.2 Communication

| Id | Exigence | Statut |
|---|---|---|
| PM-COM-1 | Aucune trame émise par la SMB ne partage un identifiant avec une trame PowerModule (§ 5.1). Un test automatique compare [BOXHY-DBC] et [PM-DBC]. | Proposée |
| PM-COM-2 | Les PowerModule et la SMB communiquent au même débit (§ 5.2). | À valider |
| PM-COM-3 | Les trames PowerModule utilisées par la SMB sont décrites dans [BOXHY-DBC] et décodées par la couche CAN générique (`can_signal`), comme les autres trames. | Proposée |
| PM-COM-4 | La SMB lit au minimum le Global Status 1, le Global Status 2 et l'Error/Warning de chaque module. Status 1 et 2 de chaque module sont lus si la télémétrie par module est conservée (Q8). | Proposée |
| PM-COM-5 | La SMB déclare la batterie perdue si le Global Status 1 n'est pas reçu pendant T_COM. La période d'émission vaut 1 s. Proposition : T_COM = 5 s. | À valider |
| PM-COM-6 | Le keep-alive IBS (0x180AD000 / 0x180AD100) n'est plus émis. [PM] ne demande aucun keep-alive. | Proposée |

### 6.3 Données et conversions

Convention SMB inchangée : courant en 0,01 A, positif en décharge. Le PowerModule compte positivement le
courant entrant, donc la charge : le signe est inversé.

| Donnée SMB | Source PowerModule | Conversion |
|---|---|---|
| `soc` (%) | Global Status 1, `soc_system` (0,01 %) | division par 100 |
| `output_current` (0,01 A, + décharge) | Global Status 2, `current_system` (0,1 A, + charge) | × (-10) |
| courant par module | Status 1, `current` (0,01 A, + charge) | × (-1) |
| `max_charge_current` | Global Status 1, IMR (0,1 A ou 1 A selon `imr_unit`) | en 0,01 A, signe négatif comme l'IBS |
| `max_discharge_current` | Global Status 1, IMD (1 A ou 10 A selon `imd_unit`) | en 0,01 A |
| température max des cellules | Status 2, max(`temp_pack_1`, `temp_pack_2`) sur tous les modules | offset -40 °C |
| erreur batterie | Global Status 1, `error_system` | erreur critique (PM-SAF-1) |
| fin de charge | Global Status 1, `end_of_charge_system` | nouvelle donnée (PM-CHG-2) |
| contacteur fermé | Status 2, `status_power_contactor` | remplace `system_running_flag` |

| Id | Exigence | Statut |
|---|---|---|
| PM-DAT-1 | Les conversions du tableau ci-dessus sont appliquées par les tables de liaison CAN. Le bit d'unité de l'IMR et de l'IMD est traité explicitement : un `.dbc` ne sait pas l'exprimer, voir [PM-DBC]. | Proposée |
| PM-DAT-2 | Le SOP de l'IBS n'a pas d'équivalent. Sa supervision est supprimée ou remplacée (Q6). | À valider |

### 6.4 Supervision et sécurité

| Id | Exigence | Statut |
|---|---|---|
| PM-SAF-1 | `error_system` = 1 ou la perte de communication (PM-COM-5) déclenche l'erreur critique SMB, avec le passage en défaut existant. | Proposée |
| PM-SAF-2 | `warn_system` = 1 est remonté en télémétrie et n'arrête pas le système, conformément à [PM] § 3.5. | Proposée |
| PM-SAF-3 | Les supervisions SMB qui doublonnent les protections internes du PowerModule sont revues, une par une, en Q7 : surchauffe en charge et en décharge, surintensité, surtension du bus. | À valider |
| PM-SAF-4 | Les bugs 7, 9 et 11 de [BUGS], de gravité haute, sont corrigés avant la mise en service avec les PowerModule. | Proposée |
| PM-SAF-5 | L'ouverture du contacteur d'un module, constatée par `status_power_contactor` = 0 hors commande, est traitée selon Q9. | À valider |

### 6.5 Charge par la pile à combustible

| Id | Exigence | Statut |
|---|---|---|
| PM-CHG-1 | En mode hybride, la consigne de courant pile respecte la limite de charge de la batterie : courant pile ≤ courant consommé + IMR. La formule actuelle de `ems.c` (limite × 0,7 × 2) est remplacée, car l'IMR est déjà une valeur système. | Proposée |
| PM-CHG-2 | Lorsque `end_of_charge_system` passe à 1, la SMB cesse de charger la batterie. Le comportement, soit la sortie du mode hybride, soit une consigne réduite à la consommation, est à définir (Q10). | À valider |
| PM-CHG-3 | Les seuils de SOC du mode hybride (entrée ≤ 70 %, sortie ≥ 85 %) sont revus pour atteindre régulièrement « End of charge », nécessaire à l'équilibrage selon [PM] § 7.1. | À valider |
| PM-CHG-4 | Aucune charge n'est demandée tant qu'une température de cellule est inférieure à 0 °C. Le chauffage est géré selon Q11. | À valider |

### 6.6 Télémétrie

| Id | Exigence | Statut |
|---|---|---|
| PM-TLM-1 | Les trames `SMB_TX_BAT_GENERAL` et `SMB_TX_BAT_DETAILS`, conçues pour deux IBS, sont redéfinies pour la batterie PowerModule (Q8), et l'IHM est mise à jour en conséquence. | À valider |
| PM-TLM-2 | La version firmware émise (0x2C00004) permet à l'IHM de savoir quel type de batterie est géré. | Proposée |

## 7. Impacts sur le firmware SMB

| Élément | Impact |
|---|---|
| `can database/BOXHY.dbc` | Ajout des trames PowerModule lues ou émises par la SMB, sur le modèle de [PM-DBC]. Changement d'identifiant des deux trames en collision (§ 5.1). Redéfinition de la télémétrie batterie. |
| `src/devices/battery.c/.h` | Remplacement des tables IBS par des tables PowerModule, suppression du keep-alive, nouveau watchdog, nouvelles données (IMR, IMD, fin de charge, contacteur). |
| `src/control/ems.c` | Consigne de courant pile fondée sur l'IMR (PM-CHG-1), prise en compte de la fin de charge. |
| `src/system/smb_state_machine.c` | Erreurs critiques (PM-SAF-1), fin de charge (PM-CHG-2), correction du bug 7. |
| `src/smb_config.h` | Nouveaux seuils et temporisations (T_COM, SOC). Suppression des seuils SOP si Q6 le confirme. |
| `src/can/can_tx.c` | Télémétrie batterie (PM-TLM-1). |
| `tests/` | Suites `test_battery`, `test_ems`, `test_can_tx` et `test_smb_state_machine` à adapter. Nouveau test de non-collision des identifiants (PM-COM-1). |
| Mémoire | Flash utilisée à 81 %, 5 976 octets libres. Garder en même temps la gestion IBS et la gestion PowerModule (Q3) est à évaluer. |

## 8. Vérification

1. **Tests unitaires sur PC.** Décodage de chaque trame PowerModule sur des vecteurs du manuel, dont l'exemple
   du Global Status 1 de [PM] § 10.2.3.3. Conversions et signes. Watchdog. Consigne pile bornée par l'IMR.
   Transitions sur erreur et sur fin de charge. Non-collision des identifiants.
2. **Banc CAN.** BUSMASTER, ou un équivalent, chargé avec [BOXHY-DBC] mis à jour. Simulation des trames
   PowerModule et contrôle des réactions de la SMB.
3. **Essais sur batterie réelle.** Mise sous tension, lecture des données, charge par la pile jusqu'à
   « End of charge », coupure d'un module, perte du bus CAN, fonctionnement à froid si applicable.

## 9. Questions ouvertes

| N° | Question | Proposition | Décision |
|---|---|---|---|
| Q1 | Combien de PowerModule, et quelle version (Standard ou Solid State) ? | Dimensionner selon la puissance onduleur (jusqu'à 7,2 kVA) et l'autonomie visée. | |
| Q2 | Faut-il un SuperMaster (précharge, monitoring distant, plus de 16 modules) ? | Non si N ≤ 16 et sans besoin de monitoring distant. | |
| Q3 | La SMB doit-elle continuer à gérer les IBS, au choix à la compilation, ou les PowerModule les remplacent-ils définitivement ? | Remplacement définitif, pour préserver la flash. | |
| Q4 | Qui commande la mise sous tension (ON/OFF isolé 5 à 12 V) : interrupteur en façade, sortie SMB à ajouter, alimentation permanente ? | | |
| Q5 | Le bus BOXHY reste-t-il à 500 kbit/s, avec des PowerModule reconfigurés, ou passe-t-il à 250 kbit/s ? | Garder 500 kbit/s. | |
| Q6 | Par quoi remplacer la supervision du SOP de l'IBS ? | Supprimer l'erreur SOP ; avertissement si l'IMD descend sous la puissance onduleur requise. | |
| Q7 | Quelles supervisions SMB conserver en plus des protections internes du PowerModule (surchauffe, surintensité, surtension) ? | Conserver celles qui protègent le reste du système, comme la surtension du bus ; supprimer les doublons. | |
| Q8 | Quel contenu pour la télémétrie batterie vers l'IHM : données système seulement, ou détail par module ? | Données système, plus un indicateur d'erreur et de warning par module. | |
| Q9 | Comment réagir à l'ouverture du contacteur d'un module : défaut, mode dégradé, simple alarme ? | | |
| Q10 | Que fait la SMB à la fin de charge : sortie du mode hybride, ou maintien avec une consigne égale à la consommation ? | | |
| Q11 | La BOXHY doit-elle fonctionner sous 0 °C ? Si oui, qui pilote le chauffage, et avec quelle consigne ? | | |
| Q12 | L'option Lifeline (arrêt d'urgence sur la ligne de communication) est-elle retenue ? | | |
| Q13 | Quelles incohérences de [PM] faut-il faire confirmer par PowerTech (unités de l'IMR et de l'IMD, identifiant cell balancing, DLC de maintenance, contenu de la commande 0x20F) ? | Les envoyer en une seule demande. | |

## 10. Historique

| Version | Date | Modification |
|---|---|---|
| 0.1 | 2026-11-04 | Premier brouillon : état des lieux, points bloquants, exigences proposées, questions ouvertes |
