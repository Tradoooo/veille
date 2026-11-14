#!/bin/sh
# demo.sh — la démonstration Veille, en phases lisibles sur l'écran de l'invité.
#
# L'écran VGA de l'invité fait 80x25 : une démo qui défile n'est pas une démo
# visible. On joue donc QUATRE phases, chacune tenant dans l'écran (≤ 20
# lignes), séparées par une pause : à chaque instant, l'écran de l'émulateur
# montre une phase complète et lisible. Les phases rebouclent trois fois, puis
# l'invite interactive reste en place.
#
# Phases :
#   1. preuve     — exploration exhaustive : 4 800 états, borne 200 ms
#   2. simulation — 600 échantillons, pannes injectées, alarmes bornées
#   3. signal     — les 4 états capteur (figé / tachy / mort / régulier)
#   4. cible      — resource manager /dev/veille + client de mesure + verdict
#
# CONTRAINTES QNX 8.0 (invité SANS disque) — déjà payées :
#   - pas de TUBE : ce ksh ne peut pas en créer -> jamais `cmd | autre` ;
#   - pas de substitution de commande `$(...)` : elle utilise un tube ;
#   - pas de `printf` : ni builtin de ce ksh, ni applet dans l'image -> `echo` ;
#   - la console VGA est orientée OCTET (police CP437) : tout ce qui est
#     affiché est en ASCII (les messages du rm aussi, voir target/rm/rm_chain.c).
#
# JOURNAL : chaque phase est aussi copiée dans /dev/shmem/veille-demo.log, et le
# journal complet part sur la console SÉRIE (cat ... > /dev/ser1) pour que la
# vitrine HTTP l'affiche en texte à côté de l'écran.

PATH=/proc/boot:/system/bin
LD_LIBRARY_PATH=/proc/boot:/system/lib:/system/lib/dll
export PATH LD_LIBRARY_PATH HOME=/proc/boot

LOG=/dev/shmem/veille-demo.log
SIMOUT=/dev/shmem/veille-sim.out
SELOUT=/dev/shmem/veille-sel.out
OUT=/dev/shmem/veille-cmd.out
RMOUT=/dev/shmem/veille-rm.out
RELOUT=/dev/shmem/veille-reel.out
STATS=/dev/shmem/veille-stats.out
RAPPORT=/dev/shmem/veille-rapport.md
: > "$LOG"

# say : console VGA + journal du scenario + console serie.
# La serie est ecrite ligne a ligne : sans cela, la vitrine HTTP n'affiche le
# scenario qu'a la fin (le journal etant renvoye d'un seul bloc en fin de
# demonstration).
say() {
    echo "$*"
    echo "$*" >> "$LOG"
    echo "$*" > /dev/ser1
}

# champ : extrait un champ « cle=valeur » d'une ligne de mesure et l'imprime sur
# SA propre ligne. Indispensable là où deux expressions sed concurrentes se
# mangeaient mutuelles (le `.*` du second effaçait ce que le premier laissait) :
# une extraction par champ est la seule forme prévisible sur l'écran 80x25.
champ() {
    grep -o "$1=[0-9][0-9.]*" "$2" | head -1
}

# show : console seule (le texte est déjà dans le journal).
show() {
    echo "$*"
}

# pick : extrait d'un fichier les lignes qui contiennent un motif, vers un
# fichier (jamais de tube sur QNX 8.0 sans disque).
#
# Les sorties du simulateur sont en UTF-8 (accents) alors que la console VGA
# est orientee OCTET (CP437) : un accent UTF-8 y devient deux glyphes
# illisibles (« provedA© »). On retire donc les octets non ASCII de tout ce
# qui est affiche — c'est aussi pour cela que les messages du rm sont en ASCII.
# pick <motif> <fichier> [expression sed]
# Extrait les lignes qui contiennent le motif, les met en ASCII (console
# orientee octet), les compacte (espaces multiples -> un espace), eventuellement
# reformatees par l'expression sed, puis les coupe a 76 colonnes : l'ecran VGA
# fait 80 colonnes, au-dela la ligne se replie et devient illegible.
pick() {
    grep "$1" "$2" > "$SELOUT.raw" 2>/dev/null
    if [ "$#" -ge 3 ]; then
        sed -e 's/[^ -~]//g' -e "$3" -e 's/   */ /g' "$SELOUT.raw" > "$SELOUT.asc"
    else
        sed -e 's/[^ -~]//g' -e 's/   */ /g' "$SELOUT.raw" > "$SELOUT.asc"
    fi
    cut -c 1-76 "$SELOUT.asc" > "$SELOUT"
    cat "$SELOUT"
    cat "$SELOUT" > /dev/ser1
}

# phase : vide l'ecran puis affiche le titre, pour qu'une seule phase soit
# visible a la fois. La sequance d'echappement est litterale (octet 0x1B) :
# l'image n'a ni `printf` ni `clear`, et on ne depend pas de l'interpretation
# des anti-slashes par `echo`.
phase() {
    echo "[3J[2J[H"
    say "================================================================"
    say "  $1"
    say "================================================================"
}

pause() {
    sleep "$1"
}

# ---------------------------------------------------------------- 1. la preuve
phase_preuve() {
    phase "PHASE 1/8  PREUVE  -  exploration exhaustive des etats"
    say "  Borne d'alarme prouvee (B_proof)      : 200 ms"
    say "  Etats explores dans l'invite QNX 8.0   :"

    pick "VEILLE_PROOF_COMPLETE" "$SIMOUT"
    say ""
    say "  La garantie tient sur TOUS les etats, pas sur un echantillon."
    pause 6
}

# ------------------------------------------------------------- 2. la simulation
phase_simulation() {
    phase "PHASE 2/8  SIMULATION  -  600 echantillons, pannes injectees"
    pick "alarmes physiologiques" "$SIMOUT"
    pick "alarmes techniques" "$SIMOUT"
    pick "marge de" "$SIMOUT"
    pick "L_measured" "$SIMOUT"
    say ""
    say "  Latence MESUREE dans l'invite, comparee a la borne prouvee."
    pause 6
}

# ------------------------------------------------------------- 3. le signal brut
phase_signal() {
    phase "PHASE 3/8  SIGNAL BRUT  -  les 4 etats capteur"
    pick "capteur=" "$SIMOUT" 's/capteur=sensor_//'
    say ""
    say "  Capteur fige detecte sur le SIGNAL BRUT (pas sur le bpm)."
    say "  Tachycardie = alarme physiologique, pas fausse alarme technique."
    pause 6
}

# ---------------------------------------------------------------- 4. la cible
phase_cible() {
    phase "PHASE 4/8  CIBLE  -  resource manager /dev/veille + mesure"
    say "  /dev/veille en service (resource manager QNX, SCHED_FIFO 62)"
    bench-client samples --n 200 --interval-us 2000 --prio 10 --label vitrine \
        > "$OUT" 2>&1
    pick "fin label" "$OUT"
    pick "VEILLE_" "$OUT" 's/^\(....\)[ ]*VEILLE_\([A-Z_]*\).*bound=\([0-9]*\).*/\1 VEILLE_\2  borne=\3 ms/'
    say ""
    say "  Verdict de la chaine apres la deconnexion du client :"
    bench-client verdict > "$OUT" 2>&1
    pick "VEILLE_" "$OUT" 's/^\(....\)[ ]*VEILLE_\([A-Z_]*\).*bound=\([0-9]*\).*/\1 VEILLE_\2  borne=\3 ms/'
    say ""
    say "  Alarme technique 'mort d'un pair' declenchee a la deconnexion."
    pause 6
}

# ------------------------------------------------------- 5. le signal RÉEL (222)
# Les octets du record mitdb/222 (fibrillation atriale) sont DANS l'IFS ; le
# décodage format 212, l'état capteur, la fréquence et la décision se font dans
# l'invité. Le critère attendu sur ce signal : AUCUNE alarme technique (le rythme
# est irrégulier mais vivant) — c'est exactement le défaut que les données réelles
# ont fait tomber (voir docs/LIMITS.md : la dérive respiratoire n'est pas une
# panne d'électrode).
phase_reel() {
    phase "PHASE 5/8  SIGNAL REEL  -  mitdb/222 (fibrillation atriale)"
    replay-ecg --sortie "$RELOUT" > "$OUT" 2>&1
    RECODE=$?
    pick "enregistrement" "$OUT"
    pick "echantillons decodes" "$OUT"
    pick "frequences rapportees" "$OUT"
    pick "etat capteur" "$OUT"
    pick "alarmes techniques  " "$OUT"
    pick "latence chaine" "$OUT"
    say ""
    say "  Decodage format 212, etat capteur et frequence : DANS l'invite."
    if [ "$RECODE" -eq 0 ]; then
        say "  Rythme irregulier vivant : 0 fausse alarme technique."
    else
        say "  ATTENTION : $RECODE signale une alarme technique sur ce signal."
    fi
    pause 6
}

# ---------------------------------------- 6. E2 : héritage de priorité sous charge
# Un hog à priorité 30 occupe le CPU ; un client à 40 appelle /dev/veille, servi
# par un rm à priorité de base 10. Sans héritage, le rm est préempté par le hog ;
# avec, le noyau le hisse à 40. La mesure est la priorité EFFECTIVE relevée par
# le serveur lui-même au moment de la réception (pthread_getschedparam).
phase_charge() {
    phase "PHASE 6/8  CHARGE  -  E2 : heritage de priorite du noyau"
    say "  rm a prio 10, hog a prio 30, client a prio 40"
    slay veille-rm
    sleep 1
    veille-rm --no-tick --prio 10 > "$RMOUT" 2>&1 &
    sleep 2
    hog --prio 30 --hold-ms 20 --sleep-ms 1 --duration-ms 4000 > /dev/null 2>&1 &
    bench-client samples --n 120 --interval-us 2000 --prio 40 --label e2-charge \
        > "$OUT" 2>&1
    pick "fin label" "$OUT"
    bench-client stats > "$STATS" 2>&1
    pick "prio_base" "$STATS"
    pick "prio_effective" "$STATS"
    pick "lat_write_max_us" "$STATS"
    say ""
    say "  prio_effective = 40 : le noyau a hisse le serveur a son client."
    say "  Sans heritage, le rm restait a 10 et le hog (30) le preemptait."
    pause 6
    slay veille-rm
    sleep 1
    veille-rm > "$RMOUT" 2>&1 &
    sleep 2
}

# ------------------------------------------------------- 7. E3 : cadence du tick
# Le timer noyau (timer_create + SIGEV_PULSE, 10 ms) est la cadence d'acquisition
# : pas de sleep() applicatif. La sonde indépendante (tick-probe) compte les pulses
# sur PROPRE canal : elle distingue un timer qui fire vraiment d'un pulse perdu ou
# coalisé — le rm, lui, ne voit que ce qu'il sert.
phase_cadence() {
    phase "PHASE 7/8  CADENCE  -  E3 : tick noyau, sonde independante"

    # Une mesure par ligne : c'est le FORMAT du binaire (--format lignes), pas une
    # découpe faite ici. Deux pièges payés : la ligne unique de tick-probe fait 118
    # colonnes (l'écran en fait 80 — la fin, maxdev_us et coalesced, sautait) et
    # `grep -o` n'existe pas dans le toybox de l'image (une extraction par champ
    # n'y rendait donc rien, sans message d'erreur).
    tick-probe --period-ms 10 --duration-ms 3000 --prio 62 --format lignes > "$OUT" 2>&1
    pick "pulses=" "$OUT"
    pick "taux_hz=" "$OUT"
    pick "maxdev_us=" "$OUT"
    pick "coalesced=" "$OUT"
    bench-client stats > "$STATS" 2>&1
    pick "tick_period_us=" "$STATS"
    say ""
    say "  pulses : pulses servis sur le canal PROPRE de la sonde (pas le rm)."
    say "  taux_hz : cadence reellement produite par le timer noyau."
    say ""
    say "  Cadence produite par le noyau (timer_create + pulse), pas par sleep()."
    say "  maxdev_us : plus grand ecart a la periode demandee (10 ms)."
    say "  A savoir : l'invite tourne sous QEMU sans KVM (emulation CPU) — l'ecart"
    say "  inclut ce surcout. Sur machine cible reelle, il est d'un autre ordre."
    pause 6
}

# ------------------------------- 8. E4 : capteur muet + rapport d'audit (S5/M5)
# Deux choses en une phase : (a) le watchdog « perte de signal » — le rm, servi
# par un timer noyau indépendant, alarme dans B_tech quand le capteur se tait ;
# (b) le rapport d'audit d'IEC 62304, produit SUR la cible par core/report.c avec
# l'empreinte SHA-256 de la configuration (core/sha256.c, pur C11).
phase_muet() {
    phase "PHASE 8/8  CAPTEUR MUET + RAPPORT D'AUDIT"
    slay veille-rm
    sleep 1
    veille-rm --no-tick --watchdog-ms 800 > "$RMOUT" 2>&1 &
    sleep 2
    say "  rm relance sans capteur interne, watchdog 800 ms"
    bench-client hold --silence-ms 1200 --duration-ms 600 > "$OUT" 2>&1
    # Le journal du client porte deux verdicts : PENDANT le silence, puis APRES le
    # retour du signal. Les deux sont intéressants : l'alarme doit monter, puis se
    # LEVER d'elle-même (pas d'acquittement manuel).
    # core/report.c OMET un champ quand sa valeur est 0 (contrat §5.5 : « clés
    # optionnelles omises quand absentes ») — latency=0 n'apparaît donc pas dans
    # le verdict OK. Un motif qui l'exigerait ne matcherait pas, et n'afficherait
    # RIEN sans message d'erreur : c'est exactement ce qui est arrivé à l'écran au
    # premier boot. Les motifs ci-dessous ne dépendent que de champs toujours présents.
    pick "VEILLE_TECHNICAL_ALARM" "$OUT" 's/^\(....\)[ ]*VEILLE_TECHNICAL_ALARM.*state=\([^|]*\).*bound=\([0-9]*\).*/\1 TECHNIQUE  capteur=\2  borne=\3 ms/'
    pick "VEILLE_OK" "$OUT" 's/^\(....\)[ ]*VEILLE_OK.*bound=\([0-9]*\).*/\1 OK  borne=\2 ms/'
    bench-client stats > "$STATS" 2>&1
    pick "wd_count=" "$STATS"
    pick "wd_fire_silence_ms=" "$STATS"
    say ""
    say "  Le silence du capteur est une alarme TECHNIQUE bornee, pas un trou."
    say "  wd_count : alarmes declenchees ; wd_fire_silence_ms : silence au tir."
    say "  --- rapport d'audit (cible) : P1 sur 4800 etats ---"
    replay-ecg --preuve --rapport markdown --quiet > "$RAPPORT" 2>&1
    # Le rapport sort en UTF-8 (core/report.c — la syntaxe du CONTRAT, pas une
    # réimplementation) ; la console VGA est orientée OCTET : `pick` y retire les
    # octets non ASCII, ce qui rend illisibles les titres accentués ("Synthèse" →
    # "Synth se"). On cueille donc les lignes dont la CLÉ est en ASCII, et on
    # réécrit l'étiquette en ASCII : l'écran reste lisible, le contenu est la
    # sortie brute de core/report.c (aucune valeur inventée).
    pick "SHA-256" "$RAPPORT" 's/^- [^:]*: /  empreinte config : /;s/^\(  empreinte config : ................\).*$/\1.../'
    pick "exploration exhaustive" "$RAPPORT" 's/^- [^:]*: /  methode  : /;s/4800 tats/4800 etats/'
    pick "borne tenue" "$RAPPORT" 's/^- [^:]*: /  resultat : /'
    pause 6
    slay veille-rm
    sleep 1
    veille-rm > "$RMOUT" 2>&1 &
    sleep 2
}

# ------------------------------------------------------------------- le scenario
say "=== Veille - invite QNX 8.0 : demonstration complete ==="
say "    (8 phases lisibles, l'ecran VGA fait 80x25)"

# 1) La simulation complete est jouee UNE fois ; les phases relisent le
#    resultat dans le fichier (pas de tube, pas de substitution de commande).
say ""
say "  [preparation] veille-sim : preuve + simulation + signaux bruts..."
veille-sim > "$SIMOUT" 2>&1
say "  [preparation] termine."

# 2) Le resource manager, une fois pour toute la demonstration.
veille-rm > "$RMOUT" 2>&1 &
RM_PID=$!
sleep 2
say "  [preparation] /dev/veille en service (pid $RM_PID)"

# 3) Les phases, en boucle : deux tours de huit phases (~2 min 40), puis
#    l'invite interactive reste en place pour les touches envoyées depuis la
#    vitrine.
CYCLE=1
while [ "$CYCLE" -le 2 ]; do
    say ""
    say "---------------- cycle $CYCLE / 2 ----------------"
    phase_preuve
    phase_simulation
    phase_signal
    phase_cible
    phase_reel
    phase_charge
    phase_cadence
    phase_muet
    CYCLE=$((CYCLE + 1))
done

say ""
say "=== demonstration terminee : l'invite reste interactive ==="
say "    (les commandes tapes ici vont a l'invite QNX 8.0)"

# Le journal du scenario part sur la console serie : la vitrine web l'affiche
# en texte (la console VGA, elle, garde la derniere image).
cat "$LOG" > /dev/ser1

# Le scenario est termine : la session de console (le shell interactif qui
# nous a lance via son .profile) rend la main : l'invite attend les touches
# injectees depuis la vitrine.
