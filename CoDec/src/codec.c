/*
 * codec.c
 * CoDec DIF — version strictement conforme au sujet
 */

#include "../include/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
 * Utils
 * ============================================================ */

static int readint(int *x, const char *str) {
    char *endptr = NULL;
    long y = strtol(str, &endptr, 10);
    if (*endptr != '\0') return 0;
    *x = (int)y;
    return ((long)*x == y);
}

/* ============================================================
 * Delta folding
 * ============================================================ */

unsigned char replier_delta(int delta) {
    return (delta < 0) ? (unsigned char)(-2 * delta - 1)
                       : (unsigned char)(2 * delta);
}

int deplier_delta(unsigned char y) {
    return (y & 1) ? -((int)y + 1) / 2 : (int)y / 2;
}

/* ============================================================
 * PNM parsing
 * ============================================================ */

static int read_token(FILE *f, char *buf, size_t bufsize) {
    int c;
    size_t i = 0;

    while ((c = fgetc(f)) != EOF) {
        if (isspace(c)) continue;
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n');
            continue;
        }
        break;
    }
    if (c == EOF) return -1;

    do {
        if (i + 1 < bufsize) buf[i++] = (char)c;
        c = fgetc(f);
    } while (c != EOF && !isspace(c));

    buf[i] = '\0';
    return 0;
}

int lire_pnm(const char *chemin, ImagePNM *out) {
    FILE *f = fopen(chemin, "rb");
    if (!f) return DIF_ERR_IO;

    char token[64];
    int width, height, maxval, channels;

    if (read_token(f, token, sizeof token) < 0) goto err;
    if (!strcmp(token, "P5")) channels = 1;
    else if (!strcmp(token, "P6")) channels = 3;
    else goto err;

    if (read_token(f, token, sizeof token) < 0 || !readint(&width, token)) goto err;
    if (read_token(f, token, sizeof token) < 0 || !readint(&height, token)) goto err;
    if (read_token(f, token, sizeof token) < 0 || !readint(&maxval, token)) goto err;

    if (width <= 0 || height <= 0 || width > 65535 || height > 65535 || maxval != 255)
        goto err;

    size_t n = (size_t)width * height * channels;
    unsigned char *buf = malloc(n);
    if (!buf) goto err;

    fread(buf, 1, n, f);
    fclose(f);

    out->largeur = (uint16_t)width;
    out->hauteur = (uint16_t)height;
    out->type = (uint8_t)channels;
    out->donnees = buf;
    return DIF_OK;

err:
    fclose(f);
    return DIF_ERR_FORMAT;
}

void liberer_pnm(ImagePNM *img) {
    if (!img) return;
    free(img->donnees);
    img->donnees = NULL;
}

/* ============================================================
 * Amplitude reduction
 * ============================================================ */

static void reduire_amplitude(ImagePNM *img) {
    size_t n = (size_t)img->largeur * img->hauteur * img->type;
    for (size_t i = 0; i < n; i++)
        img->donnees[i] >>= 1;
}

/* ============================================================
 * Delta computation
 * ============================================================ */

static int calculer_deltas(const ImagePNM *img,
                           unsigned char **premiers_out,
                           int8_t **deltas_out,
                           size_t *deltas_len_out)
{
    int n = img->largeur * img->hauteur;
    int channels = img->type;
    int len = (n > 1) ? channels * (n - 1) : 0;

    unsigned char *premiers = malloc(channels);
    int8_t *deltas = len ? malloc(len) : NULL;
    if (!premiers || (len && !deltas)) {
        free(premiers); free(deltas);
        return DIF_ERR_ALLOC;
    }

    int pos = 0;
    for (int c = 0; c < channels; c++) {
        unsigned char prev = img->donnees[c];
        premiers[c] = prev;
        for (int i = 1; i < n; i++) {
            unsigned char cur = img->donnees[i * channels + c];
            int diff = cur - prev;
            if (diff < -127 || diff > 127) {
                free(premiers); free(deltas);
                return DIF_ERR_FORMAT;
            }
            deltas[pos++] = (int8_t)diff;
            prev = cur;
        }
    }

    *premiers_out = premiers;
    *deltas_out = deltas;
    *deltas_len_out = len;
    return DIF_OK;
}

static int replier_deltas(const int8_t *deltas, size_t len, unsigned char **out) {
    unsigned char *buf = len ? malloc(len) : NULL;
    if (len && !buf) return DIF_ERR_ALLOC;
    for (size_t i = 0; i < len; i++)
        buf[i] = replier_delta(deltas[i]);
    *out = buf;
    return DIF_OK;
}

/* ============================================================
 * Bitstream writer
 * ============================================================ */

typedef struct {
    unsigned char *buf;
    size_t cap;
    size_t idx;
    unsigned char acc;
    int acc_bits;
} BitWriter;

static int bw_init(BitWriter *bw, size_t cap) {
    bw->buf = malloc(cap);
    if (!bw->buf) return DIF_ERR_ALLOC;
    bw->cap = cap;
    bw->idx = 0;
    bw->acc = 0;
    bw->acc_bits = 0;
    return DIF_OK;
}

static void bw_free(BitWriter *bw) {
    free(bw->buf);
}

static void bw_push_bits(BitWriter *bw, unsigned int code, int nbits) {
    for (int b = nbits - 1; b >= 0; b--) {
        bw->acc = (bw->acc << 1) | ((code >> b) & 1);
        bw->acc_bits++;
        if (bw->acc_bits == 8) {
            bw->buf[bw->idx++] = bw->acc;
            bw->acc = 0;
            bw->acc_bits = 0;
        }
    }
}

static void bw_flush(BitWriter *bw) {
    if (bw->acc_bits) {
        bw->acc <<= (8 - bw->acc_bits);
        bw->buf[bw->idx++] = bw->acc;
    }
}

/* ============================================================
 * Encode PNM → DIF (conforme sujet)
 * ============================================================ */

int pnm_vers_dif(const char *pnm, const char *dif) {
    ImagePNM img;
    if (lire_pnm(pnm, &img) != DIF_OK) return DIF_ERR_IO;

    reduire_amplitude(&img);

    unsigned char *premiers;
    int8_t *deltas;
    size_t len;
    calculer_deltas(&img, &premiers, &deltas, &len);

    unsigned char *folded;
    replier_deltas(deltas, len, &folded);

    BitWriter bw;
    bw_init(&bw, len * 2 + 16);

    for (size_t i = 0; i < len; i++) {
        unsigned int y = folded[i];
        if (y < 2)       { bw_push_bits(&bw, 0b0,   1); bw_push_bits(&bw, y,       1); }
        else if (y < 6)  { bw_push_bits(&bw, 0b10,  2); bw_push_bits(&bw, y - 2,   2); }
        else if (y < 22) { bw_push_bits(&bw, 0b110, 3); bw_push_bits(&bw, y - 6,   4); }
        else             { bw_push_bits(&bw, 0b111, 3); bw_push_bits(&bw, y - 22,  8); }
    }
    bw_flush(&bw);

    FILE *f = fopen(dif, "wb");
    if (!f) return DIF_ERR_IO;

    uint16_t magic = (img.type == 3) ? DIF_MAGIC_COLOR : DIF_MAGIC_GRAY;
    uint8_t nb_levels = 4;
    uint8_t bits_per_level[4] = {1, 2, 4, 8};

    fwrite(&magic, 2, 1, f);
    fwrite(&img.largeur, 2, 1, f);
    fwrite(&img.hauteur, 2, 1, f);
    fwrite(&nb_levels, 1, 1, f);
    fwrite(bits_per_level, 1, 4, f);

    fwrite(premiers, 1, img.type, f);
    fwrite(bw.buf, 1, bw.idx, f);

    fclose(f);

    bw_free(&bw);
    free(folded);
    free(deltas);
    free(premiers);
    liberer_pnm(&img);

    return DIF_OK;
}

int dif_vers_pnm(const char *dif, const char *pnm) {
    (void)dif; (void)pnm;
    return DIF_ERR_UNIMPLEMENTED;
}
