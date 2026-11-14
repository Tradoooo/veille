/* Veille — sorties du rejeu de données réelles (target/replay/replay_rapport.c).
 *
 * Séparé du conducteur (replay_ecg.c) pour tenir la règle de taille : ici il
 * n'y a que de la MISE EN FORME — la chaîne, elle, est dans core/ et le
 * pipeline partagé dans target/pipe/. Une seule implémentation par chose.
 *
 * Les formats de verdict (texte, JSON, Markdown) viennent de core/report.c : ce
 * fichier ne réinvente jamais la syntaxe du CONTRAT, il fournit seulement les
 * méta-données (empreinte, horodatage, version) que core/ ne peut pas lire
 * lui-même (core/ est pur : ni horloge, ni fichier, ni crypto).
 *
 * Empreinte SHA-256 : calculée SUR LA CIBLE par core/sha256.c (pur C11, aucune
 * dépendance) sur la forme canonique de la configuration. C'est l'empreinte
 * citée dans le rapport d'audit — celle qu'un dossier IEC 62304 doit référencer
 * pour dater la configuration qui a servi à la preuve. Le jumeau Python
 * (hashlib) produit la MÊME empreinte : tests/differential.py le vérifie. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mitdb222_212.h"
#include "replay_ecg.h"
#include "veille/chain.h"
#include "veille/report.h"
#include "veille/signal.h"
#include "veille/sha256.h"
#include "veille/state.h"

/* Forme canonique de la configuration : une ligne par champ, ordre figé.
 * C'est l'objet dont on prend l'empreinte — donc l'empreinte change si la
 * moindre borne bouge, ce qui est exactement le but. */
long replay_config_canonique(const veille_chain_config *c, char *buf, size_t cap) {
    long n = 0;
    n += snprintf(buf + n, cap - n, "veille_version=%s\n", VEILLE_REPLAY_VERSION);
    n += snprintf(buf + n, cap - n, "priority=%s\n", veille_priority_name(c->priority));
    n += snprintf(buf + n, cap - n, "bound_alarm_ms=%ld\n", c->bound_alarm_ms);
    n += snprintf(buf + n, cap - n, "bound_ack_ms=%ld\n", c->bound_ack_ms);
    n += snprintf(buf + n, cap - n, "bound_tech_ms=%ld\n", c->bound_tech_ms);
    n += snprintf(buf + n, cap - n, "tech_detect_ms=%ld\n", c->tech_detect_ms);
    for (int s = 0; s < VEILLE_STAGE__COUNT; s++)
        n += snprintf(buf + n, cap - n, "stage_%s_ms=%ld\n",
                      veille_stage_name((veille_stage)s), c->stage_latency_ms[s]);
    return (n < (long)cap) ? n : (long)cap - 1;
}

/* Empreinte SHA-256 hexadécimale (64 caractères + NUL) de la config. */
static void config_sha256(const veille_chain_config *c, char out[VEILLE_SHA256_HEXLEN]) {
    char buf[512];
    long n = replay_config_canonique(c, buf, sizeof(buf));
    veille_sha256_hex(buf, (size_t)n, out);
}

void replay_print_config(const veille_chain_config *c, const char *record,
                         long fs_hz, long n_groupes, const char *sha256_record) {
    char sha[VEILLE_SHA256_HEXLEN];
    config_sha256(c, sha);
    char buf[512];
    replay_config_canonique(c, buf, sizeof(buf));
    fputs(buf, stdout);
    printf("record=%s\n", record);
    printf("fs_hz=%ld\n", fs_hz);
    printf("n_groupes=%ld\n", n_groupes);
    printf("sha256_record=%s\n", sha256_record);
    printf("sha256_config=%s\n", sha);
}

/* Masque d'exploration complet (toutes les dimensions) — une seule définition,
 * partagée avec la simulation (target/sim/sim.c fait de même via core/). */
void replay_masque_complet(veille_fault_mask *m) {
    m->explore_sensor = 1; m->explore_bus = 1; m->explore_prio = 1;
    m->explore_alarm = 1; m->explore_stage = 1;
}

/* Verdict correspondant à un résultat de pipeline, au format du CONTRAT. */
veille_verdict replay_verdict_of(veille_pipe_result r, long lat_ms,
                                 veille_sensor_state sensor,
                                 const veille_chain_config *cfg) {
    static char etat[128];
    veille_fault_state fs;
    memset(&fs, 0, sizeof(fs));
    fs.sensor = sensor;
    fs.alarm = (r == VEILLE_PIPE_ALARM) ? VEILLE_ALARM_SINGLE : VEILLE_ALARM_NONE;
    veille_state_to_string(&fs, etat, sizeof(etat));

    veille_verdict v;
    memset(&v, 0, sizeof(v));
    int technique = (r != VEILLE_PIPE_ALARM && r != VEILLE_PIPE_OK);
    v.code = technique ? VEILLE_TECHNICAL_ALARM
                       : (lat_ms <= cfg->bound_alarm_ms ? VEILLE_OK
                                                        : VEILLE_LATENCY_BOUND_EXCEEDED);
    v.sev = veille_verdict_default_severity(v.code);
    v.message = technique ? veille_sensor_state_name(sensor)
                          : (r == VEILLE_PIPE_ALARM ? "alarme haute priorite - seuil franchi"
                                                    : "echantillon traite");
    v.chain_id = "reel/mitdb222";
    v.stage = veille_stage_name(technique ? VEILLE_STAGE_ACQUIRE : VEILLE_STAGE_ALARM);
    v.state = etat;
    v.threshold = VEILLE_HR_THRESHOLD;
    v.bound_ms = technique ? cfg->bound_tech_ms : cfg->bound_alarm_ms;
    v.latency_ms = lat_ms;
    return v;
}

void replay_print_bilan(const replay_opts *o, const replay_bilan *b,
                        const veille_chain_config *cfg) {
    long moyenne = b->n_bpm ? b->bpm_sum / b->n_bpm : 0;
    printf("--- rejeu de signal REEL (cible QNX 8.0, sans Python) ---\n");
    printf("enregistrement     : %s, canal %d, %d Hz\n",
           MITDB222_RECORD, o->canal, MITDB222_FS_HZ);
    printf("echantillons decodes (format 212 sur cible) : %ld\n", b->n_ech);
    printf("frequences rapportees : %ld (min %ld / moyen %ld / max %ld bpm)\n",
           b->n_bpm, b->bpm_min, moyenne, b->bpm_max);
    printf("etat capteur deduit du SIGNAL BRUT : %s\n",
           veille_sensor_state_name(b->sensor));
    printf("alarmes physiologiques hautes : %ld (seuil %d bpm, borne %ld ms)\n",
           b->alarmes, VEILLE_HR_THRESHOLD, cfg->bound_alarm_ms);
    printf("alarmes techniques            : %ld (borne %ld ms)\n",
           b->techniques, cfg->bound_tech_ms);
    printf("latence chaine mesuree sur cible : max %ld.%03ld ms / moyenne %ld.%03ld ms"
           " (%ld echantillons)\n",
           b->lat_max_us / 1000, b->lat_max_us % 1000,
           b->n_ech ? b->lat_sum_us / b->n_ech / 1000 : 0,
           b->n_ech ? (b->lat_sum_us / b->n_ech) % 1000 : 0, b->n_ech);
    if (b->techniques == 0 && b->n_bpm > 0)
        printf("[OK] rythme irregulier vivant : aucune fausse alarme capteur fige.\n");
    else if (b->techniques > 0)
        printf("[ATTENTION] %ld alarme(s) technique(s) sur ce signal reel.\n", b->techniques);
}

/* Rapport d'audit dans le format demandé (texte | json | markdown), produit par
 * core/report.c — la syntaxe du CONTRAT, pas une réimplementation. */
void replay_print_verdicts(const replay_opts *o, const veille_verdict *vs, size_t nv,
                           const veille_chain_config *cfg) {
    if (!o->rapport || nv == 0) return;
    char sha[VEILLE_SHA256_HEXLEN], ts[32];
    config_sha256(cfg, sha);
    time_t t = time(NULL);
    struct tm gm;
    gmtime_r(&t, &gm);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &gm);
    veille_report_meta meta = { "target/replay/mitdb222_212.h", sha, ts,
                                VEILLE_REPLAY_VERSION };
    static char buf[8192];
    long len;
    if (!strcmp(o->rapport, "json"))            len = veille_report_json(vs, nv, buf, sizeof(buf));
    else if (!strcmp(o->rapport, "markdown"))   len = veille_report_markdown(vs, nv, &meta, buf, sizeof(buf));
    else                                       len = veille_report_text(vs, nv, buf, sizeof(buf));
    if (len < 0) { fprintf(stderr, "replay-ecg: rapport refusé par core/report.c\n"); return; }
    if (len > (long)sizeof(buf)) len = (long)sizeof(buf);
    printf("--- verdicts (%s, %zu premiers événements sur %zu) ---\n", o->rapport, nv, nv);
    printf("%.*s", (int)len, buf);
}
