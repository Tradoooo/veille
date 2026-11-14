#!/usr/bin/env python3
"""Extrait un segment RÉEL du record 222 MIT-BIH et l'embarque en C.

Pourquoi embarquer l'octet brut : l'invité QNX de démonstration n'a ni disque,
ni réseau, ni Python. Le seul moyen d'y faire tourner des DONNÉES RÉELLES est
d'embarquer un extrait du fichier original dans l'IFS — et de le DÉCODER sur la
cible avec le décodeur format 212 de target/replay/ (le même algorithme que le
jumeau Python host/veille/wfdb.py, ce qui rend la voie comparative).

Ce qui est embarqué : les octets CONSÉCUTIFS du fichier 222.dat (pas de
re-échantillonnage, pas de filtre, pas de sélection de beats) — les 3600
premiers groupes, soit 10 s à 360 Hz × 2 canaux. Toute modification du signal
serait une falsification de la preuve.

Vérification d'intégrité : l'empreinte SHA-256 du fichier original complet est
publiée par PhysioNet dans SHA256SUMS.txt ; le script la contrôle avant
d'extraire et l'écrit dans l'en-tête généré.

Usage : python3 tools/qnx/extrait222.py <222.dat> <SHA256.txt> > target/replay/mitdb222_212.h
"""
import hashlib
import sys

FS_HZ = 360             # fréquence d'échantillonnage publiée du record 222
GROUPES = 3600          # 10 s à 360 Hz (2 canaux entrelacés, 3 octets par groupe)
PAR_LIGNE = 40          # octets par ligne hexadécimale (garde le fichier < 500 lignes)


def sha256_publie(chemin_sums, nom):
    """Retourne l'empreinte SHA-256 publiée pour `nom` dans SHA256SUMS.txt."""
    with open(chemin_sums, encoding="utf-8") as f:
        for ligne in f:
            parts = ligne.split()
            if len(parts) == 2 and parts[1] == nom:
                return parts[0]
    raise SystemExit("empreinte de %s introuvable dans %s" % (nom, chemin_sums))


def main(dat, sums):
    with open(dat, "rb") as f:
        brut = f.read()
    reel = hashlib.sha256(brut).hexdigest()
    attendu = sha256_publie(sums, "222.dat")
    if reel != attendu:
        raise SystemExit("EMPREINTE DIFFÉRENTE : %s != %s (fichier corrompu "
                         "ou jeu de sommes périmé)" % (reel, attendu))
    octets = brut[: GROUPES * 3]
    entete = (
        "/* MIT-BIH 222 — extrait RÉEL embarqué dans l'IFS de démonstration. */\n"
        "/*\n"
        " * Source : https://physionet.org/files/mitdb/1.0.0/222.dat (licence\n"
        " * Open Data Commons Attribution v1.0 — citation : Goldberger AL et al.,\n"
        " * 'PhysioBank, PhysioToolkit, and PhysioNet', Circulation 2000;101:e215-e220.\n"
        " *\n"
        " * Contenu : les %d premiers GROUPES du fichier original, octet par octet\n"
        " * (format WFDB 212, little-endian : 2 échantillons 12 bits signés par groupe\n"
        " * de 3 octets). %d octets = %.0f s à %d Hz, canaux MLII puis V1 entrelacés.\n"
        " *\n"
        " * Intégrité : SHA-256 du fichier complet = %s\n"
        " * (contrôlé contre SHA256SUMS.txt publié au moment de la génération).\n"
        " * Généré par tools/qnx/extrait222.py — ne pas éditer à la main.\n"
        " */"
        % (GROUPES, len(octets), GROUPES / FS_HZ, FS_HZ, attendu))
    h = [entete,
         "#ifndef MITDB222_212_H",
         "#define MITDB222_212_H",
         "",
         "#include <stdint.h>",
         "",
         "#define MITDB222_FS_HZ      360",
         "#define MITDB222_N_GROUPES  %d" % GROUPES,
         "#define MITDB222_N_OCTETS   (MITDB222_N_GROUPES * 3)",
         '#define MITDB222_RECORD     "mitdb/222 (MLII, 10 s, 360 Hz)"',
         '#define MITDB222_SHA256     "%s"' % attendu,
         "",
         "static const uint8_t mitdb222_212[MITDB222_N_OCTETS] = {"]
    lignes = []
    for i in range(0, len(octets), PAR_LIGNE):
        lignes.append("    " + "".join("0x%02x," % b for b in octets[i:i + PAR_LIGNE]))
    h.extend(lignes)
    h.append("};")
    h.append("")
    h.append("#endif")
    sys.stdout.write("\n".join(h) + "\n")
    sys.stderr.write("octets embarqués : %d (%d lignes) — SHA-256 publié vérifié\n"
                     % (len(octets), len(lignes)))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
