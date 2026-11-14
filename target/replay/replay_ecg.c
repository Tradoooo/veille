/* Veille — rejeu de DONNÉES RÉELLES sur la cible QNX 8.0, sans Python.
 *
 * Ce que ça prouve : la chaîne capteur→alarme de Veille tourne dans l'invité
 * avec un signal qui n'est PAS généré. Les octets du record 222 de MIT-BIH
 * (fibrillation atriale, bpm 43 → 182) sont embarqués dans l'IFS
 * (target/replay/mitdb222_212.h, SHA-256 publié vérifié à la génération) et
 * DÉCODÉS sur la cible par target/replay/wfdb212.h — le même algorithme que le
 * jumeau Python host/veille/wfdb.py, donc voie comparative et pas duplication.
 *
 * Pourquoi cet outil manquait : l'invité de démonstration n'a ni disque, ni
 * réseau, ni Python. Le rejeu de données réelles se faisait donc depuis l'hôte
 * (python3 -m veille.resmgr_client) et n'apparaissait JAMAIS à l'écran de
 * l'émulateur. Ici tout — décodage 212, état capteur, fréquence, décision,
 * verdict, empreinte — s'exécute dans QNX 8.0.
 *
 * Le signal traverse EXACTEMENT le pipeline partagé (veille_pipe_push_sample) :
 * état capteur déduit des ÉCHANTILLONS BRUTS, fréquence déduite des pics R,
 * décision de seuil. C'est le chemin qui porte la preuve du défaut corrigé :
 * sur un rythme irrégulier (FA), deux bpm arrondis égaux ne doivent PAS produire
 * d'alarme « capteur figé ». Le comptage `techniques` du bilan est ce que le
 * jury doit regarder : il vaut 0 sur un enregistrement sain OU irrégulier.
 *
 * Modes :
 *   replay-ecg                      bilan du rejeu sur stdout
 *   replay-ecg --sortie FICHIER     lignes « bpm <v> » dans FICHIER
 *                                   (typiquement /dev/veille : le resource manager)
 *   replay-ecg --rapport MARKDOWN   rapport d'audit (core/report.c) + empreinte
 *   replay-ecg --config             forme canonique de la config + SHA-256
 *
 * Options : --canal N (0=MLII, 1=V1) · --duree-s N · --temps-reel (rythme cardiaque
 * réel entre les écritures) · --quiet (pas du bilan, des verdicts seulement).
 *
 * Build : tools/qnx/build.sh (qcc) — voir docs/QNX-BUILD.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mitdb222_212.h"
#include "replay_ecg.h"
#include "wfdb212.h"
#include "veille/chain.h"
#include "veille/report.h"
#include "veille/signal.h"
#include "veille/state.h"
#include "veille_pipe.h"
#include "port_time.h"

void replay_opts_init(replay_opts *o) {
    o->sortie = NULL; o->rapport = NULL;
    o->canal = 0; o->duree_s = 0; o->temps_reel = 0; o->config = 0; o->quiet = 0;
    o->canon = 0; o->preuve = 0;
}

int replay_parse_args(int argc, char **argv, replay_opts *o) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--sortie") && i + 1 < argc) o->sortie = argv[++i];
        else if (!strcmp(argv[i], "--canal") && i + 1 < argc) o->canal = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--duree-s") && i + 1 < argc) o->duree_s = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rapport") && i + 1 < argc) o->rapport = argv[++i];
        else if (!strcmp(argv[i], "--temps-reel")) o->temps_reel = 1;
        else if (!strcmp(argv[i], "--config")) o->config = 1;
        else if (!strcmp(argv[i], "--canon")) o->canon = 1;
        else if (!strcmp(argv[i], "--quiet")) o->quiet = 1;
        else if (!strcmp(argv[i], "--preuve")) o->preuve = 1;
        else { fprintf(stderr, "option inconnue : %s\n", argv[i]); return 2; }
    }
    if (o->canal < 0 || o->canal >= WFDB212_CANAL_MAX) {
        fprintf(stderr, "canal hors plage (0..%d)\n", WFDB212_CANAL_MAX - 1);
        return 2;
    }
    return 0;
}

void replay_bilan_init(replay_bilan *b) {
    memset(b, 0, sizeof(*b));
    b->bpm_min = -1;
}

/* Un échantillon traverse la chaîne ; le bilan porte ce qui est MESURÉ. */
void replay_note(replay_bilan *b, veille_pipe_result r, long bpm, long lat_us) {
    b->n_ech++;
    b->lat_sum_us += lat_us;
    if (lat_us > b->lat_max_us) { b->lat_max_us = lat_us; b->lat_pire_bpm = bpm; }
    if (bpm > 0) {
        if (b->bpm_min < 0 || bpm < b->bpm_min) b->bpm_min = bpm;
        if (bpm > b->bpm_max) b->bpm_max = bpm;
        b->bpm_sum += bpm;
        b->n_bpm++;
    }
    if (r == VEILLE_PIPE_ALARM) b->alarmes++;
    if (r == VEILLE_PIPE_TECH_STUCK || r == VEILLE_PIPE_TECH_DEAD ||
        r == VEILLE_PIPE_TECH_NOISY || r == VEILLE_PIPE_TECH_DRIFT) b->techniques++;
}

/* Nombre de groupes à rejouer : tout l'extrait, ou --duree-s borné. */
static long groupes_effectifs(const replay_opts *o) {
    long n = MITDB222_N_GROUPES;
    if (o->duree_s > 0) {
        long demande = (long)o->duree_s * MITDB222_FS_HZ;
        if (demande < n) n = demande;
    }
    return n;
}

/* Espacement « temps réel » : l'intervalle cardiaque correspondant au bpm. */
static void attendre_us(long bpm) {
    if (bpm <= 0) return;
    long us = 60L * 1000000L / bpm;
    struct timespec ts = { us / 1000000L, (us % 1000000L) * 1000L };
    nanosleep(&ts, NULL);
}

/* Une fréquence nouvelle est écrite vers la sortie (jamais deux identiques :
 * c'est ce qui évite au resource manager d'y voir une valeur figée). */
static void ecrire_frequence(FILE *out, long bpm, const replay_opts *o, long *dernier) {
    if (!out || bpm <= 0 || bpm == *dernier) return;
    fprintf(out, "bpm %ld\n", bpm);
    fflush(out);
    *dernier = bpm;
    if (o->temps_reel) attendre_us(bpm);
}

/* Le rejeu lui-même : décodage 212 → pipeline partagé → bilan → verdicts. */
static void rejouer(const replay_opts *o, const veille_chain_config *cfg, FILE *out,
                    replay_bilan *b, veille_verdict *vs, size_t *nv) {
    veille_pipe_state ps;
    veille_pipe_state_init(&ps);
    veille_pipe_set_fs(&ps, MITDB222_FS_HZ);
    replay_bilan_init(b);
    *nv = 0;
    long dernier = -1;
    long n = groupes_effectifs(o);
    for (long g = 0; g < n; g++) {
        int16_t canaux[WFDB212_CANAL_MAX];
        wfdb212_decode(&mitdb222_212[g * WFDB212_OCTETS_PAR_GROUPE], canaux);
        long t0 = veille_now_us();
        veille_pipe_result r = veille_pipe_push_sample(&ps, canaux[o->canal]);
        long lat_us = veille_now_us() - t0;
        long bpm = veille_pipe_rate_bpm(&ps);
        b->sensor = veille_pipe_sensor_state(&ps);
        replay_note(b, r, bpm, lat_us);
        if (*nv < VEILLE_REPLAY_MAX_VERDICTS && r != VEILLE_PIPE_OK)
            vs[(*nv)++] = replay_verdict_of(r, lat_us / 1000, b->sensor, cfg);
        ecrire_frequence(out, bpm, o, &dernier);
    }
}

/* Rapport d'audit de la preuve : exploration exhaustive → verdicts → format
 * demandé (texte | json | markdown). Retourne 0 si la borne est tenue partout. */
static int rapport_preuve(const replay_opts *o, const veille_chain_config *cfg) {
    veille_fault_mask mask;
    replay_masque_complet(&mask);
    veille_verdict vs[2];
    size_t nv = 0;
    int rc = veille_proof_explore(cfg, &mask, vs, 2, &nv);
    if (!o->quiet)
        printf("--- rapport d'audit (cible QNX 8.0) : exploration exhaustive ---\n");
    replay_print_verdicts(o, vs, nv, cfg);
    if (o->quiet || !o->rapport) {
        char buf[512];
        long len = veille_report_text(vs, nv, buf, sizeof(buf));
        if (len > 0) printf("%.*s", (int)(len < (long)sizeof(buf) ? len : (long)sizeof(buf)), buf);
    }
    return rc < 0 ? 1 : 0;
}

int main(int argc, char **argv) {
    replay_opts o;
    replay_opts_init(&o);
    if (replay_parse_args(argc, argv, &o) != 0) return 2;

    veille_chain_config cfg;
    veille_pipe_default_config(&cfg);
    if (o.canon) {                       /* sortie machine : la forme seule */
        char buf[512];
        long n = replay_config_canonique(&cfg, buf, sizeof(buf));
        if (n > (long)sizeof(buf)) n = (long)sizeof(buf);
        fwrite(buf, 1, (size_t)n, stdout);
        return 0;
    }
    if (o.config) {
        replay_print_config(&cfg, MITDB222_RECORD, MITDB222_FS_HZ,
                            MITDB222_N_GROUPES, MITDB222_SHA256);
        return 0;
    }

    /* Mode rapport d'audit (S5/M5) : la PREUVE par exploration, mise en forme par
     * core/report.c avec empreinte SHA-256 de la configuration — le tout SUR la
     * cible. Rien n'est lu sur un fichier : la configuration est celle, gelée, du
     * CONTRAT, et l'empreinte est calculée par core/sha256.c (pur C11). */
    if (o.preuve) return rapport_preuve(&o, &cfg);

    FILE *out = NULL;
    if (o.sortie) {
        out = fopen(o.sortie, "w");
        if (!out) { perror("replay-ecg: sortie"); return 1; }
    }
    replay_bilan b;
    veille_verdict vs[VEILLE_REPLAY_MAX_VERDICTS];
    size_t nv = 0;
    rejouer(&o, &cfg, out, &b, vs, &nv);
    if (out) fclose(out);

    if (!o.quiet) replay_print_bilan(&o, &b, &cfg);
    replay_print_verdicts(&o, vs, nv, &cfg);
    /* Code de sortie : 0 si aucune alarme technique sur un signal réel — le
     * verdict attendu pour un rythme irrégulier mais vivant. */
    return b.techniques > 0 ? 1 : 0;
}
