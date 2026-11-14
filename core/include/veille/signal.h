#ifndef VEILLE_SIGNAL_H
#define VEILLE_SIGNAL_H

/* Signal BRUT → état capteur → fréquence (niveau signal, pas niveau bpm).
 *
 * Le défaut corrigé : « capteur figé » détecté sur le bpm dérivé (deux bpm
 * arrondis égaux) déclenchait une fausse alarme technique sur un rythme
 * régulier (fibrillation atriale, tachycardie). La détection porte désormais
 * sur les échantillons bruts (codes ADC signés) : un signal réellement plat
 * est figé, un rythme constant mais vivant ne l'est pas.
 *
 * core/ reste pur : ni horloge, ni fichier, ni allocation (tampons fournis
 * par l'appelant). Le jumeau Python (host/veille/state.py) fait EXACTEMENT
 * la même chose — le différentiel C↔Python le vérifie.
 */

#include "veille/state.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fenêtre d'analyse : 512 échantillons (~2 s à 250 Hz). Assez large pour
 * contenir un battement à 60 bpm (période 250 échantillons) — sinon un
 * ECG Healthy entre deux QRS passerait pour « plat ». */
#define VEILLE_SIG_WINDOW_MAX 512
#define VEILLE_SIG_MAX_BEATS  32      /* battements mémorisés (borné, pas d'allocation) */

/* DÉRIVE — ancrages de la ligne de base, un par SPAN de fenêtre (512 échantillons,
 * ~2 s à 250 Hz). Une dérive n'est une défaillance du capteur que si la ligne de
 * base monte ou descend SANS INVERSION sur cinq spans (~10 s).
 *
 * Pourquoi cinq, chiffré : mesuré le 2026-11-14 sur trois enregistrements RÉELS
 * de MIT-BIH (100, 205, 222 — 40 s chacun, 19 968 lectures par record), le critère
 * à un seul span classait 31,8 % / 31,3 % / 17,9 % des lectures en « capteur qui
 * dérive ». C'est la dérive RESPIRATOIRE du patient, pas une panne d'électrode :
 * un cycle respiratoire (~4 s) tient dans deux spans, donc deux ancres ne le
 * filtrent pas ; cinq ancres (10 s, plus de deux cycles) le filtrent — 0 % de faux
 * positifs sur les trois records, et la dérive synthétique (rampe de 200 codes/s,
 * core/signal_gen.c) reste détectée à 100 % une fois les cinq ancres en place.
 *
 * Coût assumé et documenté (docs/LIMITS.md) : la détection de dérive a un TEMPS
 * D'INSTALLATION de cinq spans (~10 s). C'est une défaillance LENTE : elle n'est
 * pas bornée par B_tech (50 ms), qui reste la borne des événements brusques
 * (capteur mort, capteur figé, déconnexion d'un pair). */
#define VEILLE_SIG_ANCRAGES    5

/* Seuil physiologique d'alarme haute (bpm) — même valeur que la chaîne bpm. */
#define VEILLE_SIG_HR_THRESHOLD 120

/* Décision de la chaîne au niveau signal (filter→decide). */
typedef enum {
    VEILLE_DECISION_OK = 0,        /* rien à signaler */
    VEILLE_DECISION_ALARM_HIGH,    /* alarme physiologique haute (bpm > seuil) */
    VEILLE_DECISION_TECH_FAULT     /* alarme technique (capteur) */
} veille_decision;

/* Tampon d'échantillons bruts (chronologique) + paramètres d'analyse. */
typedef struct {
    int16_t buf[VEILLE_SIG_WINDOW_MAX];  /* fenêtre glissante, ordre chronologique */
    int     n;                           /* échantillons valides (<= WINDOW_MAX) */
    long    fs_hz;                       /* fréquence d'échantillonnage */
    long    refractory;                  /* réfractaire de détection (échantillons) */
    /* — dérive : historique de la ligne de base (nouvelle au rang 0) — */
    long    ancrages[VEILLE_SIG_ANCRAGES];
    int     n_ancrages;                  /* ancrages remplis (<= ANCRAGES) */
    int     span;                        /* échantillons depuis le dernier ancrage */
} veille_signal_buffer;

/* Reset du tampon (fs_hz > 0 ; 0 → 250 Hz par défaut). */
void veille_signal_buffer_init(veille_signal_buffer *b, long fs_hz);

/* Un échantillon brut acquis (acquire). La fenêtre glisse quand elle est pleine. */
void veille_signal_push(veille_signal_buffer *b, int16_t sample);

/* 1 si la fenêtre est pleine (analyse possible). */
int veille_signal_window_full(const veille_signal_buffer *b);

/* État du capteur déduit du signal brut :
 *   mort (tous les zéros)       → VEILLE_SENSOR_DEAD
 *   plat (plage <= 2 codes)     → VEILLE_SENSOR_STUCK
 *   bruit excessif              → VEILLE_SENSOR_NOISY
 *   dérive lente DE LA LIGNE DE BASE, confirmée sur VEILLE_SIG_ANCRAGES spans
 *   sans inversion              → VEILLE_SENSOR_DRIFT (temps d'installation :
 *   cinq spans ; voir le #define plus haut — c'est ce qui écarte la dérive
 *   respiratoire du patient, mesuré sur MIT-BIH 100/205/222)
 *   sinon                        → VEILLE_SENSOR_OK */
veille_sensor_state veille_signal_sensor_state(const veille_signal_buffer *b);

/* Fréquence (bpm) déduite des pics R : seuil adaptatif (médiane + ½ amplitude)
 * + réfractaire. 0 si moins de deux pics (fréquence inconnue). */
long veille_signal_rate_bpm(const veille_signal_buffer *b);

/* Décision : capteur fautif → alarme technique ; sinon bpm > seuil → alarme haute. */
veille_decision veille_signal_decide(veille_sensor_state sensor, long bpm, long threshold_bpm);

/* Nom lisible d'une décision ("ok", "alarm_high", "tech_fault"). */
const char *veille_decision_name(veille_decision d);

#ifdef __cplusplus
}
#endif
#endif /* VEILLE_SIGNAL_H */
