# Spécification d'intégration des batteries PowerTech PowerModule dans la BOXHY

| | |
|---|---|
| Version | 0.9 – brouillon de travail |
| Date | 2026-11-04 |
| Statut | Décidés : topologie, mise sous tension, débit, broches SMB (Q1 à Q5, Q14), bus CAN commun (Q19), plan d'identifiants J1939 de la 2.2.1 (Q25, qui résout Q16), paramètres en flash (Q21, [MEM]), dsPIC33EV128GM106 (Q26), rôles triphasés maître et esclave fixés par paramètre, maître câblé sur le PowerModule Master (Q27, § 6.9). Questions Q6 à Q13, Q15, Q18, Q20, Q22 à Q24, Q28 à Q30 ouvertes (§ 9) |
| Firmware de départ | SMB BOXHY 2.0.0 |
| Composant de référence | dsPIC33EV128GM106 (Q26) |

## 1. Objet

Ce document spécifie le remplacement des batteries IBS de la BOXHY par des batteries PowerTech PowerModule,
du point de vue de la carte SMB. Il couvre :

- le raccordement électrique et la commande de mise sous tension ;
- la communication CAN et les données échangées ;
- les supervisions et la sécurité ;
- la gestion de la charge par la pile à combustible ;
- la télémétrie vers l'IHM ;
- le fonctionnement d'une BOXHY sans batterie (§ 6.8) ;
- la configuration triphasée, où une BOXHY maître, désignée par paramètre, pilote la batterie partagée avec deux BOXHY esclaves (§ 6.9) ;
- le plan d'identifiants CAN des BOXHY sur un bus commun (§ 6.10).

La mémoire persistante (paramètres, journal des défauts) fait l'objet d'un document séparé [MEM].
- les impacts sur le firmware SMB et la manière de les vérifier.

Il ne couvre pas le dimensionnement électrique de la batterie, le câblage de puissance interne ni la
configuration des chargeurs externes.

## 2. Documents de référence

| Réf. | Document |
|---|---|
| [PM] | PowerTech Systems, « PowerModule – Installation et manuel utilisateur », V0.2 du 29/04/26 |
| [PM-DBC] | `can database/POWERMODULE.dbc`, généré par `tools/powermodule_dbc.py` d'après le chapitre 10 de [PM] (configuration BOXHY par défaut : master + 1 slave, 500 kbit/s) |
| [BOXHY-DBC] | `can database/BOXHY.dbc` : trames actuelles de la SMB, de la FCCB, des IBS et de l'IHM |
| [BUGS] | `docs/bugs.md` : anomalies connues du firmware SMB |
| [V221] | `docs/analyse_smb_v2_2_1_triphase.md` : analyse du firmware SMB 2.2.1 triphasé (projets L1, L2, L3 de `assets/old versions/`) |
| [MEM] | `docs/spec/memoire_persistante.md` : zone de flash persistante, paramètres de configuration et journal des défauts |
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

## 4. Le PowerModule

### 4.1 Caractéristiques

Les données ci-dessous sont tirées de [PM].

| Caractéristique | Standard | Solid State |
|---|---|---|
| Tension min / nominale / max | 48,0 / 51,2 / 58,4 V | 48,0 / 51,2 / 58,4 V |
| Capacité nominale | 105 Ah (5,37 kWh) | 106 Ah (5,43 kWh) |
| Décharge continue / pic 30 s | 125 A / 250 A | 200 A / 310 A |
| Charge standard / rapide | 50 A / 100 A | 100 A / 200 A |
| Tension de charge recommandée | 57,0 V | 57,6 V |
| Fusible interne | 250 A | 325 A |

- **Mise sous tension :** deux variantes de commande sur le connecteur M12 OUT.
  - Entrée ON/OFF isolée (broches 1 et 2), alimentée en 5 à 12 V.
  - Option switch : contact sec (broches 9 et 10). Cette entrée n'est pas isolée. [PM] la réserve aux
    assemblages d'un module seul ou de modules en parallèle, sous 60 V.
  - En configuration parallèle, un PowerModule Master est désigné : c'est le seul module fabriqué avec l'option
    switch, et il porte le paramétrage du nombre de modules en parallèle. Mis en marche par le contact, il réveille
    les slaves par la ligne Wake-up de la liaison RJ45 ([PM] § 4 et § 4.1).
- **CAN :** CAN 2.0B isolé, identifiants étendus, données au format Intel. Le débit vaut 250 kbit/s par défaut
  et peut être réglé à 125, 500 kbit/s ou 1 Mbit/s. Il faut une terminaison de 120 Ω à chaque extrémité.
- **Protection autonome :** chaque module coupe son contacteur de puissance interne sur erreur. Après trois
  ouvertures en 15 minutes, il reste ouvert 15 minutes. Un warning est purement informatif.
- **Données système :** le Master, ou le MainMaster en présence de SuperMaster, publie chaque seconde
  le Global Status 1 et le Global Status 2. Un module seul est son propre Master et publie aussi ces trames.
  - Global Status 1 contient le SOC, l'IMR (courant de charge maximal), l'IMD (courant de décharge maximal),
    l'état, ainsi que les indicateurs d'erreur, de warning et de fin de charge.
  - Global Status 2 contient le courant, la tension et l'état de santé.
- **Charge :** chaque charge doit se terminer par « End of charge » pour équilibrer les cellules. [PM] recommande
  que le chargeur suive l'IMR. Le floating se fait à 53,4 V.
- **Froid :** le module passe en erreur au-dessous de 0 °C en charge et de -20 °C en décharge. Un chauffage
  interne se commande par CAN.

### 4.2 Configuration retenue pour la BOXHY

| Sujet | Décision | Réf. |
|---|---|---|
| Nombre de modules | 1 PowerModule ; 2 en parallèle dans certains cas ; 0 sur les BOXHY sans batterie. Le nombre est un paramètre de la SMB. | Q1, PM-CFG-1, § 6.8 |
| Version | Standard. | Q1 |
| Topologie | Master seul ou Master + 1 slave, sans SuperMaster. Un seul PowerModule Master par bus CAN, y compris en triphasé. | Q2, Q27 |
| Batteries IBS | Remplacées définitivement : le firmware ne gère plus que les PowerModule. | Q3 |
| Mise sous tension | Option switch (contact sec). Une clé manuelle et un relais piloté par la SMB sont câblés en parallèle sur le contact. | Q4, § 6.2 |
| Débit CAN | 500 kbit/s : les PowerModule sont configurés à ce débit. Le bus BOXHY reste inchangé. | Q5 |
| Configurations système | BOXHY seule (avec ou sans batterie), BOXHY triphasée maître ou esclave. Le maître est la BOXHY câblée sur le PowerModule Master. Un seul logiciel, configuré uniquement par paramètres stockés en flash [MEM]. | Q17, Q21, Q27, § 6.9 |
| Identifiants CAN | Plan J1939 de la 2.2.1 triphasée, pour toutes les BOXHY. `BOXHY_ID` de 0 à 2 dans l'adresse source. | Q25, § 6.10 |
| Microcontrôleur | dsPIC33EV128GM106. | Q26 |
| Broches SMB | Relais de maintien : broche 46 (RB7). Position de la clé : broche 47 (RC13). Les deux sont reliées au connecteur J24 de la SMB. | Q14, PM-HW-6, PM-HW-7 |

Dimensionnement en décharge, avec un module Standard :

- en régime continu, l'onduleur à 5,2 kVA demande environ 113 à 120 A, soit moins des 125 A continus du
  module ;
- en surcharge, à 7,2 kVA, il demande environ 160 A. Ces pics sont intermittents et le module les accepte.

Ce dimensionnement est validé : aucune exigence supplémentaire n'est nécessaire côté SMB. En charge, le
courant reste borné par l'IMR (PM-CHG-1).

## 5. Points bloquants identifiés

### 5.1 Collision d'identifiants CAN — résolu

Sans SuperMaster, les trames globales du PowerModule utilisent les mêmes identifiants que deux trames de
télémétrie déjà émises par la SMB :

| Identifiant | PowerModule (émetteur : Master) | BOXHY (émetteur : SMB) |
|---|---|---|
| 0x02C00002 | Global Status 1 | `SMB_TX_TEMPERATURE` |
| 0x02C00003 | Global Status 2 | `SMB_TX_CURRENTS` |

Deux émetteurs sur un même identifiant provoquent des erreurs de bus CAN. La SMB ne pourrait pas non plus
distinguer les trames. La vérification a été faite par comparaison de [BOXHY-DBC] avec [PM-DBC], pour des
topologies Master de 1 à 31 modules : seules ces deux collisions existent. **La configuration retenue, avec 1 ou 2 modules et sans SuperMaster, est donc concernée.**

Toutes les trames de la SMB, de la FCCB et de l'IHM en 0x02xxxxxx tombent dans l'espace d'adressage
« batterie » du PowerModule :

- **0x02C0xxxx** est l'espace des trames globales du Master ;
- **0x02AC / 0x02BA / 0x02FC** correspondent à des numéros de module de 22 à 30, inutilisés avec 2 modules.

Une future version du protocole PowerModule pourrait donc utiliser un autre identifiant global déjà pris par
la BOXHY.

Solutions possibles :

1. **Changer les identifiants des deux trames SMB** dans `BOXHY.dbc`, et dans l'IHM. C'est un faible coût côté
   SMB, puisqu'il suffit de modifier le `.dbc` et de régénérer. Cela suppose de mettre à jour l'IHM et les outils.
   Proposition : 0x02C00005 et 0x02C00006, à la suite de `SMB_TX_VERSION`. Il faut aussi faire confirmer par PowerTech
   la liste des commandes globales utilisées ou réservées (Q13).
2. ~~Adopter une topologie avec SuperMaster~~ : solution écartée par Q2.
3. **Mettre les batteries sur un bus CAN séparé.** Le dsPIC33EV de la SMB n'a qu'un contrôleur CAN :
   il faudrait modifier la carte.

Proposition : solution 1. Les nouveaux identifiants sont à valider (Q16).

**Mise à jour v0.6 :** l'identifiant de BOXHY du § 6.10, placé dans les bits 14 et 15 avec une valeur de 1 à 3,
décale toutes les trames de la BOXHY hors de l'espace des PowerModule. Il résout donc aussi cette collision,
sans renommer de trame.

**Mise à jour v0.8 : résolu.** Le plan J1939 retenu (Q25, § 6.10) place toutes les trames de la SMB, de la FCCB
et de l'IHM hors de l'espace 0x02xxxxxx. La collision disparaît (Q16).

### 5.2 Débit CAN — résolu

Le bus BOXHY tourne à 500 kbit/s : FCAN 24 MHz, BRP 1, 12 TQ par bit. Les PowerModule sont configurés à
500 kbit/s (Q5). Le moyen de configuration reste à confirmer avec PowerTech (Q13) :

- la commande « Bit rate CAN bus » 0x20F, dont [PM] ne détaille pas le contenu ;
- ou une configuration en usine.

À 500 kbit/s, le trafic PowerModule de deux modules occupe moins de 1 % du bus.

### 5.3 Commande de mise sous tension — résolu, sortie à définir

Les PowerModule utilisent l'option switch (contact sec, M12 OUT broches 9 et 10). Deux contacts sont câblés en
parallèle sur cette entrée, selon un OU logique :

- **une clé** : voie de démarrage manuelle, purement matérielle, indépendante de la SMB ;
- **un relais piloté par la SMB** : il maintient les batteries en marche lorsque la clé est sur arrêt mais que la
  machine d'états impose de les garder, par exemple pendant une séquence d'arrêt de la pile.

La SMB commande le relais par la broche 46 (RB7) et lit la position de la clé sur la broche 47 (RC13) (Q14).
Ces deux broches sont reliées au connecteur J24 de la SMB, qui reçoit donc le câblage du relais et de la clé.
Point en suspens : les états dans lesquels le maintien est demandé (Q15).

L'entrée switch n'est pas isolée. Cela reste conforme à [PM] pour un module seul ou deux modules en parallèle,
sous 60 V.

## 6. Exigences

Les exigences sont proposées et restent à valider en revue. Identifiants : `PM-<domaine>-<n>`.
Statuts :

- **Décidée** : découle d'une réponse au § 9 ;
- **Proposée** : à valider en revue ;
- **À valider** ou **À définir** : dépend d'une question ouverte.

### 6.1 Topologie, matériel et configuration

| Id | Exigence | Statut |
|---|---|---|
| PM-HW-1 | Lorsqu'elle est présente, la batterie est constituée de 1 ou 2 PowerModule Standard en parallèle (1S1P ou 1S2P, 51,2 V nominal), sans SuperMaster. | Décidée |
| PM-HW-2 | Le bus CAN des PowerModule est raccordé au bus CAN de la SMB, avec une terminaison de 120 Ω à chaque extrémité du bus complet. | Proposée |
| PM-HW-3 | La mise sous tension des PowerModule passe par l'option switch du PowerModule Master, qui réveille les slaves. Le contact de la clé et le contact du relais de maintien SMB sont câblés en parallèle (§ 5.3). | Décidée |
| PM-HW-4 | Un fusible de calibre inférieur à N × fusible interne protège chaque pôle de la batterie, conformément à [PM] § 4. | Proposée |
| PM-HW-5 | Avec 2 modules, les câbles de puissance entre modules et hubs ont tous la même longueur, conformément à [PM] § 4.1. | Proposée |
| PM-HW-6 | Le relais de maintien offre un contact sec, sans liaison galvanique avec la SMB, adapté au signal de l'entrée switch. Sa bobine est commandée par la broche 46 du dsPIC (RB7), reliée au connecteur J24, libre dans le firmware 2.0.0. La broche est déclarée dans MCC en sortie numérique, au niveau bas à l'initialisation, avec l'analogique AN25 désactivé. La commande du relais comporte un pull-down matériel : le relais reste ouvert tant que la broche est en haute impédance, c'est-à-dire du reset jusqu'à l'initialisation des ports. | Décidée |
| PM-HW-7 | La position de la clé est lue sur la broche 47 du dsPIC (RC13), reliée au connecteur J24, libre dans le firmware 2.0.0. La broche est déclarée dans MCC en entrée numérique, avec le pull-down interne activé : une clé ouverte ou débranchée se lit « arrêt ». La clé ramène le niveau haut (5 V) en position marche. L'entrée est protégée contre les surtensions et les décharges électrostatiques (à vérifier sur le schéma de la carte). L'état est filtré, sur le modèle des sélecteurs de mode (500 ms). | Décidée |
| PM-CFG-1 | Le nombre de modules pilotés N (0, 1 ou 2) est un paramètre de configuration de la SMB, `PM_MODULE_COUNT`. N = 0 désigne une BOXHY seule sans batterie (§ 6.8) ou une BOXHY esclave (§ 6.9). | Décidée |
| PM-CFG-2 | Pour N ≥ 1, la SMB vérifie que le Status 1 de chacun des N modules attendus est reçu. Proposition : Master, puis slave 1 si N = 2. Un module manquant ou un module en trop déclenche une alarme. La réaction, défaut ou mode dégradé, suit Q9. | Proposée |
| PM-CFG-3 | Les tables de liaison CAN sont compilées pour 2 modules au plus. Seules les N premières sont exploitées. | Proposée |
| PM-CFG-4 | Un seul logiciel couvre toutes les configurations (§ 6.9). `SMB_ROLE`, `BOXHY_ID` (§ 6.10) et `PM_MODULE_COUNT` sont lus au démarrage depuis la zone de flash persistante [MEM], et non fixés à la compilation. | Décidée |
| PM-CFG-5 | Au démarrage, la SMB vérifie la cohérence des paramètres : `SEULE` impose `BOXHY_ID` = 0 ; `TRI_MAITRE` impose N ≥ 1 et `BOXHY_ID` = 0 ; `TRI_ESCLAVE` impose N = 0 et `BOXHY_ID` = 1 ou 2. Une configuration absente ou incohérente maintient la SMB en défaut, relais en sécurité, et le défaut est journalisé [MEM]. | Proposée |
| PM-CFG-6 | Les paramètres de configuration sont émis en télémétrie, avec la version firmware (PM-TLM-2). | Proposée |

### 6.2 Mise sous tension et maintien

| Id | Exigence | Statut |
|---|---|---|
| PM-PWR-1 | La clé seule démarre les batteries, donc le système, sans action de la SMB. | Décidée |
| PM-PWR-2 | La SMB ferme le relais de maintien tant que la machine d'états exige que les batteries restent en marche, et l'ouvre sinon. Les états concernés sont à définir (Q15). Proposition : tous les états où l'onduleur ou la pile est en marche ou en cours d'arrêt, de `SMB_DCAC_START_1` à `SMB_STOPPING`, ainsi que `SMB_MAINTENANCE`. | À valider |
| PM-PWR-3 | Le passage de la clé sur arrêt déclenche la séquence d'arrêt ordonnée du système : arrêt de la pile s'il y a lieu, puis `SMB_STOPPING`. La SMB ouvre le relais en fin de séquence. La position de la clé est lue sur la broche 47 (PM-HW-7). | À valider |
| PM-PWR-4 | Le maintien après passage de la clé sur arrêt est limité à T_MAINTIEN_MAX. Au-delà, le relais est ouvert même si la séquence d'arrêt n'est pas terminée. Proposition : T_MAINTIEN_MAX = 5 min, à confirmer avec la durée maximale d'arrêt de la pile. | À valider |
| PM-PWR-5 | Au reset de la SMB et dans `SMB_INIT`, la sortie du relais est au repos, contact ouvert. Une SMB bloquée ou redémarrée ne maintient donc pas les batteries. Elle reste sous tension tant que la clé est sur marche. | Proposée |
| PM-PWR-6 | L'état du relais de maintien et la position de la clé sont émis en télémétrie (trame `SMB_TX_RAW1`, 0x02C00001). Le relais est commandable en mode maintenance par une trame `SMB_RX_BATTERY_HOLD_RELAY`, sur le modèle des autres relais (0x02C01D0x). | Proposée |

### 6.3 Communication

| Id | Exigence | Statut |
|---|---|---|
| PM-COM-1 | Aucune trame de [BOXHY-DBC] ne partage un identifiant avec une trame PowerModule de la configuration retenue (§ 5.1). Un test automatique compare [BOXHY-DBC] et [PM-DBC] (cible `candb-check`). | Proposée |
| PM-COM-2 | Les PowerModule et la SMB communiquent à 500 kbit/s (§ 5.2). | Décidée |
| PM-COM-3 | Les trames PowerModule utilisées par la SMB sont décrites dans [BOXHY-DBC] et décodées par la couche CAN générique (`can_signal`), comme les autres trames. | Proposée |
| PM-COM-4 | La SMB lit le Global Status 1, le Global Status 2, ainsi que le Status 1, le Status 2 et l'Error/Warning de chacun des N modules. | Proposée |
| PM-COM-5 | La SMB déclare la batterie perdue si le Global Status 1 n'est pas reçu pendant T_COM. La période d'émission vaut 1 s. Proposition : T_COM = 5 s. | À valider |
| PM-COM-6 | Le keep-alive IBS (0x180AD000 / 0x180AD100) n'est plus émis. [PM] ne demande aucun keep-alive. Les trames IBS sont retirées de [BOXHY-DBC], ainsi que le fichier `IBS.DBF`. | Décidée |

### 6.4 Données et conversions

Convention SMB inchangée : courant en 0,01 A, positif en décharge. Le PowerModule compte positivement le
courant entrant, donc la charge : le signe est inversé.

| Donnée SMB | Source PowerModule | Conversion |
|---|---|---|
| `soc` (%) | Global Status 1, `soc_system` (0,01 %) | division par 100 |
| `output_current` (0,01 A, + décharge) | Global Status 2, `current_system` (0,1 A, + charge) | × (-10) |
| courant par module | Status 1, `current` (0,01 A, + charge) | × (-1) |
| `max_charge_current` | Global Status 1, IMR (0,1 A ou 1 A selon `imr_unit`) | en 0,01 A, signe négatif comme l'IBS |
| `max_discharge_current` | Global Status 1, IMD (1 A ou 10 A selon `imd_unit`) | en 0,01 A |
| température max des cellules | Status 2, max(`temp_pack_1`, `temp_pack_2`) sur les N modules | offset -40 °C |
| erreur batterie | Global Status 1, `error_system` | erreur critique (PM-SAF-1) |
| fin de charge | Global Status 1, `end_of_charge_system` | nouvelle donnée (PM-CHG-2) |
| contacteur fermé | Status 2, `status_power_contactor` | remplace `system_running_flag` |

| Id | Exigence | Statut |
|---|---|---|
| PM-DAT-1 | Les conversions du tableau ci-dessus sont appliquées par les tables de liaison CAN. Le bit d'unité de l'IMR et de l'IMD est traité explicitement : un `.dbc` ne sait pas l'exprimer, voir [PM-DBC]. | Proposée |
| PM-DAT-2 | Le SOP de l'IBS n'a pas d'équivalent. Sa supervision est supprimée ou remplacée (Q6). | À valider |

### 6.5 Supervision et sécurité

| Id | Exigence | Statut |
|---|---|---|
| PM-SAF-1 | `error_system` = 1 ou la perte de communication (PM-COM-5) déclenche l'erreur critique SMB, avec le passage en défaut existant. | Proposée |
| PM-SAF-2 | `warn_system` = 1 est remonté en télémétrie et n'arrête pas le système, conformément à [PM] § 3.5. | Proposée |
| PM-SAF-3 | Les supervisions SMB qui doublonnent les protections internes du PowerModule sont revues, une par une, en Q7 : surchauffe en charge et en décharge, surintensité, surtension du bus. | À valider |
| PM-SAF-4 | Les bugs 7, 9 et 11 de [BUGS], de gravité haute, sont corrigés avant la mise en service avec les PowerModule. | Proposée |
| PM-SAF-5 | L'ouverture du contacteur d'un module, constatée par `status_power_contactor` = 0 hors commande, est traitée selon Q9. | À valider |

### 6.6 Charge par la pile à combustible

| Id | Exigence | Statut |
|---|---|---|
| PM-CHG-1 | En mode hybride, la consigne de courant pile respecte la limite de charge de la batterie : courant pile ≤ courant consommé + IMR. La formule actuelle de `ems.c` (limite × 0,7 × 2) est remplacée, car l'IMR est déjà une valeur système qui tient compte de N. | Proposée |
| PM-CHG-2 | Lorsque `end_of_charge_system` passe à 1, la SMB cesse de charger la batterie. Le comportement, soit la sortie du mode hybride, soit une consigne réduite à la consommation, est à définir (Q10). | À valider |
| PM-CHG-3 | Les seuils de SOC du mode hybride (entrée ≤ 70 %, sortie ≥ 85 %) sont revus pour atteindre régulièrement « End of charge », nécessaire à l'équilibrage selon [PM] § 7.1. | À valider |
| PM-CHG-4 | Aucune charge n'est demandée tant qu'une température de cellule est inférieure à 0 °C. Le chauffage est géré selon Q11. | À valider |

### 6.7 Télémétrie

| Id | Exigence | Statut |
|---|---|---|
| PM-TLM-1 | Les trames `SMB_TX_BAT_GENERAL` et `SMB_TX_BAT_DETAILS`, conçues pour deux IBS, sont redéfinies pour la batterie PowerModule (Q8), et l'IHM est mise à jour en conséquence. Avec 2 modules au plus, le détail par module tient dans les trames existantes. | À valider |
| PM-TLM-2 | La version firmware émise (0x2C00004) permet à l'IHM de savoir quel type de batterie est géré. Le nombre de modules configuré (PM-CFG-1) y est ajouté. | Proposée |

### 6.8 Configuration sans batterie (N = 0)

Certaines BOXHY seules n'auront aucun PowerModule, donc aucune batterie. Ce cas est distinct de la BOXHY esclave
(§ 6.9), qui dispose de la batterie du maître sans la piloter. Le firmware 2.0.0 suppose partout qu'une
batterie est présente :

| Dépendance | Emplacement dans [FW] | Effet si rien n'est fait avec N = 0 |
|---|---|---|
| Perte de communication batterie = erreur critique | `Battery_HasCriticalError()`, `smb_state_machine.c:94` | Mode défaut permanent dès la mise sous tension |
| SOC pour entrer en mode hybride (≤ 70 %) et en sortir (≥ 85 %) | `smb_state_machine.c:217` et `:299` | SOC à 0 : entrée en hybride toujours autorisée, jamais de sortie sur SOC haut |
| Déconnexion de l'onduleur sur SOC très bas | `Inverter_Supervise100ms(battery.soc)` (`main.c:55`) | Onduleur déconnecté dès que la puissance de sortie dépasse le seuil `DCAC_SOC_DISCONNECT_POWER_LEVEL` |
| Bips de SOC bas ou haut, avertissement SOP | `hmi.c:77-91` | Bips intempestifs |
| Courant batterie et limite de charge pour la consigne de courant pile | `Ems_Update100ms`, `Ems_Update1s` (`main.c:59`, `:72`) | Consigne pile fausse |
| Parcours nominal : onduleur d'abord alimenté par la batterie (`SMB_BATTERY_MODE`), puis démarrage de la pile | `smb_state_machine.c` | Aucune source pour démarrer l'onduleur sans batterie : le parcours est à redéfinir |

| Id | Exigence | Statut |
|---|---|---|
| PM-NOB-1 | N = 0 est une valeur valide de `PM_MODULE_COUNT` (PM-CFG-1). Aucune trame PowerModule n'est alors attendue ni émise : pas de watchdog, pas d'erreur batterie, pas de supervision de SOC, de température ni de courant batterie. | Décidée |
| PM-NOB-2 | Avec N = 0, aucune décision de la machine d'états, de l'onduleur, de l'IHM ni de la gestion énergie n'utilise une donnée batterie. Chaque dépendance du tableau ci-dessus reçoit un comportement explicite, défini par Q18. | Proposée |
| PM-NOB-3 | Avec N = 0, la télémétrie batterie (PM-TLM-1) émet une valeur « batterie absente » distincte de toute valeur valide, et le nombre de modules configuré est émis (PM-TLM-2). | Proposée |
| PM-NOB-4 | Avec N = 0, le rôle de la clé (broche 47) et du relais de maintien (broche 46) est défini par Q18. Le relais n'a plus de contact sec de batterie à maintenir. | À définir |
| PM-NOB-5 | Les tests unitaires de la machine d'états, de l'onduleur, de l'IHM et de la gestion énergie sont exécutés pour N = 0, 1 et 2, et pour chaque rôle. | Proposée |

### 6.9 Configuration triphasée : BOXHY maître et esclaves

En fonctionnement triphasé, trois BOXHY partagent le même bus 48 V et le même bus CAN (Q19) :

- la **BOXHY maître** pilote le système batterie : lecture des PowerModule, relais de maintien, supervisions et
  limites de charge et de décharge ;
- les **BOXHY esclaves** se raccordent au bus 48 V du maître. Elles ont accès à la batterie mais ne la
  pilotent pas. Sur le bus CAN commun (§ 6.10), elles peuvent lire les trames PowerModule sans les commander ;
- le maître autorise chaque esclave à fonctionner ou non, et fixe la puissance qu'il peut débiter.

**Les rôles sont fixés par paramètre (Q27).** Sans SuperMaster (Q2), le bus CAN couplé ne peut porter qu'un
seul système PowerModule, donc un seul PowerModule Master. Deux Master émettraient les mêmes identifiants, par
exemple le Global Status 1 en 0x02C00002. Le PowerModule Master est désigné par construction ([PM] § 4) :

- c'est le seul module équipé de l'option switch ;
- il porte le paramétrage du nombre de modules ;
- il réveille les slaves par la ligne Wake-up de la liaison RJ45.

La BOXHY dont la clé et le relais de maintien sont câblés sur ce Master est donc la seule à pouvoir mettre
la batterie en marche et la maintenir : **c'est la BOXHY maître**. Il n'y a ni élection ni reprise du rôle
par une autre BOXHY, car la batterie s'arrête avec le Master.

Le même logiciel gère la BOXHY seule, le maître et l'esclave. Le choix se fait uniquement par paramètres
(Q17), stockés en flash [MEM].

| Configuration | `SMB_ROLE` | `BOXHY_ID` | `PM_MODULE_COUNT` | Batterie |
|---|---|---|---|---|
| BOXHY seule, avec batterie | `SEULE` | 0 | 1 ou 2 | pilotée |
| BOXHY seule, sans batterie | `SEULE` | 0 | 0 | absente (§ 6.8) |
| BOXHY triphasée maître | `TRI_MAITRE` | 0 | 1 ou 2 : modules du système, y compris les slaves éventuellement placés dans une autre BOXHY | pilotée, partagée avec les esclaves |
| BOXHY triphasée esclave | `TRI_ESCLAVE` | 1 ou 2 | 0 | présente sur le bus 48 V, non pilotée |

#### 6.9.1 Trames de coordination

| Trame | Identifiant | Émetteur | Période | Contenu |
|---|---|---|---|---|
| `SMB_TX_TRI_STATUS` | PGN 0xFF97, `0x18FF9710 + ID` : trame d'état de la 2.2.1, étendue | Maître et esclaves | 100 ms | Rôle, état de la machine d'états, puissance prélevée sur le bus 48 V, résumé des défauts actifs, compteur de vie |
| `SMB_TX_TRI_AUTH` | PGN 0xFF98, `0x18FF9810 + ID` : nouvelle | Le maître seul | 100 ms | Maintien batterie en cours, arrêt général demandé. Pour chaque esclave (`BOXHY_ID` 1 et 2) : autorisation de marche et puissance maximale (pas de 100 W). Compteur de vie. |

`SMB_TX_TRI_AUTH` sert aussi de battement de cœur du maître. Les deux trames représentent 60 trames par seconde
pour trois BOXHY, soit moins de 2 % de charge à 500 kbit/s.

| Id | Exigence | Statut |
|---|---|---|
| TRI-1 | Le rôle de la SMB, `SMB_ROLE` (`SEULE`, `TRI_MAITRE` ou `TRI_ESCLAVE`), et l'identifiant de BOXHY, `BOXHY_ID` (§ 6.10), sont des paramètres de configuration stockés en flash [MEM] (PM-CFG-4). | Décidée |
| TRI-2 | La BOXHY maître est celle dont la clé et le relais de maintien sont câblés sur l'option switch du PowerModule Master. Un ensemble triphasé compte un seul maître, un seul PowerModule Master, et aucun SuperMaster. | Décidée |
| TRI-3 | Le maître émet `SMB_TX_TRI_AUTH` toutes les 100 ms. Pour chaque esclave, la trame porte l'ordre de marche ou d'arrêt et la puissance maximale que l'esclave peut prélever sur le bus 48 V. | Décidée (contenu), Proposée (période) |
| TRI-4 | Le maître calcule les autorisations à partir de l'état de la batterie (erreur, SOC, IMD, IMR), de sa propre consommation et de l'état rapporté par chaque esclave. La règle de répartition est définie par Q20. | À définir |
| TRI-5 | Le maître retire toutes les autorisations sur erreur batterie (PM-SAF-1), sur SOC très bas, et avant sa propre séquence d'arrêt. Le relais de maintien du maître reste fermé tant qu'un esclave n'a pas terminé son arrêt (Q15, Q23). | Proposée |
| TRI-6 | Un esclave ne démarre son onduleur que si l'autorisation de marche est reçue. Sans autorisation, il reste en `SMB_STANDBY` ou exécute sa séquence d'arrêt. | Proposée |
| TRI-7 | Un esclave fait respecter la puissance autorisée. La SMB ne peut pas fixer de consigne de puissance à l'onduleur : le protocole RS232 actuel (QMOD, QPIWS, QPIGS) ne fait que l'interroger. Proposition : surveiller la puissance de sortie, et déconnecter l'onduleur par temporisation si l'autorisation est dépassée, sur le modèle de la surveillance de surcharge existante (Q22). | À valider |
| TRI-8 | Un esclave sans autorisation valide pendant T_TRI exécute sa séquence d'arrêt et lève l'alarme « perte du maître ». À la mise sous tension, l'esclave attend le maître en `SMB_STANDBY`, sans alarme pendant T_ATTENTE, puis avec l'alarme « maître absent » (Q30). Proposition : T_TRI = 2 s et T_ATTENTE = 10 s. | Proposée |
| TRI-9 | Le maître et les esclaves émettent `SMB_TX_TRI_STATUS` toutes les 100 ms : rôle, état de la machine d'états, puissance prélevée sur le bus 48 V, erreurs. | Proposée |
| TRI-10 | Chez un esclave, les décisions qui utilisaient le SOC (mode hybride, déconnexion de l'onduleur, bips) utilisent les données transmises par le maître, ou sont désactivées (Q20). | À définir |
| TRI-11 | Erreur de configuration détectée en fonctionnement : un maître qui reçoit `SMB_TX_TRI_AUTH` d'une autre BOXHY, c'est-à-dire deux maîtres, ou un esclave qui reçoit des autorisations de deux BOXHY. La SMB passe en défaut, journalise le code de la famille 0x06 [MEM], ne débite pas et retire ses autorisations. | Proposée |
| TRI-12 | Une SMB qui reçoit, avec sa propre adresse source, une trame d'un PGN qu'elle émet elle-même (télémétrie SMB, `SMB_TX_TRI_STATUS`, `SMB_TX_TRI_AUTH`) en déduit un `BOXHY_ID` en double. Elle passe en défaut (famille 0x06 [MEM]) et ne débite pas. | Proposée |
| TRI-13 | Si des PowerModule slaves sont placés dans une BOXHY esclave, ils sont réveillés par la ligne Wake-up du Master, via la liaison RJ45 entre BOXHY. La clé et le relais de cette BOXHY ne les commandent pas, et son `PM_MODULE_COUNT` vaut 0. Ils sont comptés dans le `PM_MODULE_COUNT` du maître. | Proposée |
| TRI-14 | Le maître et les esclaves communiquent par le bus CAN commun (Q19), avec le plan d'identifiants du § 6.10. | Décidée |

### 6.10 Identifiant de BOXHY et plan d'identifiants CAN

En configuration triphasée, les trois BOXHY partagent le même bus CAN (Q19). Le firmware 2.0.0 n'a pas
d'identifiant de BOXHY : tous les identifiants sont des constantes fixes de [BOXHY-DBC]. Sur un bus commun,
deux conséquences :

- les trois BOXHY émettraient les mêmes trames, que ce soit côté SMB, FCCB ou IHM ;
- chaque équipement obéirait aux commandes des autres BOXHY, par exemple l'ordre de marche de la pile.

**Décision Q25 : le plan d'identifiants de type J1939 du firmware 2.2.1 triphasé [V221] est retenu**, pour toutes
les BOXHY, seules ou triphasées. Le champ sur les bits 14 et 15 proposé en v0.6 est abandonné.

Identifiant 29 bits = priorité (3 bits) | PGN propriétaire 0xFFxx | adresse source (8 bits). L'identifiant de
BOXHY est porté par l'adresse source :

| Équipement | Adresse source | Remarque |
|---|---|---|
| FCCB de la BOXHY n | `0x00 + n` | n est un paramètre propre de la FCCB, dans son EEPROM (ID-4) |
| SMB de la BOXHY n | `0x10 + n` | |
| Commandes de la SMB vers la FCCB n (PGN 0xFF02, 0xFF12, 0xFF22) | `0x00 + n` | Convention de la 2.2.1 conservée : l'adresse est celle de la FCCB destinataire |
| IHM et outil de maintenance vers la SMB n | 0x10 dans la 2.2.1, `0x10 + n` proposé | Q29. Les PGN des commandes IHM (0xFFx7) diffèrent de ceux de la SMB (0xFFx6) : pas de collision. |
| PowerModule | protocole PowerTech | Identifiants 0x02xxxxxx inchangés |

`BOXHY_ID` prend les valeurs 0, 1 ou 2, comme dans la 2.2.1. Une BOXHY seule et le maître triphasé valent 0, les
esclaves 1 et 2. La table complète des
trames est au § 5.2 de [V221].

Les trames se répartissent en trois familles :

| Famille | Exemples | Règle |
|---|---|---|
| Interne à une BOXHY | SMB, FCCB, IHM : télémétrie, commandes, maintenance | Porte l'adresse source de son équipement. Chaque équipement ignore les trames d'une autre BOXHY. |
| Entre BOXHY | `SMB_TX_TRI_STATUS`, `SMB_TX_TRI_AUTH` (§ 6.9.1) | Portent l'adresse source de la SMB émettrice. Chaque esclave ne retient que la partie de l'autorisation qui le concerne. |
| PowerModule | Global Status, Status, commandes | Inchangées, car le protocole est fixé par PowerTech. Le bus ne porte qu'un système batterie, avec un seul PowerModule Master (§ 6.9) : seul le maître le commande, les esclaves peuvent lire ses trames. |

Conséquences vérifiées :

- **Plus de collision avec les PowerModule.** Toutes les trames de la SMB, de la FCCB et de l'IHM quittent
  l'espace 0x02xxxxxx, en priorité 3 (0x0C…) ou 6 (0x18…). La collision du § 5.1 disparaît (Q16). Vérifié
  par comparaison avec [PM-DBC] pour 1 à 31 modules, et pour 16 stacks de 8 modules avec SuperMaster.
- **Filtre de réception.** Le filtre MCC, identique dans la 2.0.0 et la 2.2.1, n'accepte que les identifiants
  dont les bits 24 et 26 valent 0. Il laisse passer la priorité 6 (0x18…) et les PowerModule, mais rejette
  la priorité 3 (0x0C…). Aujourd'hui, seule `SMB_TX_GENERAL` (0x0CFF0610 + ID) est en priorité 3, et la SMB ne
  fait que l'émettre.

| Id | Exigence | Statut |
|---|---|---|
| ID-1 | `BOXHY_ID` (0, 1 ou 2) est un paramètre de configuration de la SMB, stocké en flash [MEM]. Une BOXHY seule et le maître valent 0, les esclaves 1 et 2. Les valeurs sont uniques sur un bus couplé. | Décidée (valeurs), Proposée (affectation) |
| ID-2 | La SMB émet ses trames selon le plan J1939 de [V221], adresse source `0x10 + BOXHY_ID`, et les commandes FCCB en adresse source `0x00 + BOXHY_ID`. Les commandes PowerModule suivent le protocole PowerTech. | Décidée |
| ID-3 | La SMB ignore toute trame interne d'une autre BOXHY : elle compare le PGN et l'adresse source attendue (`0x00 + BOXHY_ID` pour la FCCB, adresse de l'IHM selon Q29). La priorité n'est pas comparée, comme dans la 2.2.1. | Décidée |
| ID-4 | La FCCB tient son `BOXHY_ID` comme un paramètre propre, dans son EEPROM. Elle ne le reçoit pas de la SMB : sur un bus couplé, la SMB ne peut pas savoir quelle FCCB est dans sa BOXHY. L'IHM suit la même règle (Q29). | Décidée (FCCB), À valider (IHM) |
| ID-5 | [BOXHY-DBC] est réécrit selon le plan J1939, avec l'adresse source de `BOXHY_ID` = 0. `tools/candb_gen.py` génère les `.DBF` BUSMASTER pour chaque `BOXHY_ID`. Le test PM-COM-1 vérifie l'absence de collision pour les trois valeurs, entre BOXHY et avec [PM-DBC]. | Proposée |
| ID-6 | La couche CAN générique applique l'identifiant à l'exécution, sans dupliquer les tables en flash. À l'émission, l'adresse source de base reçoit `+ BOXHY_ID`. À la réception, la trame est recherchée sur son PGN, puis l'adresse source est contrôlée. | Proposée |
| ID-7 | La charge du bus commun est évaluée pour trois BOXHY et la batterie à 500 kbit/s. Les périodes d'émission sont ajoutées à [BOXHY-DBC], où elles manquent aujourd'hui (attribut `GenMsgCycleTime`). Proposition : charge inférieure à 40 %. | Proposée |
| ID-8 | Le bus commun est une ligne unique, terminée par 120 Ω à ses deux extrémités seulement. Les terminaisons des BOXHY intermédiaires sont désactivées. | Proposée |
| ID-9 | Toute trame que la SMB doit recevoir est émise en priorité 6 (0x18…). Sinon, le filtre de réception est revu. Le test PM-COM-1 vérifie chaque trame reçue de [BOXHY-DBC] contre le masque et le filtre MCC. | Proposée |
| ID-10 | Mise en service : la SMB et la FCCB d'une BOXHY reçoivent le même `BOXHY_ID`, bus CAN de la BOXHY isolé. La SMB signale alors en maintenance les adresses FCCB qu'elle entend. Une seule doit apparaître, `0x00 + BOXHY_ID`. Une fois le bus couplé, une inversion d'identifiants entre deux FCCB n'est plus détectable par la SMB. | Proposée |

## 7. Impacts sur le firmware SMB

| Élément | Impact |
|---|---|
| `can database/BOXHY.dbc` | Réécriture selon le plan J1939 de [V221], avec `BOXHY_ID` = 0 (ID-5). Ajout des trames PowerModule lues ou émises par la SMB, pour 2 modules, sur le modèle de [PM-DBC]. Ajout de `SMB_TX_TRI_STATUS` et `SMB_TX_TRI_AUTH` (§ 6.9.1), des trames de paramètres et de journal [MEM]. Retrait des trames IBS. Redéfinition de la télémétrie batterie. Nouvelle trame de maintenance du relais de maintien. |
| `tools/candb_gen.py` | `.DBF` BUSMASTER générés pour chaque `BOXHY_ID` (ID-5). Répartition des messages `PM_` dans un `.DBF` BUSMASTER dédié. Suppression de `IBS.DBF`. Contrôles de non-collision avec [PM-DBC] et de passage du filtre de réception (PM-COM-1, ID-9). |
| `src/devices/battery.c/.h` | Remplacement des tables IBS par des tables PowerModule. Suppression du keep-alive. Nouveau watchdog et contrôle de présence des N modules. Nouvelles données : IMR, IMD, fin de charge, contacteur. |
| `src/control/ems.c` | Consigne de courant pile fondée sur l'IMR (PM-CHG-1), prise en compte de la fin de charge. |
| `src/system/smb_state_machine.c` | Erreurs critiques (PM-SAF-1). Fin de charge (PM-CHG-2). Maintien des batteries et arrêt sur clé (PM-PWR-2 à 4). Correction du bug 7. |
| `hal/board_io.h` et son double de test | `Board_SetBatteryHoldRelay` / `Board_GetBatteryHoldRelay` (broche 46) et `Board_GetKeySwitch` (broche 47), au repos au reset (PM-PWR-5). Ajout à `Board_RelaysSafeState` à décider avec Q15. |
| MCC (`test_dspic2.mc3`, `mcc_generated_files/`) | Passage au dsPIC33EV128GM106 (Q26) : régénération MCC, horloge à fixer (la 2.2.1 tourne à 15 MIPS), configuration de build et script de build à mettre à jour. RB7 : `BATTERY_HOLD_RELAY`, sortie numérique, niveau bas, `ANSELB` bit 7 à 0. RC13 : `KEY_SWITCH`, entrée numérique, pull-down (`CNPDC` bit 13). Gestionnaire de trappes adapté aux erreurs ECC de la zone persistante [MEM] (MEM-RT-4). |
| Nouveau module de configuration (`src/system/config.c/.h`) | Lecture des paramètres en flash [MEM] et contrôle de `SMB_ROLE`, `BOXHY_ID` et `PM_MODULE_COUNT` au démarrage (PM-CFG-4, PM-CFG-5). Toutes les décisions dépendantes de la configuration passent par lui. |
| Nouveau module de liaison triphasée | Autorisations du maître, état des SMB, watchdog T_TRI, répartition de la puissance (TRI-3 à TRI-10). Détection de deux maîtres et d'un identifiant en double (TRI-11, TRI-12). |
| `src/can/can_signal.c/.h`, `can_tx.c`, `can_rx.c` | Adresse source `+ BOXHY_ID` à l'émission. Recherche sur le PGN et contrôle de l'adresse source à la réception (ID-2, ID-3, ID-6). |
| Logiciels FCCB et IHM | Plan J1939 de la 2.2.1 pour toutes les BOXHY, y compris seules (Q28). `BOXHY_ID` en paramètre propre de la FCCB (ID-4). Adresse de l'IHM (Q29). |
| `src/smb_config.h` | T_COM, T_MAINTIEN_MAX, T_TRI, T_ATTENTE, seuils de SOC. Suppression des seuils SOP si Q6 le confirme. |
| `src/can/can_tx.c`, `src/hmi/maintenance.c` | Télémétrie batterie (PM-TLM-1), état du relais et de la clé (PM-PWR-6), commande de maintenance du relais. |
| Nouveaux modules `src/system/nvm.c/.h` et `src/system/faults.c/.h` | Accès à la flash persistante, paramètres, journal des défauts, catalogue des codes [MEM]. |
| `tests/` | Suites `test_battery`, `test_ems`, `test_can_tx`, `test_maintenance` et `test_smb_state_machine` à adapter. Nouveaux tests : maintien et clé, présence des N modules, non-collision des identifiants (PM-COM-1). |
| Mémoire | Avec le dsPIC33EV128GM106, la zone de code compte environ 117 Ko une fois la zone persistante réservée (8 pages, 12 Ko, [MEM] § 4). Pour comparaison, la 2.0.0 occupe environ 27 Ko et la 2.2.1 environ 35 Ko. La contrainte de place du 32GM106 disparaît. |

## 8. Vérification

1. **Tests unitaires sur PC.**
   - Décodage de chaque trame PowerModule sur des vecteurs du manuel, dont l'exemple du Global Status 1 de
     [PM] § 10.2.3.3. Conversions et signes. Watchdog et présence des modules pour N = 1 et N = 2. Absence de toute supervision batterie pour N = 0.
   - Consigne pile bornée par l'IMR. Transitions sur erreur et sur fin de charge.
   - Commande du relais de maintien dans chaque état. Arrêt sur clé et temporisation T_MAINTIEN_MAX.
   - Non-collision des identifiants, pour chaque `BOXHY_ID` et avec [PM-DBC].
   - Adresse source `+ BOXHY_ID` appliquée à l'émission. Trames d'une autre BOXHY ignorées à la réception.
   - Passage du filtre de réception pour chaque trame reçue (ID-9).
   - Liaison triphasée simulée sur PC avec trois instances : démarrages décalés, perte et retour du maître, deux maîtres, identifiant en double, configuration incohérente.
2. **Banc CAN.** BUSMASTER, ou un équivalent, chargé avec [BOXHY-DBC] mis à jour. Simulation des trames
   PowerModule, à 500 kbit/s, et contrôle des réactions de la SMB.
3. **Essais sur batterie réelle, avec 1 module puis 2.**
   - Mise sous tension par la clé, lecture des données.
   - Passage de la clé sur arrêt en mode hybride : maintien jusqu'à la fin de l'arrêt de la pile, puis coupure.
   - Charge par la pile jusqu'à « End of charge ».
   - Coupure d'un module, perte du bus CAN, fonctionnement à froid si applicable.
4. **Essais sur une BOXHY sans batterie (N = 0).** Mise sous tension, démarrage et arrêt selon le parcours défini en Q18, absence d'erreur batterie.
5. **Essais triphasés, avec trois BOXHY.**
   - Démarrage du maître, puis des esclaves sur autorisation. Esclaves mis sous tension avant le maître.
   - Avec 2 modules dont un slave dans une BOXHY esclave : réveil par la ligne Wake-up, présence des deux modules.
   - Erreurs de configuration volontaires : deux maîtres, identifiant en double.
   - Respect de la puissance autorisée par chaque esclave.
   - Retrait des autorisations sur erreur batterie et sur SOC très bas.
   - Perte de la liaison : arrêt des esclaves après T_TRI.
   - Aucune interaction croisée : une IHM ne commande que sa BOXHY, une SMB ne commande que sa FCCB.
   - Mesure de la charge du bus commun (ID-7).
   - Arrêt du maître par la clé alors que des esclaves fonctionnent.

## 9. Questions ouvertes

| N° | Question | Proposition | Décision |
|---|---|---|---|
| Q1 | Combien de PowerModule, et quelle version (Standard ou Solid State) ? | Dimensionner selon la puissance onduleur (jusqu'à 7,2 kVA) et l'autonomie visée. | **Version Standard. 1 module, 2 dans certains cas, 0 sur les BOXHY sans batterie et les BOXHY esclaves. Le nombre de modules est un paramètre de la SMB.** Les pics de décharge de l'onduleur sont admis par le module (§ 4.2). |
| Q2 | Faut-il un SuperMaster (précharge, monitoring distant, plus de 16 modules) ? | Non si N ≤ 16 et sans besoin de monitoring distant. | **Non.** |
| Q3 | La SMB doit-elle continuer à gérer les IBS, au choix à la compilation, ou les PowerModule les remplacent-ils définitivement ? | Remplacement définitif, pour préserver la flash. | **Remplacement définitif.** |
| Q4 | Qui commande la mise sous tension ? | | **Option switch (contact sec). Une clé manuelle et un relais SMB en parallèle. Le relais maintient les batteries quand la clé est sur arrêt et que la machine d'états l'exige.** |
| Q5 | Le bus BOXHY reste-t-il à 500 kbit/s, avec des PowerModule reconfigurés, ou passe-t-il à 250 kbit/s ? | Garder 500 kbit/s. | **PowerModule à 500 kbit/s.** |
| Q6 | Par quoi remplacer la supervision du SOP de l'IBS ? | Supprimer l'erreur SOP ; avertissement si l'IMD descend sous la puissance onduleur requise. | |
| Q7 | Quelles supervisions SMB conserver en plus des protections internes du PowerModule (surchauffe, surintensité, surtension) ? | Conserver celles qui protègent le reste du système, comme la surtension du bus ; supprimer les doublons. | |
| Q8 | Quel contenu pour la télémétrie batterie vers l'IHM : données système seulement, ou détail par module ? | Données système, plus le détail des 2 modules au plus : SOC, courant, température max, erreur, warning, contacteur. | |
| Q9 | Comment réagir à l'ouverture du contacteur d'un module, ou à l'absence d'un module attendu : défaut, mode dégradé, simple alarme ? | Avec N = 1 : défaut. Avec N = 2 : alarme, si PowerTech confirme que le Master recalcule l'IMR et l'IMD pour le module restant (à ajouter à Q13). | |
| Q10 | Que fait la SMB à la fin de charge : sortie du mode hybride, ou maintien avec une consigne égale à la consommation ? | | |
| Q11 | La BOXHY doit-elle fonctionner sous 0 °C ? Si oui, qui pilote le chauffage, et avec quelle consigne ? | | |
| Q12 | L'option Lifeline (arrêt d'urgence sur la ligne de communication) est-elle retenue ? | | |
| Q13 | Quelles incohérences de [PM] faut-il faire confirmer par PowerTech ? | Les envoyer en une seule demande : unités de l'IMR et de l'IMD, identifiant cell balancing, DLC de maintenance, contenu de la commande 0x20F et procédure de passage à 500 kbit/s, liste des commandes globales (0x02C00xxx) utilisées ou réservées., recalcul de l'IMR et de l'IMD par le Master quand un module s'ouvre, comportement des slaves quand le Master s'arrête ou que la ligne Wake-up disparaît | |
| Q14 | Quelle sortie de la SMB commande le relais de maintien ? La SMB dispose-t-elle d'une entrée pour lire la position de la clé ? | Entrée clé : nécessaire pour PM-PWR-3. | **Relais : broche 46 (RB7). Clé : broche 47 (RC13).** |
| Q15 | Dans quels états la SMB maintient-elle les batteries ? Que faire en mode défaut (`SMB_EMERGENCY`) ? | Maintien de `SMB_DCAC_START_1` à `SMB_STOPPING` et en `SMB_MAINTENANCE`. En défaut : maintien limité à T_MAINTIEN_MAX pour signaler l'erreur et laisser la pile s'arrêter. | |
| Q16 | Quels nouveaux identifiants pour `SMB_TX_TEMPERATURE` et `SMB_TX_CURRENTS` (§ 5.1) ? | 0x02C00005 et 0x02C00006, sous réserve de la réponse de PowerTech à Q13. **v0.6 :** l'identifiant de BOXHY du § 6.10 (valeurs 1 à 3, bits 14 et 15) résout la collision sans renommer de trame. | **Résolu par le plan J1939 (Q25) : aucune trame SMB n'est renommée dans l'espace PowerModule.** |
| Q17 | Le nombre de modules est-il fixé à la compilation, ou réglable sur le terrain, par commande CAN de maintenance enregistrée en EEPROM ? | À la compilation dans `smb_config.h` : plus simple, sans EEPROM. | **Un seul logiciel pour toutes les configurations (BOXHY seule, triphasée maître ou esclave), réglé uniquement par paramètres.** Les paramètres sont donc lus au démarrage, sur un support défini en Q21. |
| Q18 | Comment fonctionne une BOXHY sans batterie ? (a) Quelle source alimente la SMB, la FCCB et le démarrage de la pile (24 V, précharges) ? (b) Quel parcours de la machine d'états : démarrage de la pile puis de l'onduleur, sans `SMB_BATTERY_MODE` ? (c) La pile suit-elle seule les appels de charge de l'onduleur, et quelle consigne de courant lui envoyer ? (d) Quel rôle pour la clé et le relais de maintien ? (e) Quelle surveillance remplace celle du SOC (tension du bus, par exemple) ? | À définir avec l'architecture électrique de la version sans batterie. | |
| Q19 | Par quelle liaison le maître et les esclaves communiquent-ils ? | Un bus CAN commun aux trois BOXHY est exclu tel quel : SMB, FCCB et IHM de chaque BOXHY émettent les mêmes identifiants. Il faudrait que tous les équipements, FCCB comprise, prennent des identifiants qui dépendent du numéro de BOXHY. Piste proposée : liaison série dédiée entre SMB, sur l'UART2 du dsPIC (l'UART1 sert à l'onduleur), routée sur des broches remappables libres, avec un émetteur-récepteur RS-485 à ajouter sur la carte. Alternative minimale : autorisation de marche en TOR, et puissance autorisée en PWM ou en analogique. | **Bus CAN commun aux BOXHY. Chaque BOXHY reçoit un identifiant, intégré à ses identifiants de trame (§ 6.10).** |
| Q20 | Comment le maître répartit-il la puissance entre lui et les esclaves ? Les piles des esclaves peuvent-elles charger la batterie du maître, et dans quelle limite (IMR) ? Les esclaves ont-ils besoin du SOC, par exemple pour leur mode hybride ? | Répartition égale de l'IMD disponible entre les BOXHY en marche, moins une marge. Charge par les piles des esclaves bornée par une part de l'IMR, transmise dans l'autorisation. | |
| Q21 | Où sont stockés les paramètres de configuration (`SMB_ROLE`, `BOXHY_ID`, `PM_MODULE_COUNT`) ? | Trois options. (a) Straps ou cavaliers sur des broches libres : simple, sans code d'écriture, mais il faut un câblage ou une modification de carte. (b) Page de flash réservée, écrite par une commande CAN de maintenance : sans matériel, mais 1 536 octets de flash en moins. (c) Envoi par l'IHM à chaque démarrage : pas de stockage dans la SMB, mais la SMB dépend de l'IHM pour démarrer. Proposition : (a) si la carte le permet. | **Page de flash réservée, sur le dsPIC33EV128GM106 : zone persistante de 8 pages, préservée à la reprogrammation, qui contient aussi le journal des défauts [MEM].** |
| Q22 | Comment l'esclave fait-il respecter la puissance autorisée, alors que la SMB n'envoie aucune consigne à l'onduleur ? | Surveillance de la puissance de sortie (QPIGS), avertissement sonore, puis déconnexion temporisée de l'onduleur au-delà de l'autorisation. À vérifier : l'onduleur accepte-t-il une commande de limitation de puissance ? | |
| Q23 | Comment s'enchaînent le démarrage et l'arrêt triphasés ? La clé du maître arrête-t-elle tout le système ? Les esclaves ont-ils leur propre clé, et quel rôle ont leur relais de maintien (broche 46), puisqu'ils ne pilotent pas de contact sec de batterie ? | La clé du maître arrête tout : le maître retire les autorisations, attend la fin de l'arrêt des esclaves, puis ouvre son relais de maintien. La clé d'un esclave n'arrête que cet esclave. | |
| Q24 | Qui fait évoluer les logiciels de la FCCB et de l'IHM pour qu'ils prennent en compte le `BOXHY_ID` (ID-4) ? Comment ces équipements reçoivent-ils leur identifiant : paramètre propre, ou valeur transmise par la SMB ? | Même support de configuration que la SMB (Q21) pour chaque équipement. Sinon, la SMB annonce son `BOXHY_ID` au démarrage. | **En partie : la FCCB tient son `BOXHY_ID` comme paramètre propre (EEPROM). La SMB ne peut pas le fournir, car sur un bus couplé elle ne sait pas quelle FCCB est dans sa BOXHY.** Reste : qui fait évoluer les logiciels, et le cas de l'IHM (Q29). |
| Q25 | Quel plan d'identifiants CAN pour l'identifiant de BOXHY : le champ sur les bits 14 et 15 (§ 6.10), ou le plan J1939 de la 2.2.1 triphasée [V221], déjà compris par les FCCB et IHM triphasées ? | Plan J1939 de la 2.2.1 si les FCCB et IHM triphasées doivent rester compatibles ; il faudra alors l'étendre à la BOXHY seule et décrire ces trames dans [BOXHY-DBC]. | **Plan J1939 de la 2.2.1 retenu, pour toutes les BOXHY (§ 6.10).** |
| Q26 | Quel microcontrôleur équipe chaque carte SMB en service ? La 2.2.1 triphasée vise le dsPIC33EV128GM106, avec une horloge à 15 MIPS, et ne tient pas dans le 32GM106 [V221]. Le firmware unique doit-il cibler les deux composants ? | Recenser les cartes. Si les deux composants coexistent, garder un seul code source et deux configurations de build, la HAL absorbant les écarts d'horloge. | **dsPIC33EV128GM106 comme référence.** |
| Q27 | Plusieurs BOXHY d'un même ensemble triphasé peuvent-elles avoir des PowerModule ? Si oui, leurs modules forment-ils un seul système PowerModule (un module Master, les autres en slave, numérotation unique) ? Sans SuperMaster, deux systèmes indépendants ne peuvent pas partager le bus CAN (§ 6.9.1). PowerTech doit aussi confirmer le comportement du système quand les modules d'une BOXHY sont coupés par sa clé, en particulier le module Master. | Un seul système PowerModule par bus couplé. Le maître élu contrôle la présence de la somme des N annoncés par les candidates. Le module Master est placé dans la BOXHY qui est en pratique toujours en service. | **Un seul PowerModule Master par bus couplé, désigné par construction (option switch) et paramétré avec le nombre de modules ([PM] V0.2 § 4). La BOXHY câblée sur ce Master est la BOXHY maître : rôles maître et esclave fixés par paramètre, sans élection.** Reste à confirmer par PowerTech (Q13) : comportement des slaves quand le Master s'arrête. |
| Q28 | Les BOXHY seules en service ont des FCCB et des IHM qui utilisent l'ancien plan d'identifiants (0x02FCxxxx, 0x02C01Dxx). Faut-il les mettre à jour, ou la SMB doit-elle aussi accepter l'ancien plan ? | Un seul plan : mise à jour des FCCB et IHM des BOXHY seules vers les logiciels triphasés, avec `BOXHY_ID` = 0. Deux plans dans la SMB doubleraient les tables CAN et les tests. | |
| Q29 | Quelle adresse source pour l'IHM de chaque BOXHY ? La 2.2.1 utilise 0x10 fixe, et seule la BOXHY d'identifiant 0 accepte ses commandes ([V221], anomalie A5). Y a-t-il une IHM par BOXHY, ou une IHM commune à l'ensemble triphasé ? | Une IHM par BOXHY, en adresse source `0x10 + BOXHY_ID`, avec son `BOXHY_ID` comme paramètre propre. C'est compatible avec la 2.2.1 pour l'identifiant 0. | |
| Q30 | Que fait un esclave quand le maître est absent ou arrêté : arrêt, ou fonctionnement sans batterie selon Q18 ? | Attente en `SMB_STANDBY`, puis alarme « maître absent » (TRI-8), tant que Q18 n'est pas défini. | |

## 10. Historique

| Version | Date | Modification |
|---|---|---|
| 0.1 | 2026-11-04 | Premier brouillon : état des lieux, points bloquants, exigences proposées, questions ouvertes |
| 0.2 | 2026-11-04 | Décisions Q1 à Q5 : 1 ou 2 modules Standard, nombre de modules paramétrable, sans SuperMaster, IBS remplacées, mise sous tension par clé et relais SMB en parallèle sur l'option switch, CAN à 500 kbit/s. Dimensionnement en décharge validé. Collision d'identifiants confirmée pour la configuration retenue. Nouvelles exigences PM-CFG et PM-PWR. Nouvelles questions Q14 à Q17. [PM-DBC] régénéré à 500 kbit/s. |
| 0.3 | 2026-11-04 | Décision Q14 : relais de maintien sur la broche 46 (RB7), position de la clé sur la broche 47 (RC13). Les deux broches sont reliées au connecteur J24. Nouvelle exigence PM-HW-7. Configuration MCC et HAL précisées. |
| 0.4 | 2026-11-04 | Ajout du cas sans batterie (N = 0) : PM-CFG-1 étendu à 0 ≤ N ≤ 2, nouvelle section 6.8 (dépendances du firmware à la batterie, exigences PM-NOB), nouvelle question Q18. |
| 0.5 | 2026-11-04 | Configuration triphasée (§ 6.9) : BOXHY maître qui pilote la batterie partagée, esclaves autorisés par le maître (marche et puissance), exigences TRI. Décision Q17 : un seul logiciel configuré par paramètres (`SMB_ROLE`, `SMB_SLAVE_ID`, `PM_MODULE_COUNT`), exigences PM-CFG-4 à 6. Nouvelles questions Q19 à Q23. Correction : la mention « 0 module » était ajoutée à la mauvaise ligne du § 4.2. |
| 0.6 | 2026-11-04 | Décision Q19 : bus CAN commun aux BOXHY, avec un identifiant de BOXHY dans les trames. Nouvelle section 6.10 : champ `BOXHY_ID` proposé sur les bits 14 et 15, valeurs 1 à 3, sans collision avec les PowerModule ; il résout aussi Q16. Exigences ID-1 à ID-8. `SMB_SLAVE_ID` remplacé par `BOXHY_ID`. Nouvelle question Q24. |
| 0.7 | 2026-11-04 | Référence [V221] : analyse du firmware 2.2.1 triphasé. Note sur son plan d'identifiants J1939 au § 6.10. Nouvelles questions Q25 (plan d'identifiants) et Q26 (microcontrôleur). |
| 0.8 | 2026-11-04 | Décision Q25 : plan d'identifiants J1939 de la 2.2.1 pour toutes les BOXHY, `BOXHY_ID` de 0 à 2 dans l'adresse source. § 6.10 réécrit, champ des bits 14 et 15 abandonné, Q16 résolue. Décision Q26 : dsPIC33EV128GM106 de référence. Décision Q21 : paramètres en flash, spécifiés avec le journal des défauts dans le nouveau document [MEM]. Q24 en partie décidée : `BOXHY_ID` en paramètre propre de la FCCB. Triphasé (§ 6.9) : maître élu au démarrage parmi les BOXHY qui ont une batterie, `SMB_ROLE` réduit à `SEULE` ou `TRIPHASE`, trames de coordination, exigences TRI-1 à TRI-17. PM-CFG-5 réécrite. Nouvelles questions Q27 à Q30. |
| 0.9 | 2026-11-04 | Décision Q27 : un seul PowerModule Master par bus couplé, désigné par construction. La BOXHY câblée sur ce Master est la BOXHY maître : retour aux rôles `SEULE`, `TRI_MAITRE` et `TRI_ESCLAVE` fixés par paramètre, élection abandonnée. Maître et BOXHY seule en `BOXHY_ID` 0, esclaves 1 et 2. § 6.9 réécrit (TRI-1 à TRI-14), PM-CFG-5 et PM-HW-3 mis à jour. Correction du § 4.1 : le Master est un module désigné, qui réveille les slaves par la ligne Wake-up ([PM] V0.2 § 4). |
