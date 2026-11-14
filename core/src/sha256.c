/* SHA-256 pur — implémentation de référence de Veille (FIPS 180-4).
 *
 * Voir core/include/veille/sha256.h pour la raison d'être (l'empreinte de
 * configuration fait partie du CONTRAT et doit être calculable là où le verdict
 * est produit, y compris dans un invité QNX sans paquet optionnel).
 *
 * Style : aucune allocation, aucun état global (réentrants), pas d'#ifdef par
 * plateforme. Les fonctions sont courtes et une seule logique chacune :
 * compression d'un bloc, absorption d'un octet, finalisation, rendu hex.
 */
#include "veille/sha256.h"

/* Constantes FIPS 180-4 §4.2.2 (carrés des 64 premiers nombres premiers). */
static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}
static void put_be32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);  b[3] = (uint8_t)v;
}

/* Fonctions logiques §4.1.2. */
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z)   { return (x & y) ^ (~x & z); }
static uint32_t maj(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t bigsig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static uint32_t bigsig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static uint32_t smallsig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static uint32_t smallsig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

/* Pré-extension du message : W[0..15] depuis le bloc, W[16..63] par la récurrence. */
static void expand(const uint8_t *bloc, uint32_t *w) {
    for (int i = 0; i < 16; i++) w[i] = be32(bloc + i * 4);
    for (int i = 16; i < 64; i++)
        w[i] = w[i - 16] + smallsig1(w[i - 2]) + w[i - 7] + smallsig0(w[i - 15]);
}

/* Compression d'un bloc de 64 octets sur l'état chaîné h (§6.2.2). */
static void compress(uint32_t *h, const uint8_t *bloc) {
    uint32_t w[64];
    expand(bloc, w);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = hh + bigsig1(e) + ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = bigsig0(a) + maj(a, b, c);
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void veille_sha256_init(veille_sha256_ctx *c) {
    if (!c) return;
    c->h[0] = 0x6a09e667u; c->h[1] = 0xbb67ae85u; c->h[2] = 0x3c6ef372u;
    c->h[3] = 0xa54ff53au; c->h[4] = 0x510e527fu; c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu; c->h[7] = 0x5be0cd19u;
    c->rempli = 0;
    c->n_octets = 0;
    for (int i = 0; i < VEILLE_SHA256_BLOC; i++) c->bloc[i] = 0;
}

/* Absorbe un octet : remplit le bloc courant, le compresse quand il est plein. */
static void absorb(veille_sha256_ctx *c, uint8_t octet) {
    c->bloc[c->rempli++] = octet;
    c->n_octets++;
    if (c->rempli == VEILLE_SHA256_BLOC) {
        compress(c->h, c->bloc);
        c->rempli = 0;
    }
}

void veille_sha256_update(veille_sha256_ctx *c, const void *donnees, size_t n) {
    if (!c || !donnees) return;
    const uint8_t *p = (const uint8_t *)donnees;
    for (size_t i = 0; i < n; i++) absorb(c, p[i]);
}

/* Remplissage §5.1 : 0x80, des zéros, puis la longueur en bits sur 64 bits BE. */
static void padding(veille_sha256_ctx *c) {
    uint64_t bits = c->n_octets * 8u;
    absorb(c, 0x80);
    while (c->rempli != 56) absorb(c, 0x00);
    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - i * 8));
    for (int i = 0; i < 8; i++) absorb(c, len[i]);
}

void veille_sha256_final(veille_sha256_ctx *c, uint8_t *out) {
    if (!c || !out) return;
    padding(c);
    for (int i = 0; i < 8; i++) put_be32(out + i * 4, c->h[i]);
    veille_sha256_init(c);
}

void veille_sha256_hex(const void *donnees, size_t n, char *out) {
    if (!out) return;
    out[0] = '\0';
    if (!donnees) return;
    veille_sha256_ctx c;
    veille_sha256_init(&c);
    veille_sha256_update(&c, donnees, n);
    uint8_t d[VEILLE_SHA256_SIZE];
    veille_sha256_final(&c, d);
    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < VEILLE_SHA256_SIZE; i++) {
        out[i * 2]     = HEX[d[i] >> 4];
        out[i * 2 + 1] = HEX[d[i] & 0x0F];
    }
    out[VEILLE_SHA256_SIZE * 2] = '\0';
}
