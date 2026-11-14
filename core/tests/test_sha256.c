/* Test C : SHA-256 de core/ (voir core/include/veille/sha256.h).
 *
 * Vecteurs de référence : les deux exemples publiés de FIPS 180-4 (§ "Examples",
 * 3 octets et 56 octets) et des empreintes calculées par le jumeau Python
 * (hashlib) pour les cas qui frappent la FINALISATION — bloc exact (64 octets),
 * frontières de remplissage (55/56/57 octets : c'est là qu'un 0x80 mal placé
 * ajoute un bloc de trop), multi-blocs (1000 octets).
 *
 * Pourquoi ces cas et pas seulement "abc" : une table K recopiée de travers ne
 * se voit qu'au premier bit ; ici l'erreur trouvée au développement était
 * K[6] = 0x92397582 au lieu de 0x923f82a4 — "abc" la détecte, mais les cas de
 * remplissage prouvent en plus que le compteur de longueur est juste.
 *
 * Le jumeau Python est comparé octet par octet dans tests/differential.py.
 */
#include <stdio.h>
#include <string.h>

#include "veille/sha256.h"

static int echecs = 0;
static int n_tests = 0;

static void attends_hex(const char *nom, const void *d, size_t n, const char *attendu) {
    char o[VEILLE_SHA256_HEXLEN];
    veille_sha256_hex(d, n, o);
    n_tests++;
    if (strcmp(o, attendu) != 0) {
        printf("[ÉCHEC] %s\n  obtenu   : %s\n  attendu  : %s\n", nom, o, attendu);
        echecs++;
    } else {
        printf("[OK] %s\n", nom);
    }
}

/* Hachage OCTET PAR OCTET (flux) : doit rendre la même empreinte qu'un seul
 * appel — c'est ce que fait le rejeu d'ECG réel, qui avale le signal par
 * morceaux. */
static void attends_flux(const char *nom, const uint8_t *d, size_t n,
                         const char *attendu) {
    veille_sha256_ctx c;
    veille_sha256_init(&c);
    for (size_t i = 0; i < n; i++) veille_sha256_update(&c, d + i, 1);
    uint8_t dig[VEILLE_SHA256_SIZE];
    veille_sha256_final(&c, dig);
    static const char HEX[] = "0123456789abcdef";
    char o[VEILLE_SHA256_HEXLEN];
    for (int i = 0; i < VEILLE_SHA256_SIZE; i++) {
        o[i * 2]     = HEX[dig[i] >> 4];
        o[i * 2 + 1] = HEX[dig[i] & 0x0F];
    }
    o[64] = '\0';
    n_tests++;
    if (strcmp(o, attendu) != 0) {
        printf("[ÉCHEC] %s (flux)\n  obtenu  : %s\n  attendu : %s\n", nom, o, attendu);
        echecs++;
    } else {
        printf("[OK] %s (flux octet par octet)\n", nom);
    }
}

int main(void) {
    /* FIPS 180-4, exemples publiés. */
    attends_hex("abc (FIPS exemple 1)", "abc", 3,
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    attends_hex("56 octets (FIPS exemple 2)",
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    attends_hex("tampon vide", "", 0,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    static uint8_t a64[64], f55[55], f56[56], f57[57], z1000[1000];
    memset(a64, 'a', sizeof(a64));
    memset(f55, 0x37, sizeof(f55));
    memset(f56, 0x37, sizeof(f56));
    memset(f57, 0x37, sizeof(f57));
    memset(z1000, 'z', sizeof(z1000));

    attends_hex("64 octets (bloc exact)", a64, sizeof(a64),
                "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    attends_flux("64 octets (bloc exact)", a64, sizeof(a64),
                 "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    attends_hex("55 octets (avant la frontière)", f55, sizeof(f55),
                "f9e3ac6e56363ff68f01e0642921f38b53a23ee735f35bbfc6fed6c25d10f9ec");
    attends_hex("56 octets (0x80 force un bloc)", f56, sizeof(f56),
                "43a099f1cba0f94f662d28903554a2151cd175d199833891f9c3ea83145bb2eb");
    attends_hex("57 octets (après la frontière)", f57, sizeof(f57),
                "ea7e133eb1f0f8f929884e7fb4f592a779aaac1a8f9a5b828e49eb51bf0af468");
    attends_hex("1000 octets (multi-blocs)", z1000, sizeof(z1000),
                "950f88b09cf1d5e2cdbc5660c77dce3962265c548797950095629a0ea2daea46");
    attends_flux("1000 octets (multi-blocs)", z1000, sizeof(z1000),
                 "950f88b09cf1d5e2cdbc5660c77dce3962265c548797950095629a0ea2daea46");

    printf("\n%d test(s), %d échec(s)\n", n_tests, echecs);
    return echecs ? 1 : 0;
}
