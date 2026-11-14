#!/usr/bin/env bash
# Veille — compilation croisée QNX 8.0 du resource manager et des outils de
# mesure (tools/qnx/). Sortie : build/qnx/ (veille-rm, bench_client, hog).
#
#   bash tools/qnx/build.sh              # construit tout
#   QCC_V=gcc_ntoaarch64le bash tools/qnx/build.sh   # cible RPi5
#
# Le SDP 8.0 est requis (qcc) : QNX_SDP=/chemin/vers/sdp par défaut ~/qnx800.
set -euo pipefail
racine=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$racine"

SDP=${QNX_SDP:-$HOME/qnx800}
QCC_V=${QCC_V:-gcc_ntox86_64}
OUT=${OUT:-build/qnx}
CFLAGS="-std=c11 -O2 -Wall -Wextra -Werror -D_QNX_SOURCE"
INC="-Icore/include -Itarget/pipe -Itarget/port -Itarget/rm -Itarget/replay"

if [ ! -f "$SDP/qnxsdp-env.sh" ]; then
  echo "[build] SDP introuvable dans $SDP — voir docs/QNX-BUILD.md" >&2
  exit 1
fi
# shellcheck disable=SC1090
. "$SDP/qnxsdp-env.sh" >/dev/null 2>&1

mkdir -p "$OUT"
echo "[build] $(qcc -V 2>&1 | head -1) → $QCC_V"

CORE_SRC=$(ls core/src/*.c)
qcc -V "$QCC_V" $CFLAGS $INC \
  target/rm/rm_chain.c target/rm/rm_io.c target/rm/resmgr_veille.c \
  target/pipe/veille_pipe.c target/port/qnx_time.c $CORE_SRC \
  -o "$OUT/veille-rm"

# Outils de preuve : noms IDENTIQUES à ceux déclarés dans image/buildfile, pour
# que le même binaire serve dans l'invité et les journaux de preuve.
qcc -V "$QCC_V" $CFLAGS $INC tools/qnx/bench_client.c -o "$OUT/bench-client"
qcc -V "$QCC_V" $CFLAGS $INC tools/qnx/hog.c -o "$OUT/hog"
qcc -V "$QCC_V" $CFLAGS $INC tools/qnx/tick_probe.c -o "$OUT/tick-probe"

# Rejeu du signal RÉEL sur cible : les octets du record mitdb/222 sont embarqués
# dans l'IFS (target/replay/mitdb222_212.h, empreinte SHA-256 publiée vérifiée à
# la génération), décodés en C (format WFDB 212) et hachés par core/sha256.c.
# Ni Python, ni disque, ni réseau dans l'invité — c'est ce qui rend la donnée
# réelle visible À L'ÉCRAN de l'émulateur (elle ne l'était pas avant : le rejeu
# se faisait depuis l'hôte).
qcc -V "$QCC_V" $CFLAGS $INC \
  target/replay/replay_ecg.c target/replay/replay_rapport.c \
  target/pipe/veille_pipe.c target/port/qnx_time.c $CORE_SRC \
  -o "$OUT/replay-ecg"

file "$OUT/veille-rm" "$OUT/bench-client" "$OUT/hog" "$OUT/tick-probe" "$OUT/replay-ecg"
echo "[build] ok → $OUT/"
