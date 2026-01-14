/*
 * codec.c
 * CoDec DIF — version strictement conforme au sujet
 */

#include "../include/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* ============================================================
 * Utils
 * ============================================================ */

typedef struct {
    unsigned char *buf;
    size_t size;
    size_t idx;
    unsigned char acc;
    int acc_bits;
} BitStream;

static unsigned char clamp_u8(int v)
{
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

static int read_bit(BitStream *br, int *b)
{
    if (br->acc_bits == 0) {
        if (br->idx >= br->size) return 0;
        br->acc = br->buf[br->idx++];
        br->acc_bits = 8;
    }
    *b = (br->acc >> 7) & 1;
    br->acc <<= 1;
    br->acc_bits--;
    return 1;
}

static int read_bits(BitStream *br, int n, unsigned int *v)
{
    *v = 0;
    for (int i = 0, b; i < n; i++) {
        if (!read_bit(br, &b)) return 0;
        *v = (*v << 1) | b;
    }
    return 1;
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

static void skip_comments(FILE *f)
{
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#')
            while ((c = fgetc(f)) != EOF && c != '\n');
        else if (!isspace(c)) {
            ungetc(c, f);
            return;
        }
    }
}



int lire_pnm(const char *chemin, ImagePNM *out) {
    FILE *f = fopen(chemin, "rb");
    if (!f) return DIF_ERR_IO;

    char magic[3];
    int width, height, maxval;
    int channels;

    skip_comments(f);
    if (fscanf(f, "%2s", magic) != 1) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    if (strcmp(magic, "P5") == 0) channels = 1;
    else if (strcmp(magic, "P6") == 0) channels = 3;
    else {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    skip_comments(f);
    if (fscanf(f, "%d", &width) != 1) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    skip_comments(f);
    if (fscanf(f, "%d", &height) != 1) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    skip_comments(f);
    if (fscanf(f, "%d", &maxval) != 1) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    if (width <= 0 || height <= 0 ||
        width > 65535 || height > 65535 ||
        maxval != 255) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    /* sauter le retour ligne après maxval */
    fgetc(f);

    size_t n = (size_t)width * height * channels;
    unsigned char *buf = malloc(n);
    if (!buf) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    if (fread(buf, 1, n, f) != n) {
        free(buf);
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    fclose(f);

    out->largeur = (uint16_t)width;
    out->hauteur = (uint16_t)height;
    out->type = (uint8_t)channels;
    out->donnees = buf;

    return DIF_OK;
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

    /* For grayscale: simple single-channel scan (existing behavior).
     * For color (channels > 1) we produce deltas interleaved per pixel
     * (R0,G0,B0, R1,G1,B1, ...) so the encoder writes codes in the
     * same order as the decoder expects (per-pixel order).
     */
    if (channels == 1) {
        unsigned char prev = img->donnees[0];
        premiers[0] = prev;
        for (int i = 1; i < n; i++) {
            unsigned char cur = img->donnees[i];
            int diff = cur - prev;
            if (diff < -127 || diff > 127) {
                free(premiers); free(deltas);
                return DIF_ERR_FORMAT;
            }
            deltas[pos++] = (int8_t)diff;
            prev = cur;
        }
    } else {
        /* initialize per-channel previous values */
        int *prevs = malloc(sizeof(int) * channels);
        if (!prevs) { free(premiers); free(deltas); return DIF_ERR_ALLOC; }
        for (int c = 0; c < channels; c++) {
            prevs[c] = img->donnees[c];
            premiers[c] = (unsigned char)prevs[c];
        }
        for (int i = 1; i < n; i++) {
            for (int c = 0; c < channels; c++) {
                unsigned char cur = img->donnees[i * channels + c];
                int diff = (int)cur - prevs[c];
                if (diff < -127 || diff > 127) {
                    free(prevs); free(premiers); free(deltas);
                    return DIF_ERR_FORMAT;
                }
                deltas[pos++] = (int8_t)diff;
                prevs[c] = cur;
            }
        }
        free(prevs);
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

static int bw_init(BitStream *bw, size_t size) {
    bw->buf = malloc(size);
    if (!bw->buf) return DIF_ERR_ALLOC;
    bw->size = size;
    bw->idx = 0;
    bw->acc = 0;
    bw->acc_bits = 0;
    return DIF_OK;
}

static void bw_free(BitStream *bw) {
    free(bw->buf);
}

static void bw_push_bits(BitStream *bw, unsigned int code, int nbits) {
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

static void bw_flush(BitStream *bw) {
    if (bw->acc_bits) {
        bw->acc <<= (8 - bw->acc_bits);
        bw->buf[bw->idx++] = bw->acc;
    }
}

/* ============================================================
 * Encode PNM → DIF (conforme sujet)
 * ============================================================ */

int pnmtodif(const char *pnm, const char *dif) {
    ImagePNM img;
    if (lire_pnm(pnm, &img) != DIF_OK) return DIF_ERR_IO;

    reduire_amplitude(&img);

    unsigned char *premiers;
    int8_t *deltas;
    size_t len;
    calculer_deltas(&img, &premiers, &deltas, &len);

    unsigned char *folded;
    replier_deltas(deltas, len, &folded);

    BitStream bw;
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

    /* write header in one binary block */
    uint8_t header[2 + 2 + 2 + 1 + 4];
    memcpy(header + 0, &magic, 2);
    memcpy(header + 2, &img.largeur, 2);
    memcpy(header + 4, &img.hauteur, 2);
    header[6] = nb_levels;
    memcpy(header + 7, bits_per_level, nb_levels);
    if (fwrite(header, 1, 7 + nb_levels, f) != (size_t)(7 + nb_levels)) { fclose(f); return DIF_ERR_IO; }

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


/* ============================================================
 * Decode DIF → PNM
 * ============================================================ */
int diftopnm(const char* chemin_dif, const char* chemin_image_pnm)
{
    FILE *f = fopen(chemin_dif, "rb");
    if (!f) return DIF_ERR_IO;

    /* get file size without seeking */
    struct stat st;
    if (stat(chemin_dif, &st) != 0) { fclose(f); return DIF_ERR_IO; }
    long file_size = (long)st.st_size;
    if (file_size < 0) { fclose(f); return DIF_ERR_IO; }

    /* read header: magic(2) width(2) height(2) nlevels(1) pbits[nlevels] */
    uint16_t magic, width, height;
    uint8_t nlevels;
    uint8_t head7[7];
    if (fread(head7, 1, 7, f) != 7) { fclose(f); return DIF_ERR_FORMAT; }
    memcpy(&magic, head7 + 0, 2);
    memcpy(&width, head7 + 2, 2);
    memcpy(&height, head7 + 4, 2);
    nlevels = head7[6];

    if (nlevels != 4) { fclose(f); return DIF_ERR_FORMAT; }

    uint8_t pbits[4];
    if (fread(pbits, 1, nlevels, f) != (size_t)nlevels) { fclose(f); return DIF_ERR_FORMAT; }

    int channels;
    if (magic == DIF_MAGIC_GRAY) channels = 1;
    else if (magic == DIF_MAGIC_COLOR) channels = 3;
    else { fclose(f); return DIF_ERR_FORMAT; }

    /* offsets du quantificateur */
    unsigned int offset[4];
    offset[0] = 0;
    for (int k = 1; k < 4; k++) offset[k] = offset[k-1] + (1U << pbits[k-1]);

    /* premier pixel */
    unsigned char first[3] = {0,0,0};
    if (fread(first, 1, channels, f) != (size_t)channels) { fclose(f); return DIF_ERR_FORMAT; }

    /* compressed data length = file_size - header_len - premiers_len */
    long header_len = 2 + 2 + 2 + 1 + nlevels;
    long premiers_len = channels;
    long comp_len = file_size - (header_len + premiers_len);
    if (comp_len < 0) { fclose(f); return DIF_ERR_FORMAT; }

    unsigned char *comp = NULL;
    size_t comp_size = 0;
    if (comp_len > 0) {
        comp = malloc((size_t)comp_len);
        if (!comp) { fclose(f); return DIF_ERR_ALLOC; }
        if (fread(comp, 1, (size_t)comp_len, f) != (size_t)comp_len) { free(comp); fclose(f); return DIF_ERR_FORMAT; }
        comp_size = (size_t)comp_len;
    }

    fclose(f);

    /* number of pixels */
    uint64_t N64 = (uint64_t)width * (uint64_t)height;
    if (N64 == 0 || N64 > SIZE_MAX / (size_t)channels) { free(comp); return DIF_ERR_FORMAT; }
    size_t N = (size_t)N64;

    unsigned char *planes = malloc(N * (size_t)channels);
    if (!planes) { free(comp); return DIF_ERR_ALLOC; }

    BitStream br = { comp, comp_size, 0, 0, 0 };

    /* décodage : pour couleur (channels>1) on décode par-pixel, sinon plan-par-plan */
    if (channels == 1) {
        int prev = first[0];
        planes[0] = (unsigned char)prev;
        for (size_t i = 1; i < N; i++) {
            int b, k;
            unsigned int val = 0;
            if (!read_bit(&br, &b)) { free(comp); free(planes); return DIF_ERR_FORMAT; }
            if (!b) k = 0;
            else {
                if (!read_bit(&br, &b)) { free(comp); free(planes); return DIF_ERR_FORMAT; }
                if (!b) k = 1;
                else {
                    if (!read_bit(&br, &b)) { free(comp); free(planes); return DIF_ERR_FORMAT; }
                    k = b ? 3 : 2;
                }
            }
            if (pbits[k] > 0) {
                if (!read_bits((BitStream*)&br, pbits[k], &val)) { free(comp); free(planes); return DIF_ERR_FORMAT; }
            }
            unsigned int d = offset[k] + val;
            int delta = deplier_delta((unsigned char)d);
            int cur = prev + delta;
            planes[0 * N + i] = clamp_u8(cur);
            prev = cur;
        }
    } else {
        int *prevs = malloc(sizeof(int) * channels);
        if (!prevs) { free(comp); free(planes); return DIF_ERR_ALLOC; }
        for (int c = 0; c < channels; c++) {
            prevs[c] = first[c];
            planes[c * N] = (unsigned char)prevs[c];
        }
        for (size_t i = 1; i < N; i++) {
            for (int c = 0; c < channels; c++) {
                int b, k;
                unsigned int val = 0;
                if (!read_bit(&br, &b)) { free(prevs); free(comp); free(planes); return DIF_ERR_FORMAT; }
                if (!b) k = 0;
                else {
                    if (!read_bit(&br, &b)) { free(prevs); free(comp); free(planes); return DIF_ERR_FORMAT; }
                    if (!b) k = 1;
                    else {
                        if (!read_bit(&br, &b)) { free(prevs); free(comp); free(planes); return DIF_ERR_FORMAT; }
                        k = b ? 3 : 2;
                    }
                }
                if (pbits[k] > 0) {
                    if (!read_bits((BitStream*)&br, pbits[k], &val)) { free(prevs); free(comp); free(planes); return DIF_ERR_FORMAT; }
                }
                unsigned int d = offset[k] + val;
                int delta = deplier_delta((unsigned char)d);
                int cur = prevs[c] + delta;
                planes[c * N + i] = clamp_u8(cur);
                prevs[c] = cur;
            }
        }
        free(prevs);
    }

    free(comp);

    /* réinterleavage RGB */
    unsigned char *img = malloc(N * (size_t)channels);
    if (!img) { free(planes); return DIF_ERR_ALLOC; }

    for (size_t i = 0; i < N; i++)
        for (int c = 0; c < channels; c++)
            img[i * channels + c] = planes[c * N + i];

    free(planes);

    /* restaure amplitude : multiplier par 2 et clamp à 255 */
    size_t total_bytes = N * (size_t)channels;
    for (size_t _i = 0; _i < total_bytes; _i++) {
        unsigned int v = ((unsigned int)img[_i]) << 1;
        if (v > 255u) v = 255u;
        img[_i] = (unsigned char)v;
    }


    /* écriture PNM */
    FILE *out = fopen(chemin_image_pnm, "wb");
    if (!out) { free(img); return DIF_ERR_IO; }

    fprintf(out, channels == 1 ? "P5\n" : "P6\n");
    fprintf(out, "%u %u\n255\n", width, height);
    if (fwrite(img, 1, N * (size_t)channels, out) != N * (size_t)channels) { fclose(out); free(img); return DIF_ERR_IO; }
    fclose(out);

    free(img);
    return DIF_OK;
}
