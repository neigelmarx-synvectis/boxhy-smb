# Spécification de la mémoire persistante de la SMB (paramètres et journal des défauts)

| | |
|---|---|
| Version | 0.1 – brouillon de travail |
| Date | 2026-11-04 |
| Statut | Plan mémoire, formats et règles d'écriture proposés. Questions MEM-Q1 à MEM-Q6 ouvertes (§ 9) |
| Composant de référence | dsPIC33EV128GM106 (décision Q26 de [SPEC-PM]) |

## 1. Objet

Le dsPIC33EV n'a pas d'EEPROM de données. Ce document définit une zone de la flash programme qui sert de
mémoire persistante à la SMB et précise :

- son emplacement et la manière de la protéger lors de la reprogrammation du firmware ;
- le stockage des **paramètres de configuration** (`BOXHY_ID`, `SMB_ROLE`, `PM_MODULE_COUNT`…), décision Q21 de [SPEC-PM] ;
- le **journal des derniers défauts actifs**, en buffer tournant, dimensionné pour l'endurance de la flash ;
- les contraintes temps réel de l'écriture en flash et les moyens de lire ou d'écrire cette mémoire.

## 2. Documents de référence

| Réf. | Document |
|---|---|
| [DS] | Microchip DS70005144H, « dsPIC33EVXXXGM00X/10X Family Data Sheet » : § 5 (Flash Program Memory) et tableau 30-14 |
| [FRM] | Microchip DS70000609, « dsPIC33/PIC24 FRM – Flash Programming » |
| [GLD] | `p33EV128GM106.gld`, DFP dsPIC33EV-GM-00X-10X 1.3.79 |
| [SPEC-PM] | `docs/spec/integration_batteries_powertech.md` : paramètres de configuration, triphasé, plan d'identifiants CAN |

## 3. Caractéristiques de la flash du dsPIC33EV128GM106

| Caractéristique | Valeur | Source |
|---|---|---|
| Mémoire programme | 0x000000 à 0x0157FF (adresses PC), soit 44 032 instructions de 24 bits | [GLD] |
| Zone de code du linker | 0x000200 à 0x01577F | [GLD] |
| Mots de configuration | 0x015780 à 0x0157C5, **dans la dernière page** (0x015400 à 0x0157FF) | [GLD] |
| Page effaçable | 512 instructions, soit 0x400 adresses PC ou 1 536 octets, alignée | [DS] § 5.2 |
| Ligne programmable | 64 instructions (192 octets), alignée | [DS] § 5.2 |
| Plus petite écriture | Double mot : 2 instructions adjacentes, alignées sur une adresse paire de double mot | [DS] § 5.3.1 |
| Endurance | **10 000 cycles effacement/écriture minimum**, de −40 à +125 °C | [DS] D130 |
| Rétention | 20 ans | [DS] D134 |
| Effacement d'une page | 19,2 à 20,7 ms | [DS] D137 |
| Écriture d'une ligne | 0,65 à 0,70 ms | [DS] D136 |
| Écriture d'un double mot | 45 à 49 µs | [DS] D138 |
| Pendant l'effacement ou l'écriture | **Le CPU est arrêté** jusqu'à la fin de l'opération : aucune interruption n'est servie | [DS] § 5.3 |
| ECC | Code de Hamming par double mot. Une erreur simple est corrigée. **Une erreur double déclenche une trappe matérielle générique.** Un double mot ne peut être programmé qu'une fois entre deux effacements. | [DS] § 5.4 |

La protection en écriture du segment général est désactivée dans la configuration actuelle (`GWRP = OFF`,
`GSS = DISABLED`, dans `mcc_generated_files/system.c`) : l'auto-programmation est possible.

## 4. Plan mémoire

La zone persistante occupe les **8 pages** placées juste avant la page des mots de configuration :
0x013400 à 0x0153FF, soit 12 Ko.

| Page | Adresses PC | Adresses du fichier .hex | Usage |
|---|---|---|---|
| 0 à 76 | 0x000000 à 0x0133FF | 0x000000 à 0x0267FF | Vecteurs et code applicatif (39 168 instructions de code) |
| 77 | 0x013400 à 0x0137FF | 0x026800 à 0x026FFF | Réserve : compteurs d'exploitation (MEM-Q5) |
| 78 | 0x013800 à 0x013BFF | 0x027000 à 0x0277FF | Réserve |
| 79 | 0x013C00 à 0x013FFF | 0x027800 à 0x027FFF | Paramètres, banque A |
| 80 | 0x014000 à 0x0143FF | 0x028000 à 0x0287FF | Paramètres, banque B |
| 81 | 0x014400 à 0x0147FF | 0x028800 à 0x028FFF | Journal des défauts, page 0 |
| 82 | 0x014800 à 0x014BFF | 0x029000 à 0x0297FF | Journal des défauts, page 1 |
| 83 | 0x014C00 à 0x014FFF | 0x029800 à 0x029FFF | Journal des défauts, page 2 |
| 84 | 0x015000 à 0x0153FF | 0x02A000 à 0x02A7FF | Journal des défauts, page 3 |
| 85 | 0x015400 à 0x0157FF | 0x02A800 à 0x02AFFF | Mots de configuration : jamais effacée par le firmware |

Après réservation, la zone de code compte 39 168 instructions, soit environ 117 Ko. Pour comparaison, le
firmware 2.2.1 triphasé compilé pour ce composant occupe 34 911 octets.

Format de stockage : seuls les **16 bits de poids faible** de chaque instruction portent des données, comme
le fait le compilateur pour `space(prog)`. L'octet de poids fort est programmé à 0x00, ce qui distingue
un mot écrit d'un mot effacé (0xFFFFFF). Une page offre ainsi 1 024 octets utiles, et un double mot 4 octets.

## 5. Protection lors de la reprogrammation

Une reprogrammation normale commence par un effacement complet de la puce, zone persistante comprise. Trois
mesures sont nécessaires pour que la zone survive à la mise à jour du firmware :

1. **Le linker ne place rien dans la zone et le .hex ne contient aucune donnée dans la zone.** La zone est
   réservée par un objet `noload` placé à adresse fixe, sans modifier le script de linker standard. Exemple,
   à valider sur la map :
   ```c
   /* 8 pages réservées : 4 096 instructions, absentes du .hex */
   const uint16_t smb_nvm_zone[4096]
       __attribute__((space(prog), address(0x13400), noload));
   ```
2. **L'outil de programmation préserve la plage.** Dans MPLAB X, propriétés du projet, outil (PICkit, ICD) :
   catégorie « Memories to Program », option « Preserve Program Memory », plage 0x13400 à 0x153FF. L'outil lit la
   plage avant l'effacement et la réécrit ensuite. Le réglage est enregistré dans
   `nbproject/configurations.xml` : il est versionné avec le projet, pour toutes les configurations de build,
   debug compris. MPLAB IPE propose le même réglage en mode avancé (à vérifier sur la version utilisée en production).
3. **La protection en lecture du code reste désactivée** (`GSS = DISABLED`). Sinon, l'outil ne peut pas relire
   la plage à préserver.

Un effacement complet sans préservation reste possible, volontairement ou par erreur de réglage. La SMB
retrouve alors une zone vierge : paramètres absents (MEM-PAR-6) et journal vide.

| Id | Exigence | Statut |
|---|---|---|
| MEM-MAP-1 | La zone persistante occupe les pages 77 à 84 (0x013400 à 0x0153FF). Le firmware n'efface ni n'écrit aucune autre page. La page 85, qui porte les mots de configuration, n'est jamais effacée. | Proposée |
| MEM-MAP-2 | La zone est réservée au linker par un objet `noload` à adresse fixe. Le fichier .map montre la réservation. Le .hex ne contient aucun enregistrement entre les adresses 0x026800 et 0x02A7FF. Un contrôle automatique sur le .hex produit par le build le vérifie. | Proposée |
| MEM-MAP-3 | Le projet MPLAB X préserve la plage 0x13400 à 0x153FF à la programmation, dans toutes les configurations. La procédure de programmation de production (IPE) le précise. | Proposée |
| MEM-MAP-4 | Chaque structure stockée porte un numéro de version de format. Une version inconnue, plus récente que celle du firmware, n'est ni interprétée ni écrasée : c'est le cas d'un retour à un firmware plus ancien. | Proposée |

## 6. Paramètres de configuration

### 6.1 Format

Les paramètres sont stockés sous forme de couples identifiant et valeur. Un firmware plus récent peut ainsi
ajouter des paramètres sans rendre illisibles ceux qui existent déjà. Chaque couple occupe un double mot.

| Double mot | Contenu |
|---|---|
| 0 | Marqueur 0x5350 (« SP ») et version de format (16 bits) |
| 1 | Numéro de séquence (32 bits), incrémenté à chaque enregistrement |
| 2 | Nombre n de paramètres (16 bits), réservé (16 bits) |
| 3 à 2 + n | Identifiant (16 bits), valeur (16 bits) |
| 3 + n | CRC-16 CCITT des doubles mots précédents, puis 0xA5A5 |

Deux banques, A et B, sont écrites en alternance. La banque active est la banque valide, marqueur et CRC
corrects, qui a le plus grand numéro de séquence. Pour enregistrer une nouvelle configuration :

1. la SMB efface la banque inactive ;
2. elle y écrit le bloc complet, CRC en dernier ;
3. elle relit et vérifie le bloc.

La banque inactive devient alors active, par son numéro de séquence. Une coupure à n'importe quelle étape laisse
l'ancienne banque intacte et valide.

### 6.2 Liste initiale

| Id | Nom | Valeurs | Défaut (zone vierge) | Prise en compte |
|---|---|---|---|---|
| 0x0001 | `BOXHY_ID` | 0, 1 ou 2 | 0 | Au redémarrage |
| 0x0002 | `SMB_ROLE` | 0 = `SEULE`, 1 = `TRIPHASE` | `SEULE` | Au redémarrage |
| 0x0003 | `PM_MODULE_COUNT` | 0, 1 ou 2 | aucun : configuration absente | Au redémarrage |
| 0x0010, 0x0011 | Numéro de série de la BOXHY | 32 bits, poids faible puis poids fort | 0 | Immédiate |
| 0x0100 à 0x7FFF | Réservés aux futurs paramètres : seuils, temporisations | | | |

Les seuils et temporisations restent dans `src/smb_config.h` tant qu'ils n'ont pas besoin d'être réglés sur
le terrain.

### 6.3 Exigences

| Id | Exigence | Statut |
|---|---|---|
| MEM-PAR-1 | Les paramètres sont stockés dans les banques A et B (pages 79 et 80), au format du § 6.1. Une écriture n'efface jamais la banque active. | Proposée |
| MEM-PAR-2 | Au démarrage, la SMB charge la banque active, puis vérifie chaque valeur : plage, et cohérence selon PM-CFG-5 de [SPEC-PM]. Un identifiant inconnu est conservé tel quel lors des réécritures, sans être interprété. | Proposée |
| MEM-PAR-3 | L'écriture se fait par commande CAN de maintenance, en deux temps. D'abord des trames d'écriture, une par paramètre, sont stockées en RAM. Ensuite une trame de validation, qui porte une clé et le CRC de l'ensemble, lance l'enregistrement. La SMB répond par le statut et relit les valeurs enregistrées. | Proposée |
| MEM-PAR-4 | L'enregistrement n'est accepté qu'en `SMB_STANDBY` ou en `SMB_MAINTENANCE`, onduleur et pile arrêtés. Il est refusé dans les autres états. | Proposée |
| MEM-PAR-5 | `BOXHY_ID`, `SMB_ROLE` et `PM_MODULE_COUNT` ne changent qu'au redémarrage. L'identifiant CAN de la SMB ne change donc jamais en cours de fonctionnement. | Proposée |
| MEM-PAR-6 | Zone vierge ou aucune banque valide : la SMB démarre avec `BOXHY_ID` = 0 et l'état « configuration absente ». Elle reste en défaut, relais en sécurité, et n'accepte que la lecture et l'écriture de paramètres. Le défaut est journalisé (§ 7). | Proposée |
| MEM-PAR-7 | Les paramètres sont lisibles à tout moment par commande CAN. Ils sont aussi émis en télémétrie avec la version du firmware (PM-CFG-6 de [SPEC-PM]). | Proposée |
| MEM-PAR-8 | Mise en service : sur un bus CAN couplé, plusieurs SMB non configurées répondraient toutes avec `BOXHY_ID` = 0. Chaque SMB est donc configurée avec le bus CAN de sa BOXHY isolé des autres, avant le couplage. Option d'atelier : un outil génère un .hex de la banque A à partir d'une liste de paramètres, programmé seul avec l'outil (MEM-Q2). | Proposée |

## 7. Journal des derniers défauts actifs

### 7.1 Catalogue des codes de défaut

Le firmware 2.0.0 n'a pas de codes de défaut : les erreurs sont des booléens répartis dans les modules
(`fcs.h`, `inverter.h`, `ctn_sensor.h`, `smb_state_machine.h`, `battery.h`…). Le journal impose un
**catalogue unique de codes**, dans un nouveau module `src/system/faults.c/.h`. Chaque module y déclare ses
défauts, et le module tient l'ensemble des défauts actifs dans leur ordre d'apparition.

Code sur 16 bits = famille (8 bits) et numéro (8 bits) :

| Famille | Domaine | Exemples |
|---|---|---|
| 0x01 | SMB | état inconnu, configuration absente ou incohérente |
| 0x02 | FCCB | erreur critique, erreur mineure, fuite H2, échec du démarrage, perte de communication |
| 0x03 | Onduleur | un code par drapeau de QPIWS, perte de communication, surcharge prolongée, déconnexion sur SOC |
| 0x04 | Batterie PowerModule | perte de communication, module manquant, alarmes et erreurs du Global Status, contacteur ouvert |
| 0x05 | Capteurs de température | une sonde × (haute, basse, perdue) |
| 0x06 | Triphasé | perte du maître, `BOXHY_ID` en double, puissance autorisée dépassée |
| 0x07 | Mémoire persistante | banque de paramètres invalide, erreur ECC, page de journal invalide |
| 0x08 | Reset | cause d'un reset anormal (RCON : watchdog, trappe, brown-out) |

Le catalogue complet, avec le texte affiché par l'IHM, est un livrable de l'implémentation (MEM-Q1). Il est
décrit dans un fichier de la base CAN, ou dans un document, pour que les outils et l'IHM le partagent.

### 7.2 Format d'un enregistrement

Un enregistrement occupe 16 instructions, soit 8 doubles mots et 32 octets utiles.

| Octets | Champ |
|---|---|
| 0 à 3 | Numéro d'enregistrement (32 bits), croissant sur toute la vie de la SMB |
| 4 et 5 | Numéro de session : nombre de mises sous tension, voir ci-dessous |
| 6 à 8 | Temps depuis la mise sous tension, en secondes (24 bits, saturé à 194 jours) |
| 9 | Type d'événement : 1 = apparition de défaut, 2 = arrêt ordonné, 3 = acquittement des défauts, 4 = reset anormal, 5 = journal effacé |
| 10 | État de la machine d'états SMB |
| 11 | Nombre total de défauts actifs, qui peut dépasser 8 |
| 12 | SOC batterie en %, 0xFF si pas de batterie |
| 13 | Nombre d'événements regroupés dans cet enregistrement ou non enregistrés depuis le précédent (saturé à 255) |
| 14 à 29 | Les 8 premiers codes actifs, dans l'ordre d'apparition. Le premier est le plus ancien : c'est en général la cause première. 0x0000 marque une case vide. |
| 30 et 31 | CRC-16 CCITT des octets 0 à 29 |

Le numéro de session n'exige aucune écriture au démarrage. La SMB relit le dernier enregistrement et
attribue le numéro suivant à sa session. Une session sans enregistrement ne consomme pas de numéro : ce n'est
pas un compteur de démarrages. Ce compteur relève des compteurs d'exploitation (MEM-Q5).

La SMB n'a pas d'horloge temps réel : l'horodatage est relatif à la session (MEM-Q3).

### 7.3 Buffer tournant

- **Organisation.** Le journal occupe 4 pages, chacune divisée en 32 cases de 32 octets.
  - La case 0 est l'en-tête : marqueur 0x534C (« SL »), version de format, numéro de séquence de page (32 bits), CRC.
  - Les cases 1 à 31 reçoivent les enregistrements.
- **Recherche au démarrage.** La SMB lit les quatre en-têtes. La page courante est la page valide de plus grand numéro de
  séquence. Dans cette page, la première case vierge est la prochaine case à écrire.
- **Écriture.** Un enregistrement est écrit dans la case suivante, double mot par double mot, CRC en dernier. Une case
  dont le CRC est faux est sautée : c'est le cas d'une écriture interrompue par une coupure.
- **Rotation.** Quand la page courante est pleine, la page la plus ancienne est effacée. Elle reçoit un en-tête dont le
  numéro de séquence vaut le plus grand plus un, et devient la page courante.
- **Zone vierge.** La première écriture initialise la page 0.
- **Historique conservé.** Entre 93 et 124 enregistrements, les plus récents : trois pages pleines, plus la page en cours.

### 7.4 Endurance

Chaque page n'est effacée qu'une fois par rotation complète, soit une fois tous les 124 enregistrements :

- capacité sur la vie de la SMB : 10 000 cycles × 124 = **1 240 000 enregistrements** ;
- usage courant, de l'ordre de 20 enregistrements par jour : environ 170 ans ;
- pire cas permanent, avec un défaut instable qui épuise la limite de débit (MEM-LOG-4, 6 enregistrements par
  heure en régime établi) : 52 560 enregistrements par an, soit environ 23 ans.

La durée de vie est donc limitée par la rétention de 20 ans, et non par l'endurance. Les pages du journal sont de
toute façon réécrites régulièrement. Les banques de paramètres, écrites quelques fois sur la vie de la BOXHY,
ne posent pas de question d'endurance (10 000 écritures par banque).

### 7.5 Règles d'écriture

| Id | Exigence | Statut |
|---|---|---|
| MEM-LOG-1 | Le journal occupe les pages 81 à 84, au format des § 7.2 et 7.3. Une page n'est effacée qu'à la rotation, ou à l'effacement volontaire du journal. | Proposée |
| MEM-LOG-2 | Un enregistrement « apparition de défaut » est écrit quand un code devient actif alors qu'il n'était pas actif dans le dernier enregistrement de la session. Un défaut qui disparaît puis réapparaît, sans autre changement, n'est donc pas réécrit. La disparition d'un défaut n'est pas enregistrée, sauf à l'acquittement (MEM-LOG-5). | Proposée |
| MEM-LOG-3 | Regroupement : après l'apparition d'un code, la SMB attend T_REGROUPEMENT avant d'écrire. Une cascade de défauts donne alors un seul enregistrement, avec la cause première en tête. Proposition : T_REGROUPEMENT = 2 s. | Proposée |
| MEM-LOG-4 | Limite de débit, par seau à jetons : 8 jetons, un jeton rendu toutes les 10 min. Sans jeton disponible, l'instantané le plus récent est gardé en RAM et écrit dès qu'un jeton revient. Le champ « événements non enregistrés » est incrémenté. | Proposée |
| MEM-LOG-5 | Sont aussi enregistrés : l'arrêt ordonné (clé ou commande d'arrêt), avec les défauts encore actifs ; l'acquittement des défauts en mode défaut ; un reset anormal, détecté au démarrage par RCON ; l'effacement du journal. Ces enregistrements consomment aussi des jetons, sauf le reset anormal. | Proposée |
| MEM-LOG-6 | Au démarrage, la SMB émet en télémétrie CAN le dernier enregistrement de la session précédente, qui contient les derniers défauts actifs, pour que l'IHM affiche la cause du dernier arrêt. | Proposée |
| MEM-LOG-7 | Le journal est lisible par commande CAN : lecture de l'enregistrement de rang k à partir du plus récent, et réponse sur plusieurs trames. Il est effaçable par une commande de maintenance protégée par une clé, seulement en `SMB_STANDBY` ou en `SMB_MAINTENANCE`. | Proposée |

## 8. Contraintes temps réel et robustesse

Pendant l'effacement ou la programmation de la flash, le CPU est arrêté et les interruptions ne sont pas
servies :

- **double mot :** moins de 49 µs, soit moins de 0,4 ms pour un enregistrement de 8 doubles mots. L'effet est négligeable,
  et l'écriture peut être répartie sur plusieurs tours de boucle ;
- **effacement de page :** jusqu'à 20,7 ms. À 2 400 bauds, l'UART de l'onduleur reçoit un caractère toutes les
  4,2 ms. Sa FIFO de 4 caractères, plus le registre à décalage, déborde en 20,8 ms environ : la marge est
  nulle si l'effacement tombe pendant une réponse de l'onduleur. Le tick de 1 ms prend jusqu'à 21 ms de retard.
  Les trames CAN reçues pendant ce temps peuvent être perdues. Les watchdog et DMT sont aujourd'hui
  désactivés (`FWDTEN = OFF`, `DMTEN = DISABLE`).

| Id | Exigence | Statut |
|---|---|---|
| MEM-RT-1 | Un effacement de page n'est lancé qu'en dehors d'un échange RS232 avec l'onduleur, c'est-à-dire avant l'envoi d'une nouvelle requête. Il n'est lancé ni pendant une séquence de démarrage ou d'arrêt de la pile ou de l'onduleur, ni pendant une écriture de paramètres. | Proposée |
| MEM-RT-2 | La page suivante du journal est effacée par anticipation à l'initialisation, avant le démarrage de la boucle de contrôle, quand la page courante a moins de 8 cases libres. Le cas d'un effacement en fonctionnement devient ainsi rare. Si aucun moment sûr ne se présente, l'enregistrement attend en RAM. | Proposée |
| MEM-RT-3 | Les watchdogs logiciels de communication (onduleur, FCCB, PowerModule, triphasé) tolèrent une interruption de 25 ms. Si le watchdog matériel ou le DMT sont activés un jour, leur période dépasse 25 ms. | Proposée |
| MEM-RT-4 | Erreur ECC double : la relecture d'un double mot interrompu par une coupure peut déclencher une trappe matérielle générique. Le gestionnaire de trappe MCC, aujourd'hui une boucle infinie, est adapté. Pendant la lecture de la zone persistante, il marque la case ou la banque comme invalide et reprend, au lieu de bloquer la SMB. Le défaut est journalisé (famille 0x07). | Proposée |
| MEM-RT-5 | Toutes les écritures passent par un seul module, `src/system/nvm.c/.h`. Il expose la lecture, l'écriture par double mot, l'effacement de page limité aux pages 77 à 84 et la vérification par relecture. Une adresse hors zone est refusée. La séquence de déverrouillage `NVMKEY` n'existe qu'à cet endroit. | Proposée |
| MEM-RT-6 | Une coupure d'alimentation à tout instant, en effacement ou en écriture, ne fait perdre au plus que l'enregistrement en cours, ou la configuration en cours d'enregistrement. L'ancienne configuration reste active. | Proposée |

## 9. Questions ouvertes

| N° | Question | Proposition | Décision |
|---|---|---|---|
| MEM-Q1 | Qui établit le catalogue des codes de défaut et leurs textes ? Faut-il reprendre une numérotation existante de l'IHM ou de la télémétrie ? | Catalogue établi à partir des drapeaux du firmware 2.0.0, relu par l'équipe système, publié avec la base CAN. | |
| MEM-Q2 | Comment les paramètres sont-ils saisis en mise en service : outil PC sur le bus CAN, IHM, ou .hex d'atelier ? | Commande CAN (MEM-PAR-3) depuis l'outil de maintenance. .hex d'atelier en option pour la production. | |
| MEM-Q3 | Faut-il une date et une heure absolues dans le journal ? La SMB n'a pas d'horloge temps réel. L'IHM ou la télémétrie pourraient la diffuser. | Horodatage relatif (session et temps écoulé), qui suffit à ordonner les événements. Ajout d'une heure absolue si l'IHM en diffuse une sur le bus. | |
| MEM-Q4 | Les 8 derniers codes actifs par enregistrement suffisent-ils ? | Oui : le nombre total de défauts actifs est aussi enregistré. | |
| MEM-Q5 | Faut-il des compteurs d'exploitation persistants : heures de pile, heures d'onduleur, nombre de démarrages, énergie ? | Page 77 réservée. Écriture toutes les heures environ, en buffer tournant, sur le même modèle que le journal. | |
| MEM-Q6 | Le journal doit-il aussi être consultable sur l'IHM, ou seulement par l'outil de maintenance ? | IHM : dernier arrêt et défauts actifs (MEM-LOG-6). Outil de maintenance : journal complet. | |

## 10. Historique

| Version | Date | Modification |
|---|---|---|
| 0.1 | 2026-11-04 | Premier brouillon : plan mémoire du dsPIC33EV128GM106 (8 pages de 0x013400 à 0x0153FF), protection à la reprogrammation, paramètres en banques A et B, journal des défauts en buffer tournant de 4 pages, endurance, contraintes temps réel. |
