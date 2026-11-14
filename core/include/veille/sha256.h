#ifndef VEILLE_SHA256_H
#define VEILLE_SHA256_H

/* SHA-256 pur C11 — aucune dépendance (ni openssl, ni zlib, ni libc au-delà
 * de string.h). Pourquoi : l'empreinte SHA-256 d'une configuration fait partie
 * du CONTRAT (§5.4 : `veille_report_meta.config_sha256`, 64 hex) et doit donc
 * être calculable LÀ OÙ LE VERDICT EST PRODUIT — y compris dans l'invité QNX de
 * démonstration, qui n'a ni disque, ni réseau, ni paquet optionnel, et sur un
 * poste de travail où personne n'a installé libssl-dev.
 *
 * `core/` est pur par règle (ni horloge, ni fichier, ni socket, ni allocation) :
 * ce module respecte la même contrainte — contexte sur la pile de l'appelant,
 * aucun malloc.
 *
 * Le jumeau Python (host/veille/report.py) utilise `hashlib.sha256` : c'est la
 * bibliothèque standard, donc PAS une dépendance installée. Le test
 * `tests/differential.py` compare les deux implémentations octet par octet —
 * un désaccord est un bug, jamais une approximation tolérée.
 *
 * Vecteurs de référence (NIST FIPS 180-4, exemple §"SHA-256 Example 1 and 2") :
 *   sha256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 *   sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
 *         = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VEILLE_SHA256_SIZE   32   /* octets du digest */
#define VEILLE_SHA256_HEXLEN 65   /* 64 hex + NUL */
#define VEILLE_SHA256_BLOC   64   /* taille d'un bloc, en octets */

/* Contexte de hachage. Fourni par l'appelant (pile acceptable, ~100 octets). */
typedef struct {
    uint32_t h[8];                      /* état chaîné (H0..H7) */
    uint8_t  bloc[VEILLE_SHA256_BLOC];  /* bloc en cours de remplissage */
    size_t   rempli;                    /* octets présents dans `bloc` */
    uint64_t n_octets;                  /* total de données absorbées */
} veille_sha256_ctx;

/* Initialise le contexte (vecteurs d'initialisation FIPS 180-4). */
void veille_sha256_init(veille_sha256_ctx *c);

/* Absorbe `n` octets ; appelable autant de fois que voulu (flux). */
void veille_sha256_update(veille_sha256_ctx *c, const void *donnees, size_t n);

/* Finalise : écrit 32 octets dans `out` (32 octets minimum) et réinitialise. */
void veille_sha256_final(veille_sha256_ctx *c, uint8_t *out);

/* Convenience : digest d'un tampon contigu, rendu en 64 hexadécimaux minuscules
 * + NUL dans `out` (VEILLE_SHA256_HEXLEN octets minimum). `out` est vidé en
 * cas d'argument invalide. */
void veille_sha256_hex(const void *donnees, size_t n, char *out);

#ifdef __cplusplus
}
#endif
#endif /* VEILLE_SHA256_H */
