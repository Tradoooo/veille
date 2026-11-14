#ifndef WFDB212_H
#define WFDB212_H

/* Décodeur WFDB « format 212 » — pur, sans dépendance.
 *
 * Le record mitdb/222 de PhysioNet stocke deux échantillons 12 bits signés par
 * groupe de 3 octets, little-endian (spec `annot(5)` / WFDB Applications Guide,
 * reprise à l'identique dans le jumeau Python host/veille/wfdb.py). C'est le
 * format réel du fichier : le décoder SUR LA CIBLE est ce qui permet d'y faire
 * entrer de la donnée réelle sans Python, sans disque et sans réseau.
 *
 * Une seule implémentation, partagée par tout ce qui rejoue un signal réel
 * (target/replay/). Les deux canaux sont entrelacés : groupe i = (MLII_i, V1_i).
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WFDB212_OCTETS_PAR_GROUPE 3
#define WFDB212_CANAL_MAX         2      /* 2 échantillons 12 bits par groupe */

/* Décode un groupe de 3 octets : `out[0]` = canal 0, `out[1]` = canal 1.
 * Complément à deux sur 12 bits (valeurs signées, plage -2048..2047). */
static inline void wfdb212_decode(const uint8_t *b, int16_t out[WFDB212_CANAL_MAX]) {
    int v0 = (int)b[0] | ((int)(b[1] & 0x0F) << 8);
    int v1 = (int)(b[1] >> 4) | ((int)b[2] << 4);
    if (v0 & 0x800) v0 -= 0x1000;
    if (v1 & 0x800) v1 -= 0x1000;
    out[0] = (int16_t)v0;
    out[1] = (int16_t)v1;
}

/* Nombre d'échantillons par canal dans `n_octets` (0 si groupe incomplet). */
static inline long wfdb212_echantillons(long n_octets) {
    return (n_octets / WFDB212_OCTETS_PAR_GROUPE) * WFDB212_CANAL_MAX;
}

#ifdef __cplusplus
}
#endif
#endif /* WFDB212_H */
