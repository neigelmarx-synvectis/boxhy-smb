#!/usr/bin/env python3
"""Générateur de la base CAN du firmware SMB BOXHY.

Lit la base de référence `can database/BOXHY.dbc` (format Vector DBC) et génère :
  - src/can/can_db.h : identifiants, DLC et disposition de chaque signal (bit de départ,
    longueur, ordre des octets, signe, facteur, offset, min, max, tables de valeurs) ;
  - can database/BOXHY_SMB.DBF, BOXHY_FCCB.DBF, IBS.DBF : bases BUSMASTER chargées par
    BOXHY_CONFIG_1.1.0.cfx (répartition des messages par préfixe de nom SMB_, FCCB_, IBS_).

Usage :
  python3 tools/candb_gen.py            génère les fichiers
  python3 tools/candb_gen.py --check    vérifie qu'ils sont à jour (code retour 1 sinon)

Python standard uniquement (aucune dépendance).
"""
import argparse
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DBC_PATH = os.path.join(ROOT, "can database", "BOXHY.dbc")
HEADER_PATH = os.path.join(ROOT, "src", "can", "can_db.h")
DBF_FILES = [("SMB_", os.path.join(ROOT, "can database", "BOXHY_SMB.DBF")),
             ("FCCB_", os.path.join(ROOT, "can database", "BOXHY_FCCB.DBF")),
             ("IBS_", os.path.join(ROOT, "can database", "IBS.DBF"))]
DBC_ENCODING = "cp1252"


class DbcError(Exception):
    """Erreur de syntaxe ou de cohérence du .dbc (message affiché, code retour 2)."""
    pass


# ----------------------------------------------------------------------------------------
# Modèle
# ----------------------------------------------------------------------------------------
class Signal:
    """Signal DBC (ligne SG_), complété par VAL_, SIG_VALTYPE_ et CM_."""
    def __init__(self, name, start, length, intel, signed, factor, offset, vmin, vmax, unit, receivers):
        self.name = name
        self.start = start          # convention DBC : LSB (Intel) ou MSB en dents de scie (Motorola)
        self.length = length
        self.intel = intel
        self.signed = signed
        self.is_float = False
        self.factor = factor
        self.offset = offset
        self.min = vmin
        self.max = vmax
        self.unit = unit
        self.receivers = receivers
        self.values = []            # [(valeur, libellé)]
        self.comment = ""

    def bit_positions(self):
        """Positions des bits (numérotation octet*8 + bit), du MSB au LSB."""
        if self.intel:
            return [self.start + i for i in range(self.length - 1, -1, -1)]
        pos, out = self.start, []
        for _ in range(self.length):
            out.append(pos)
            pos = pos + 15 if pos % 8 == 0 else pos - 1
        return out

    def lsb(self):
        return self.bit_positions()[-1]


class Message:
    """Message DBC (ligne BO_). L'identifiant DBC porte le bit 31 pour une trame étendue :
    `raw_id` le conserve, `id` est l'identifiant CAN 29 bits réel."""
    def __init__(self, raw_id, name, dlc, transmitter):
        self.extended = bool(raw_id & 0x80000000)
        self.id = raw_id & 0x1FFFFFFF
        self.raw_id = raw_id
        self.name = name
        self.dlc = dlc
        self.transmitter = transmitter
        self.signals = []
        self.comment = ""


# ----------------------------------------------------------------------------------------
# Lecture du DBC
# ----------------------------------------------------------------------------------------
RE_BO = re.compile(r'^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+)')
RE_SG = re.compile(r'^SG_\s+(\w+)\s*(?:M|m\d+)?\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*'
                   r'\(\s*([^,]+),\s*([^)]+)\)\s*\[\s*([^|]+)\|\s*([^\]]+)\]\s*"([^"]*)"\s*(.*)$')
RE_VAL = re.compile(r'^VAL_\s+(\d+)\s+(\w+)\s+(.*);\s*$')
RE_VALTYPE = re.compile(r'^SIG_VALTYPE_\s+(\d+)\s+(\w+)\s*:\s*(\d)\s*;')
RE_CM_BO = re.compile(r'^CM_\s+BO_\s+(\d+)\s+"(.*)";\s*$', re.S)
RE_CM_SG = re.compile(r'^CM_\s+SG_\s+(\d+)\s+(\w+)\s+"(.*)";\s*$', re.S)


def statements(text):
    """Découpe en instructions ; un CM_ peut s'étendre sur plusieurs lignes."""
    lines = text.replace("\r\n", "\n").split("\n")
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith("CM_ ") and '"' in line:   # « CM_ » seul dans NS_ n'est pas un commentaire
            buf = line
            while not buf.rstrip().endswith('";') and i + 1 < len(lines):
                i += 1
                buf += "\n" + lines[i]
            yield buf
        else:
            yield line
        i += 1


def parse_dbc(path):
    """Lit le .dbc et renvoie la liste des messages, dans l'ordre du fichier (qui fixe l'ordre de
    can_db.h et des .DBF). Sous-ensemble DBC reconnu : BO_, SG_, VAL_, SIG_VALTYPE_, CM_ BO_ et
    CM_ SG_ ; les autres instructions (BU_, BA_DEF_, BA_...) sont ignorées. La base est ensuite
    validée par validate()."""
    text = open(path, encoding=DBC_ENCODING).read()
    messages, by_id, current = [], {}, None
    for st in statements(text):
        m = RE_BO.match(st)
        if m:
            current = Message(int(m.group(1)), m.group(2), int(m.group(3)), m.group(4))
            if current.raw_id in by_id:
                raise DbcError("identifiant en double : %s" % current.name)
            messages.append(current)
            by_id[current.raw_id] = current
            continue
        m = RE_SG.match(st)
        if m:
            if current is None:
                raise DbcError("SG_ hors message : %s" % st)
            name, start, length, order, sign, fac, off, vmin, vmax, unit, rx = m.groups()
            current.signals.append(Signal(name, int(start), int(length), order == "1", sign == "-",
                                          float(fac), float(off), float(vmin), float(vmax), unit,
                                          [r for r in re.split(r"[,\s]+", rx.strip()) if r]))
            continue
        if not st.startswith("SG_") and not st.startswith("BO_"):
            current = None if st == "" else current
        m = RE_VAL.match(st)
        if m:
            sig = find_signal(by_id, int(m.group(1)), m.group(2))
            sig.values = [(int(v), t) for v, t in re.findall(r'(-?\d+)\s+"([^"]*)"', m.group(3))]
            continue
        m = RE_VALTYPE.match(st)
        if m:
            sig = find_signal(by_id, int(m.group(1)), m.group(2))
            if m.group(3) == "1":
                sig.is_float = True
            elif m.group(3) != "0":
                raise DbcError("SIG_VALTYPE_ %s non supporté (double)" % m.group(2))
            continue
        m = RE_CM_SG.match(st)
        if m:
            find_signal(by_id, int(m.group(1)), m.group(2)).comment = m.group(3)
            continue
        m = RE_CM_BO.match(st)
        if m:
            by_id[int(m.group(1))].comment = m.group(2)
    validate(messages)
    return messages


def find_signal(by_id, raw_id, name):
    """Signal `name` du message d'identifiant DBC brut `raw_id` (bit 31 = trame étendue)."""
    if raw_id not in by_id:
        raise DbcError("message %d inconnu" % raw_id)
    for s in by_id[raw_id].signals:
        if s.name == name:
            return s
    raise DbcError("signal %s inconnu dans %s" % (name, by_id[raw_id].name))


def validate(messages):
    """Contrôles qui protègent le firmware d'une base incohérente : noms utilisables en C, DLC
    0..8, signaux de 1 à 32 bits (limite de la couche C), flottants de 32 bits, facteur non nul,
    chaque bit dans le DLC et utilisé par un seul signal (pas de multiplexage)."""
    ident = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
    for m in messages:
        if not ident.match(m.name):
            raise DbcError("nom de message invalide : %s" % m.name)
        if not 0 <= m.dlc <= 8:
            raise DbcError("%s : DLC %d" % (m.name, m.dlc))
        used = {}
        names = set()
        for s in m.signals:
            where = "%s.%s" % (m.name, s.name)
            if s.name.upper() in names:
                raise DbcError("%s : nom en double (insensible à la casse)" % where)
            names.add(s.name.upper())
            if not 1 <= s.length <= 32:
                raise DbcError("%s : longueur %d hors 1..32" % (where, s.length))
            if s.is_float and s.length != 32:
                raise DbcError("%s : un flottant doit faire 32 bits" % where)
            if s.factor == 0:
                raise DbcError("%s : facteur nul" % where)
            for p in s.bit_positions():
                if p < 0 or p >= 8 * m.dlc:
                    raise DbcError("%s : bit %d hors du DLC %d" % (where, p, m.dlc))
                if p in used:
                    raise DbcError("%s : chevauche %s (bit %d)" % (where, used[p], p))
                used[p] = s.name


# ----------------------------------------------------------------------------------------
# Génération de src/can/can_db.h
# ----------------------------------------------------------------------------------------
def c_num(x):
    """Littéral C double : « 100.0 » pour un entier (évite une division entière dans les macros
    de conversion de can_signal.h), sinon la représentation Python la plus courte (« 0.01 »)."""
    x = float(x)
    if x == int(x) and abs(x) < 1e15:
        return "%d.0" % int(x)
    return repr(x)


def c_ident(name):
    """Nom de message / signal / valeur converti en identifiant C majuscule (caractères hors
    [A-Za-z0-9_] remplacés par « _ »)."""
    return re.sub(r"[^A-Za-z0-9_]", "_", name).upper()


def flags(s):
    """Drapeaux de disposition (sCanSignalLayout.flags) : ordre des octets | type de valeur."""
    order = "CAN_INTEL" if s.intel else "CAN_MOTOROLA"
    kind = "CAN_FLOAT32" if s.is_float else ("CAN_SIGNED" if s.signed else "CAN_UNSIGNED")
    return "%s | %s" % (order, kind)


def c_comment_lines(text, prefix, width=106):
    """Découpe un commentaire du .dbc en lignes de commentaire C de largeur bornée.
    « */ » est neutralisé pour ne pas fermer le commentaire C par erreur."""
    words = text.replace("*/", "* /").split()
    lines, cur = [], prefix
    for word in words:
        if len(cur) + 1 + len(word) > width and cur.strip() != prefix.strip():
            lines.append(cur)
            cur = " *" + " " * (len(prefix) - 2)
        cur = cur + (" " if not cur.endswith(" ") else "") + word
    lines.append(cur)
    return lines


def gen_header(messages):
    """Contenu de src/can/can_db.h : une section de macros par message, puis (pour les tests
    seulement) la liste de tous les signaux. Les commentaires CM_ du .dbc sont recopiés en
    commentaire C au-dessus du message ou du signal concerné ; ils n'ont aucun effet sur le code."""
    out = []
    w = out.append
    w("/**")
    w(" * @file can_db.h")
    w(" * @brief Base CAN BOXHY - FICHIER GÉNÉRÉ par tools/candb_gen.py depuis can database/BOXHY.dbc.")
    w(" *")
    w(" * NE PAS MODIFIER À LA MAIN : modifier le .dbc puis relancer « python3 tools/candb_gen.py ».")
    w(" *")
    w(" * Pour chaque message <M> : CANDB_<M>_ID, _EXTENDED, _DLC, _NB_SIGNALS.")
    w(" * Pour chaque signal <S> de <M> : CANDB_<M>__<S>_LAYOUT (bit de départ DBC, longueur, drapeaux),")
    w(" * _FACTOR, _OFFSET, _MIN, _MAX (valeurs physiques) et CANDB_<M>__<S>__<VALEUR> pour les VAL_.")
    w(" *")
    w(" * Ces macros ne génèrent ni code ni données : elles servent à construire les tables de liaison")
    w(" * des modules, par exemple :")
    w(" *     CAN_BIND(CANDB_SMB_TX_GENERAL__STEP, &smb.step, CAN_VAR_U8, 1)")
    w(" * (voir src/can/can_signal.h). Les commentaires sous chaque message et signal reprennent les")
    w(" * commentaires CM_ du .dbc.")
    w(" */")
    w("#ifndef CAN_DB_H")
    w("#define CAN_DB_H")
    w("")
    w('#include "src/can/can_signal.h"')
    w("")
    for m in messages:
        mi = "CANDB_" + c_ident(m.name)
        w("/* " + "-" * 86 + " */")
        if m.comment:
            w("/* %s : 0x%X, DLC %d, émetteur %s" % (m.name, m.id, m.dlc, m.transmitter))
            out.extend(c_comment_lines(m.comment, " * "))
            w(" */")
        else:
            w("/* %s : 0x%X, DLC %d, émetteur %s */" % (m.name, m.id, m.dlc, m.transmitter))
        w("#define %-60s 0x%08XUL" % (mi + "_ID", m.id))
        w("#define %-60s %d" % (mi + "_EXTENDED", 1 if m.extended else 0))
        w("#define %-60s %du" % (mi + "_DLC", m.dlc))
        w("#define %-60s %du" % (mi + "_NB_SIGNALS", len(m.signals)))
        for s in m.signals:
            si = "%s__%s" % (mi, c_ident(s.name))
            unit = (" [%s]" % s.unit) if s.unit else ""
            if s.comment:
                w("/* %s%s" % (s.name, unit))
                out.extend(c_comment_lines(s.comment, " *   "))
                w(" */")
            else:
                w("/* %s%s */" % (s.name, unit))
            w("#define %-60s CAN_LAYOUT(%du, %du, %s)" % (si + "_LAYOUT", s.start, s.length, flags(s)))
            w("#define %-60s %s" % (si + "_FACTOR", c_num(s.factor)))
            w("#define %-60s %s" % (si + "_OFFSET", c_num(s.offset)))
            w("#define %-60s %s" % (si + "_MIN", c_num(s.min)))
            w("#define %-60s %s" % (si + "_MAX", c_num(s.max)))
            for v, t in s.values:
                w("#define %-60s %d" % ("%s__%s" % (si, c_ident(t)), v))
        w("")
    # Liste complète des signaux : compilée uniquement pour les tests (pas en flash cible).
    w("/* Liste de tous les signaux de la base, compilée seulement si CANDB_WITH_SIGNAL_LIST est défini")
    w(" * (tests/test_can_signal.c) : elle n'occupe aucune place dans le firmware. */")
    w("#ifdef CANDB_WITH_SIGNAL_LIST")
    w("typedef struct")
    w("{")
    w("    const char *message;")
    w("    const char *signal;")
    w("    uint32_t id;")
    w("    uint8_t dlc;")
    w("    sCanSignalLayout layout;")
    w("    double factor, offset, min, max;")
    w("} sCanDbSignalInfo;")
    w("")
    w("static const sCanDbSignalInfo candb_signal_list[] = {")
    for m in messages:
        mi = "CANDB_" + c_ident(m.name)
        for s in m.signals:
            si = "%s__%s" % (mi, c_ident(s.name))
            w('    {"%s", "%s", %s_ID, %s_DLC, %s_LAYOUT, %s_FACTOR, %s_OFFSET, %s_MIN, %s_MAX},'
              % (m.name, s.name, mi, mi, si, si, si, si, si))
    w("};")
    w("#define CANDB_SIGNAL_LIST_COUNT (sizeof(candb_signal_list) / sizeof(candb_signal_list[0]))")
    w("#endif /* CANDB_WITH_SIGNAL_LIST */")
    w("")
    w("#endif /* CAN_DB_H */")
    return "\n".join(out) + "\n"


# ----------------------------------------------------------------------------------------
# Génération des .DBF BUSMASTER
# ----------------------------------------------------------------------------------------
def dbf_num(x):
    """Nombre au format des bornes BUSMASTER : entier sans décimale, sinon 6 décimales au plus."""
    x = float(x)
    if x == int(x):
        return "%d" % int(x)
    return ("%.6f" % x).rstrip("0").rstrip(".")


def gen_dbf(messages):
    """Format BUSMASTER 3.2.2 : bit de départ = LSB (octet 1-based + bit), ordre 1 = Intel, 0 = Motorola."""
    head = ["//******************************BUSMASTER Messages and signals Database ******************************//",
            "", "[DATABASE_VERSION] 1.3", "", "[PROTOCOL] CAN", "", "[BUSMASTER_VERSION] [3.2.2]", "",
            "[NUMBER_OF_MESSAGES] %d" % len(messages), ""]
    blocks = []
    for m in messages:
        b = ["[START_MSG] %s,%d,%d,%d,1,%s" % (m.name, m.id, m.dlc, len(m.signals), "X" if m.extended else "S")]
        for s in m.signals:
            lsb = s.lsb()
            if s.is_float:
                typ = "F"
            elif s.length == 1 and not s.signed:
                typ = "B"
            else:
                typ = "I" if s.signed else "U"
            raw_a = (s.max - s.offset) / s.factor
            raw_b = (s.min - s.offset) / s.factor
            raw_max, raw_min = max(raw_a, raw_b), min(raw_a, raw_b)
            b.append("[START_SIGNALS] %s,%d,%d,%d,%s,%s,%s,%d,%f,%f,%s," % (
                s.name, s.length, lsb // 8 + 1, lsb % 8, typ, dbf_num(raw_max), dbf_num(raw_min),
                1 if s.intel else 0, s.offset, s.factor, s.unit))
            for v, t in s.values:
                b.append("[VALUE_DESCRIPTION] %s,%d" % (t, v))
        b.append("[END_MSG]")
        blocks.append("\r\n".join(b))
    return "\r\n".join(head) + "\r\n" + "\r\n\r\n".join(blocks)


# ----------------------------------------------------------------------------------------
def outputs(messages):
    """Contenu attendu de chaque fichier généré, en octets : can_db.h en UTF-8, .DBF en latin-1
    (encodage des fichiers BUSMASTER d'origine). Chaque message doit relever d'un .DBF."""
    files = {HEADER_PATH: gen_header(messages).encode("utf-8")}
    for prefix, path in DBF_FILES:
        subset = [m for m in messages if m.name.startswith(prefix)]
        files[path] = gen_dbf(subset).encode("latin-1")
    others = [m.name for m in messages if not any(m.name.startswith(p) for p, _ in DBF_FILES)]
    if others:
        raise DbcError("messages sans fichier .DBF (préfixe SMB_, FCCB_ ou IBS_ attendu) : %s" % ", ".join(others))
    return files


def main():
    """Génère les fichiers (ou les vérifie avec --check). N'écrit que les fichiers dont le contenu
    change, pour ne pas toucher aux dates de modification inutilement. Codes de retour : 0 succès,
    1 fichiers obsolètes (--check), 2 base .dbc invalide."""
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="vérifie que les fichiers générés sont à jour")
    ap.add_argument("--dbc", default=DBC_PATH)
    args = ap.parse_args()
    try:
        messages = parse_dbc(args.dbc)
        files = outputs(messages)
    except DbcError as e:
        print("candb_gen: erreur : %s" % e, file=sys.stderr)
        return 2
    stale = []
    for path, content in files.items():
        current = open(path, "rb").read() if os.path.exists(path) else None
        if current != content:
            stale.append(path)
            if not args.check:
                with open(path, "wb") as f:
                    f.write(content)
    rel = [os.path.relpath(p, ROOT) for p in stale]
    if args.check:
        if stale:
            print("candb_gen: fichiers générés obsolètes : %s" % ", ".join(rel), file=sys.stderr)
            print("candb_gen: lancer « python3 tools/candb_gen.py »", file=sys.stderr)
            return 1
        print("candb_gen: %d messages, %d signaux, fichiers générés à jour"
              % (len(messages), sum(len(m.signals) for m in messages)))
        return 0
    print("candb_gen: %d messages, %d signaux ; %s"
          % (len(messages), sum(len(m.signals) for m in messages),
             ("écrits : " + ", ".join(rel)) if rel else "rien à changer"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
