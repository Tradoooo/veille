#ifndef REPLAY_ECG_H
#define REPLAY_ECG_H

/* Types partagés du rejeu de données réelles sur cible (target/replay/).
 *
 * Séparé du conducteur (replay_ecg.c) pour que chaque fichier reste sous 500
 * lignes et chaque fonction sous 50 : le bilan (bilan_t) et les options
 * (opts_t) sont manipulés par le conducteur ET par le rapporteur
 * (replay_rapport.c) — une seule définition, pas de duplication. */
#include <stddef.h>

#include "veille/proof.h"    /* veille_fault_mask */
#include "veille/report.h"
#include "veille/state.h"
#include "veille_pipe.h"

#define VEILLE_REPLAY_VERSION "0.3"
#define VEILLE_REPLAY_MAX_VERDICTS 8   /* 3 premiers + pire cas : tampon borné */

/* Options de ligne de commande. */
typedef struct {
    const char *sortie;        /* fichier recevant les lignes « bpm <v> » (NULL = stdout) */
    const char *rapport;       /* "texte" | "json" | "markdown" | NULL (bilan seul) */
    int   canal;               /* 0 = MLII, 1 = V1 (canaux entrelacés du format 212) */
    int   duree_s;             /* 0 = tout l'extrait embarqué */
    int   temps_reel;          /* espacer les écritures à l'intervalle cardiaque réel */
    int   config;              /* 1 = forme canonique + empreinte, puis sort */
    int   canon;               /* 1 = forme canonique SEULE (sortie machine) */
    int   quiet;               /* 1 = pas du bilan humain (sortie machine seulement) */
    int   preuve;              /* 1 = rapport d'audit de la PREUVE (S5), pas du rejeu */
} replay_opts;

/* Bilan d'un rejeu : ce qui est MESURÉ, pas simulé. */
typedef struct {
    long n_ech;                /* échantillons bruts décodés et poussés */
    long n_bpm;                /* fréquences rapportées (changements mesurés) */
    long bpm_min, bpm_max, bpm_sum;
    long alarmes;              /* passages en alarme physiologique haute */
    long techniques;           /* passages en alarme technique (capteur) */
    long lat_max_us, lat_sum_us;
    long lat_pire_bpm;         /* fréquence échantillonnée au moment du pire cas */
    veille_sensor_state sensor;/* dernier état capteur déduit du signal brut */
} replay_bilan;

/* replay_ecg.c */
void replay_opts_init(replay_opts *o);
int  replay_parse_args(int argc, char **argv, replay_opts *o);
void replay_bilan_init(replay_bilan *b);
void replay_note(replay_bilan *b, veille_pipe_result r, long bpm, long lat_us);

/* replay_rapport.c */
void replay_masque_complet(veille_fault_mask *m);
/* Forme CANONIQUE de la configuration gelée : une ligne par champ, ordre figé.
 * C'est l'objet dont on prend l'empreinte SHA-256 — donc l'empreinte bouge dès
 * qu'une borne change. Exposée publique pour que tests/differential.py la hache
 * AUSSI avec hashlib (Python) et compare les deux empreintes : le C et le
 * jumeau Python doivent dater la même configuration, octet pour octet.
 * Retourne le nombre d'octets qui auraient été écrits (comme snprintf). */
long replay_config_canonique(const veille_chain_config *c, char *buf, size_t cap);

/* Verdict correspondant à un résultat de pipeline, au format du CONTRAT.
 * `bpm` ne figure PAS dans le verdict : le CONTRAT porte `threshold` (le seuil
 * d'alarme), pas la fréquence mesurée — celle-ci est dans le bilan. */
veille_verdict replay_verdict_of(veille_pipe_result r, long lat_ms,
                                 veille_sensor_state sensor,
                                 const veille_chain_config *cfg);
void replay_print_config(const veille_chain_config *cfg, const char *record,
                         long fs_hz, long n_groupes, const char *sha256);
void replay_print_bilan(const replay_opts *o, const replay_bilan *b,
                        const veille_chain_config *cfg);
void replay_print_verdicts(const replay_opts *o, const veille_verdict *vs, size_t nv,
                           const veille_chain_config *cfg);

#endif /* REPLAY_ECG_H */
