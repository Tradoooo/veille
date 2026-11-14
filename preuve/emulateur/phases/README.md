# Preuve — les huit phases de la démo, à l'écran de l'invité QNX 8.0

Une capture **par phase**, prise au moment où la phase s'affiche (et non à
intervalle fixe) : chaque image correspond donc à un passage précis du journal
texte, et les deux se relisent côte à côte.

**Exécution** : 2026-11-14, 16 h 52 → 17 h 00 (Paris), invité QNX 8.0 de la
vitrine `https://r630-debian.tail3a8bdf.ts.net:8091/veille/`, dépôt à `f3dfd9d`
(PR #26 + #27 mergées). Les 8 PNG ont 8 empreintes MD5 distinctes : ce sont bien
8 écrans différents, pas la même image reprise.

| Fichier | Phase | Ce que l'écran montre |
|---|---|---|
| `PHASE_1_8.png` | 1/8 PREUVE | `VEILLE_PROOF_COMPLETE bound=200 states=4800` — exploration exhaustive jouée **dans l'invité** |
| `PHASE_2_8.png` | 2/8 SIMULATION | 291 alarmes physiologiques, 8 techniques, `L_measured <= B_proof` |
| `PHASE_3_8.png` | 3/8 SIGNAL BRUT | les 4 états capteur (plat → figé, tachy → alarme haute, mort, régulier) |
| `PHASE_4_8.png` | 4/8 CIBLE | `/dev/veille` + client de mesure + verdict après déconnexion du pair |
| `PHASE_5_8.png` | 5/8 SIGNAL RÉEL | **ECG réel** (mitdb/222) décodé format 212 dans l'invité : 3600 échantillons, `sensor_ok`, **0 alarme technique** |
| `PHASE_6_8.png` | 6/8 CHARGE | E2 : `prio_base=10` → `prio_effective=40` (client à 40, charge `hog` à 30) |
| `PHASE_7_8.png` | 7/8 CADENCE | E3 : `pulses=300`, `taux_hz=99.98`, `maxdev_us=51`, `coalesced=0` |
| `PHASE_8_8.png` | 8/8 CAPTEUR MUET | E4 : `wd_count=1`, `wd_fire_silence_ms=801` + rapport d'audit (empreinte `c8faf69b…`) |

Journal texte complet de la même exécution :
[`journal-serie-2026-11-14.log`](journal-serie-2026-11-14.log) (console série de
l'invité, telle que la vitrine l'affiche).

## Ce que ces captures établissent, et ce qu'elles n'établissent pas

**Elles établissent** que la chaîne, la preuve par exploration, le rejeu d'ECG
réel, l'héritage de priorité, la cadence du tick noyau, le watchdog et le rapport
d'audit s'exécutent **dans QNX 8.0** — pas dans un terminal de l'hôte, pas en
simulation Python. L'invite reste interactive après la démo : les touches envoyées
depuis la page arrivent dans l'invité (`uname -a` → `QNX localhost 8.0.0`).

**Elles n'établissent pas** une propriété du matériel ni d'une cible embarquée :
l'invité tourne sous QEMU **sans KVM** (émulation CPU). La preuve en est la
variabilité d'E3 entre deux boot : `maxdev_us=51` ici, `27832` lors de
l'exécution du 14/11 16 h 19 sur la même image. L'écart ne vient pas du noyau
QNX, il vient de la charge de l'hôte et de l'émulation. **Un chiffre de la démo
n'est pas une mesure de référence** — les protocoles de mesure isolés sont dans
[`../../qnx80/`](../../qnx80/) et discutés dans `docs/QNX-EVIDENCE.md` §5 à §8.

## Régénérer

```bash
# l'invité doit être joignable (la vitrine le relance à la première visite)
cd preuve/emulateur/phases
B=https://r630-debian.tail3a8bdf.ts.net:8091/veille \
SSH=baptiste@100.124.18.48 \
LOG=/home/baptiste/veille-demo/serial.log \
./capture-phases.sh 8
```

Le script **relance l'invité et vide le journal** avant de capturer : sans ça, il
lirait la phase de la démonstration précédente (déjà terminée) et n'attendrait
aucune phase nouvelle — c'est ce qui a donné une première tentative à 0/8.

## Lecture des images

Les PNG font 720×400 : c'est l'écran VGA de l'invité (80×25 caractères), capturé
par le moniteur QEMU (`screendump`) et servi par la vitrine. Le texte y est
orienté **octet** (CP437) : les accents UTF-8 sont retirés par le scénario, d'où
l'ASCII partout à l'écran. Ce n'est pas une dégradation du calcul — les journaux
bruts (sortie complète de `veille-sim`, verdicts intégraux) restent sur la
console série et dans `/dev/shmem/`.
