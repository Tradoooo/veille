#!/bin/bash
# Capture l'écran VGA de l'invité Veille AU MOMENT de chaque phase, en surveillant
# la console série. Une capture par phase : la preuve visuelle est ainsi calée sur
# le journal texte (des captures à intervalle fixe tombent sur des instants
# quelconques et ne correspondent à aucune phase).
#
# L'invité est RELANCÉ au départ (journal vidé) : sans ça, on lirait la phase de
# la démonstration précédente, déjà terminée — et aucune nouvelle phase n'arrive.
#
# usage : B=<base vitrine> SSH=<hôte> LOG=<journal> QEMU=<unité> ./capture-phases.sh [nb_phases]
set -u
B=${B:-https://r630-debian.tail3a8bdf.ts.net:8091/veille}
LOG=${LOG:-/home/baptiste/veille-demo/serial.log}
SSH=${SSH:-baptiste@100.124.18.48}
QEMU=${QEMU:-veille-qemu.service}
OUT=${OUT:-.}
N=${1:-8}
VUES=$(mktemp)
trap 'rm -f "$VUES"' EXIT

ssh -o BatchMode=yes "$SSH" "systemctl --user stop $QEMU; : > $LOG" || exit 1
sleep 3
curl -sk -o /dev/null "$B/"                  # visite déclenchante (garde d'inactivité)
fin=$((SECONDS + 420))
while [ $SECONDS -lt $fin ] && [ "$(wc -l < "$VUES")" -lt "$N" ]; do
    curl -sk -o /dev/null "$B/"              # maintient la vitrine « visitée »
    phase=$(ssh -o BatchMode=yes "$SSH" "grep -o 'PHASE [0-9]/8' '$LOG' | tail -1" 2>/dev/null | tr -d '\r\n')
    if [ -n "$phase" ] && ! grep -qx "$phase" "$VUES"; then
        nom=$(echo "$phase" | tr ' /' '__')   # le '/' de « 8/8 » est un séparateur
        if curl -sk -o "$OUT/$nom.png" "$B/screen.png" && [ -s "$OUT/$nom.png" ]; then
            echo "$phase" >> "$VUES"
            echo "  $phase → $nom.png ($(wc -c < "$OUT/$nom.png") o)"
        fi
    fi
    sleep 2
done
echo "phases capturées : $(wc -l < "$VUES")/$N"
