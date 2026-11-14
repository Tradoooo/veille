# Veille — ce qui se joue DANS l'émulateur (huit phases + données réelles)

Extrait de [`docs/QNX-EVIDENCE.md`](docs/QNX-EVIDENCE.md) (§3.4), découpé pour tenir la
règle de taille des fichiers (< 500 lignes). Le document mère garde la liste des
sections ; celui-ci porte **la démonstration de l'invité** : les huit phases
jouées au démarrage, le rejeu d'ECG réel décodé en C sur la cible, et les
précautions de lecture qui vont avec. Les mesures isolées (E2/E3/E4/H2 hors démo)
restent dans `QNX-EVIDENCE.md` §5 à §8.

---

### 3.4a — Rejeu réel visible dans la démo VGA (record 222, 10 s embarqué)

Depuis le commit `42d64ca` (PR #26, CI verte), les **3600 échantillons bruts** du
record MIT-BIH 222 (fibrillation atriale, canal MLII, 360 Hz, 10 secondes)
sont embarqués *octet par octet* dans `target/replay/mitdb222_212.h` — 10 800
octets du fichier PhysioNet original, format 212 entrelacé, SHA-256 du fichier
complet vérifié contre `SHA256SUMS.txt` à la génération. Dans l'invité QNX 8.0,
`replay-ecg` décode le format 212 et fait passer les échantillons bruts dans le
pipeline C partagé : **3600 échantillons, 3238 fréquences mesurées (65–87 bpm,
moyenne 77), 0 alarme technique**. Résultat capturé en direct dans la phase 5
sur la console VGA + série de l'invité (vérifié le 14/11 à 16 h 19). Ça démontre
que le chemin de SIGNAL BRUT ne confond pas un rythme irrégulier mais vivant
avec un capteur figé — pas que le détecteur de pics est cliniquement validé.

L'empreinte SHA-256 du fichier complet sert à la provenance de l'extrait
embarqué. Le rapport d'audit S5, lui, porte l'empreinte de la **configuration
** (`c8faf69b…`, calculée sur la cible par `core/sha256.c`, pur C11). Le rapport
ne porte PAS le hash de l'extrait : ne pas confondre les deux.

### 3.4b — Rejeu hors démo (2401 BPM MIT-BIH 222, 30 min)

Le rejeu intégral du record 222, lui, reste un client **Python sur l'hôte**
(`host/veille/resmgr_client.py`) qui écrit 2401 valeurs BPM dérivées dans
`/dev/veille`. Il n'est pas décodé dans le noyau QNX. Ça prouve que le resource
manager accepte des BPM dérivés de données réelles — pas que le pipeline C de
l'invité a traité 30 minutes d'ECG brut. Son journal est dans
`preuve/qnx80/preuve-replay-mitdb*-*.log`.

### 3.4c — Rejets de données réelles (conduits depuis l'hôte)

| Jeu | Enregistrement | Signal | Résultat sur cible |
|---|---|---|---|
| MIT-BIH Arrhythmia | 100 | ECG MLII, 360 Hz | `INFO VEILLE_OK chain=100_MLII latency=50ms` (rythme sinusal normal : aucune alarme, conforme) |
| MIT-BIH Arrhythmia | 105 | ECG MLII | `INFO VEILLE_OK chain=105_MLII latency=50ms` |
| MIT-BIH Arrhythmia | 222 | ECG MLII (fibrillation atriale) | 2500 pics R détectés, 2401 échantillons de bpm réels écrits dans `/dev/veille` (bpm 43 → 182, moyenne 87,9) |
| MIT-BIH Arrhythmia | 205 | ECG MLII (tachycardie ventriculaire) | 2647 pics R, bpm 46 → 200 |
| BIDMC | bidmc01 | ECG + PLETH + SpO2 | SpO2 réel lu 96–98 %, HR 88–95 bpm |
| MIT-BIH Polysomnographic | slp02a | ECG + pression artérielle | PA réelle : 41 → 109,47 mmHg |

Le rapport d'audit produit **sur la cible** (client Python) porte l'empreinte du
fichier source :

```
- Configuration : /data/veille-data/mitdb/105
- Empreinte SHA-256 : fcd0153dfea69c749194bb73de7db124588857cfa2f6aecb72754894c8afde02
- Horodatage (UTC) : 2026-11-12T14:45:18Z
```

Journaux : `preuve/qnx80/preuve-replay-mitdb*-*.log`,
`preuve/qnx80/preuve-reel-*-*.log`, `preuve/qnx80/preuve-vt205-cible-*.log`.

### 3.4d — La démonstration de l'invité : huit phases, tout dans l'émulateur

Depuis la PR #26, le scénario joué au démarrage de l'invité (`image/demo.sh`,
lancé par `.profile` dans la session VGA unique) fait **huit phases**, toutes
exécutées dans QNX 8.0, toutes visibles sur l'écran VGA :

| Phase | Ce qui est démontré dans l'invité | Relevé (console série, 14/11) |
|---|---|---|
| 1/8 PREUVE | exploration exhaustive des 4800 états | `VEILLE_PROOF_COMPLETE bound=200 states=4800` |
| 2/8 SIMULATION | 600 échantillons, pannes injectées | 291 alarmes physio, 8 techniques, `L_measured ≤ B_proof` |
| 3/8 SIGNAL BRUT | les 4 états capteur | figé / tachy / mort / régulier |
| 4/8 CIBLE | resource manager `/dev/veille` + client de mesure + verdict | `min_us=9 max_us=79 moyenne_us=31` |
| 5/8 SIGNAL RÉEL | décodage 212 du record 222 **dans l'invité** | 3600 échantillons, `sensor_ok`, **0 alarme technique** |
| 6/8 CHARGE | E2 : héritage de priorité du noyau | `prio_base=10` → `prio_effective=40` (client à 40, hog à 30) |
| 7/8 CADENCE | E3 : tick noyau, sonde indépendante | `pulses=300`, `taux_hz=99,98`, `maxdev_us=51` (exécution de 16 h 52) — **et** `pulses=298`, `taux_hz=99,04`, `maxdev_us=27832` (exécution de 16 h 19, même binaire, même image) |
| 8/8 CAPTEUR MUET | E4 : watchdog + rapport d'audit S5 | `wd_count=1`, `wd_fire_silence_ms=801`, empreinte `c8faf69b…` |

Deux tours de huit phases, puis l'invite interactive reste en place (les touches
envoyées depuis la vitrine web arrivent dans l'invité QNX).

**Une capture par phase, au moment où elle s'affiche** :
[`preuve/emulateur/phases/`](preuve/emulateur/phases/README.md) (8 PNG 720×400
d'empreintes distinctes + le journal série complet de la même exécution). C'est
la preuve visuelle que ce tableau n'est pas un récit : la phase 5, par exemple,
montre à l'écran `echantillons decodes (format 212 sur cible) : 3600` et
`alarmes techniques : 0`.

Précautions de lecture, à ne pas passer sous :
- **Écran VGA 80×25, console orientée octet.** Le scénario y présente les
  mesures sous forme ASCII (≤ 76 colonnes) : `pick` retire les octets non ASCII
  et coupe les lignes longues. C'est une **présentation**, pas un calcul — les
  journaux bruts (sortie complète de `veille-sim`, verdicts intégraux) restent
  dans la console série et dans `/dev/shmem/`. Les phrases accentuées du
  simulateur (`sûreté`, `prouvée`) perdent leurs lettres accentuées à l'écran ;
  les chiffres, eux, sont intacts.
- **E3 sous TCG, et non reproductible d'un boot à l'autre.** L'invité tourne sous
  QEMU *sans KVM* (le r630 a `/dev/kvm` mais l'unité `veille-qemu` ne l'utilise
  pas). Même binaire, même image, deux exécutions à 33 minutes d'intervalle :
  `maxdev_us=27832` à 16 h 19, `maxdev_us=51` à 16 h 52. L'écart ne vient pas du
  noyau QNX, il vient de la charge de l'hôte et de l'émulation CPU. La lecture
  honnête est donc : « la cadence demandée est tenue à ~0,02 % près (99,98 Hz)
  **et** à ~1 % près (99,04 Hz) selon l'exécution, sur un invité émulé » — pas
  « le tick QNX est exact à 10 ms ». Ce que la phase 7 établit sans ambiguïté,
  c'est la **mécanique** (timer noyau + pulses, pas `sleep()`), pas un chiffre.
- **E2 conditionnel.** `prio_effective=40` prouve que le noyau a hisse le
  serveur à la priorité de son client ; l'effet *contrefactuel* (sans héritage,
  le serveur serait préempté) reste ce qu'il est — voir §5.1.
---

## À ne pas confondre

| Ce que tu lis | Où ça tourne | Durée |
|---|---|---|
| §3.4a — 3600 échantillons, format 212 décodé en C | **dans l'invité QNX 8.0** | 10 s (extrait embarqué dans l'IFS) |
| §3.4b — 2401 BPM écrits dans `/dev/veille` | client Python **depuis l'hôte** | 30 min (record complet) |
| §3.4c — rejets MIT-BIH 100/105/205/222, BIDMC, Sleep-EDF | client Python **depuis l'hôte** | variable |

La phase 5 de la démonstration, c'est §3.4a. Ce n'est **pas** le rejeu de
30 minutes : l'invité n'a ni le fichier complet, ni Python, ni réseau.
