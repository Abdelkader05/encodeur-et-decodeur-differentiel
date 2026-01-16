#include "../include/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* ============================================================
 * Outils généraux
 * ============================================================ */

/* Structure pour lire un flux binaire bit par bit */
typedef struct {
    unsigned char *buf;   // données compressées 
    size_t size;          // taille du buffer 
    size_t idx;           // position octet courante 
    unsigned char acc;    // accumulateur de bits 
    int acc_bits;         // nombre de bits valides dans acc 
} BitStream;

/* Clamp simple sur [0,255] */
static unsigned char clamp_u8(int v)
{
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

/* Lecture d’un bit dans le flux compressé */
static int read_bit(BitStream *br, int *b)
{
    if (br->acc_bits == 0) {
        if (br->idx >= br->size)
            return 0; // plus de données 
        br->acc = br->buf[br->idx++];
        br->acc_bits = 8;
    }

    *b = (br->acc >> 7) & 1;
    br->acc <<= 1;
    br->acc_bits--;
    return 1;
}

/* Lecture de n bits consécutifs */
static int read_bits(BitStream *br, int n, unsigned int *v)
{
    *v = 0;
    for (int i = 0, b; i < n; i++) {
        if (!read_bit(br, &b))
            return 0;
        *v = (*v << 1) | b;
    }
    return 1;
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
 * Bitstream 
 * ============================================================ */

static int bs_init(BitStream *bs, size_t size) {
    bs->buf = malloc(size);
    if (!bs->buf) return DIF_ERR_ALLOC;
    bs->size = size;
    bs->idx = 0;
    bs->acc = 0;
    bs->acc_bits = 0;
    return DIF_OK;
}

static void bs_free(BitStream *bs) {
    free(bs->buf);
}

/*
* Ajoute nbits bits dans le buf BitStream
*/
static void bs_push_bits(BitStream *bs, unsigned int code, int nbits) {
    // On parcourt les bits du plus significatif au moins significatif 
    for (int b = nbits - 1; b >= 0; b--) {
        // Décalage de l’accumulateur et ajout du bit courant 
        bs->acc = (bs->acc << 1) | ((code >> b) & 1);
        bs->acc_bits++;

        // Quand on a accumulé 8 bits → on forme un octet
        if (bs->acc_bits == 8) {
            bs->buf[bs->idx++] = bs->acc; // écriture dans le buffer 
            bs->acc = 0;
            bs->acc_bits = 0;
        }
    }
}

/*
*termine l’écriture du flux de bits
*/
static void bs_flush(BitStream *bs) {
    //S’il reste des bits non écrits
    if (bs->acc_bits) {
        //On décale pour compléter l’octet avec des zéros
        bs->acc <<= (8 - bs->acc_bits);

        //Écriture du dernier octet
        bs->buf[bs->idx++] = bs->acc;
    }
}


/* ============================================================
 * repliement pair/impair
 * ============================================================ */

/* Repliement pair/impair : x signé -> non signé */
unsigned char centrer_replier(int delta)
{
    if (delta < 0)
        return (unsigned char)(-2 * delta - 1);
    else
        return (unsigned char)(2 * delta);
}

/* Dépliement : inverse du repliement */
int replier_centrer(unsigned char y)
{
    if (y & 1)
        return -((int)y + 1) / 2;
    else
        return (int)y / 2;
}

static int calculer_diff(const ImagePNM *img, unsigned char **premiers_out,
                           int8_t **deltas_out, size_t *deltas_len_out)
{
    int n = img->largeur * img->hauteur;
    int type = img->type;
    int len = (n > 1) ? type * (n - 1) : 0;

    unsigned char *premiers = malloc(type);
    int8_t *deltas = len ? malloc(len) : NULL;
    if (!premiers || (len && !deltas)) {
        free(premiers); free(deltas);
        return DIF_ERR_ALLOC;
    }

    int pos = 0;


    if (type == 1) {
        /* premier pixel stocké tel quel */
        unsigned char prev = img->donnees[0];
        premiers[0] = prev;

         /* calcul des différences pixel par pixel */
        for (int i = 1; i < n; i++) {
            unsigned char cur = img->donnees[i];
            int diff = cur - prev;

            /* on vérifie que la fiff tient sur int8 */
            if (diff < -127 || diff > 127) {
                free(premiers); free(deltas);
                return DIF_ERR_FORMAT;
            }

            deltas[pos++] = (int8_t)diff;
            prev = cur;
        }
    } else {
         /* tableau des valeurs précédentes pour chaque canal */
        int *prevs = malloc(sizeof(int) * type);
        if (!prevs) { free(premiers); free(deltas); return DIF_ERR_ALLOC; }

         /* initialisation :
           premiers pixels R0 G0 B0 */
        for (int c = 0; c < type; c++) {
            prevs[c] = img->donnees[c];
            premiers[c] = (unsigned char)prevs[c];
        }

        /* parcours pixel par pixel */
        for (int i = 1; i < n; i++) {
            for (int c = 0; c < type; c++) {

                unsigned char cur = img->donnees[i * type + c];
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

/*
*Transforme un tableau de deltas signés (int8_t)
* en un tableau de valeurs positives (unsigned char).
*/
static int replier(const int8_t *deltas, size_t len, unsigned char **out) {
    unsigned char *buf = len ? malloc(len) : NULL;
    if (len && !buf) return DIF_ERR_ALLOC;

    /* Repliement de chaque delta */
    for (size_t i = 0; i < len; i++)
        buf[i] = centrer_replier(deltas[i]);
    
    /* On retourne le buffer au programme appelant */
    *out = buf;
    return DIF_OK;
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
    int type;

    skip_comments(f);
    if (fscanf(f, "%2s", magic) != 1) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    if (strcmp(magic, "P5") == 0) type = 1;
    else if (strcmp(magic, "P6") == 0) type = 3;
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

    size_t n = (size_t)width * height * type;
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
    out->type = (uint8_t)type;
    out->donnees = buf;

    return DIF_OK;
}


void liberer_pnm(ImagePNM *img) {
    if (!img) return;
    free(img->donnees);
    img->donnees = NULL;
}


/* ============================================================
 * Encode PNM → DIF (conforme sujet)
 * ============================================================ */

int pnmtodif(const char *pnm, const char *dif) {
    ImagePNM img;
    if (lire_pnm(pnm, &img) != DIF_OK) return DIF_ERR_IO;

    reduire_amplitude(&img);

    unsigned char *premiers = NULL;
    int8_t *deltas = NULL;
    size_t len = 0;
    calculer_diff(&img, &premiers, &deltas, &len);

    unsigned char *folded = NULL;
    replier(deltas, len, &folded);

    BitStream bs;
    bs_init(&bs, len * 2 + 16);

    for (size_t i = 0; i < len; i++) {
        unsigned int y = folded[i];
        if (y < 2)       { bs_push_bits(&bs, 0b0,   1); bs_push_bits(&bs, y,       1); }
        else if (y < 6)  { bs_push_bits(&bs, 0b10,  2); bs_push_bits(&bs, y - 2,   2); }
        else if (y < 22) { bs_push_bits(&bs, 0b110, 3); bs_push_bits(&bs, y - 6,   4); }
        else             { bs_push_bits(&bs, 0b111, 3); bs_push_bits(&bs, y - 22,  8); }
    }
    bs_flush(&bs);

    FILE *f = fopen(dif, "wb");
    if (!f) return DIF_ERR_IO;

    uint16_t magic = (img.type == 3) ? DIF_MAGIC_COLOR : DIF_MAGIC_GRAY;
    uint8_t nb_levels = 4;
    uint8_t bits_per_level[4] = {1, 2, 4, 8};

    uint8_t header[2 + 2 + 2 + 1 + 4];
    memcpy(header + 0, &magic, 2);
    memcpy(header + 2, &img.largeur, 2);
    memcpy(header + 4, &img.hauteur, 2);
    header[6] = nb_levels;
    memcpy(header + 7, bits_per_level, nb_levels);
    if (fwrite(header, 1, 7 + nb_levels, f) != (size_t)(7 + nb_levels)) { fclose(f); return DIF_ERR_IO; }

    fwrite(premiers, 1, img.type, f);
    fwrite(bs.buf, 1, bs.idx, f);

    fclose(f);

    bs_free(&bs);
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

    //Récupération de la taille du fichier
    struct stat st;
    if (stat(chemin_dif, &st) != 0) {
        fclose(f);
        return DIF_ERR_IO;
    }
    long file_size = (long)st.st_size;
    if (file_size < 0) { fclose(f); return DIF_ERR_IO; }

    /* Lecture de l’en-tête DIF
       - magic   : 2 octets
       - width   : 2 octets
       - height  : 2 octets
       - nlevels : 1 octet
     */    
    uint16_t magic, width, height;
    uint8_t nlevels;
    uint8_t head7[7];
    if (fread(head7, 1, 7, f) != 7) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }
    memcpy(&magic, head7 + 0, 2);
    memcpy(&width, head7 + 2, 2);
    memcpy(&height, head7 + 4, 2);
    nlevels = head7[6];

    // quantificateur à 4 niveaux 
    if (nlevels != 4) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    //pbits[k] = nombre de bits associés au niveau k
    uint8_t pbits[4];
    if (fread(pbits, 1, nlevels, f) != (size_t)nlevels) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    // Détermination du nombre de couleur
    int type;
    if (magic == DIF_MAGIC_GRAY) type = 1;
    else if (magic == DIF_MAGIC_COLOR) type = 3;
    else {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    /* offsets du quantificateur */
    unsigned int offset[4];
    offset[0] = 0;
    for (int k = 1; k < 4; k++)
        offset[k] = offset[k-1] + (1U << pbits[k-1]);

    // premier pixel 
    unsigned char first[3] = {0,0,0};
    if (fread(first, 1, type, f) != (size_t)type) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    //Lecture des données compressées
    long header_len = 2 + 2 + 2 + 1 + nlevels;
    long premiers_len = type;
    long comp_len = file_size - (header_len + premiers_len);
    if (comp_len < 0) {
        fclose(f);
        return DIF_ERR_FORMAT;
    }

    unsigned char *comp = NULL;
    size_t comp_size = 0;
    if (comp_len > 0) {
        comp = malloc((size_t)comp_len);

        if (!comp) { 
            fclose(f);
            return DIF_ERR_ALLOC;
        }
        if (fread(comp, 1, (size_t)comp_len, f) != (size_t)comp_len) {
            free(comp); fclose(f);
            return DIF_ERR_FORMAT; 
        }
        comp_size = (size_t)comp_len;
    }
    fclose(f);

    // Calcul du nombre de pixels
    size_t N = (uint64_t)width * (uint64_t)height;
    if (N == 0) { 
        free(comp);
        return DIF_ERR_FORMAT;
    }

    //Allocation des plans (1 par type)
    unsigned char *planes = malloc(N * (size_t)type);
    if (!planes) {
        free(comp);
        return DIF_ERR_ALLOC;
    }

    BitStream br = { comp, comp_size, 0, 0, 0 };

    /* décodage : pour couleur (type>1) on décode par-pixel, sinon plan-par-plan */
    if (type == 1) {
        int prev = first[0];
        planes[0] = (unsigned char)prev;

        for (size_t i = 1; i < N; i++) {
            int b, k;
            unsigned int val = 0;

            if (!read_bit(&br, &b)) {
                free(comp);
                free(planes);
                return DIF_ERR_FORMAT;
            }
            if (!b) k = 0;
            else {
                if (!read_bit(&br, &b)) {
                    free(comp);
                    free(planes);
                    return DIF_ERR_FORMAT;
                }
                if (!b) k = 1;
                else {
                    if (!read_bit(&br, &b)) { free(comp); free(planes); return DIF_ERR_FORMAT; }
                    k = b ? 3 : 2;
                }
            }
            if (pbits[k] > 0) {
                if (!read_bits(&br, pbits[k], &val)) { free(comp); free(planes); return DIF_ERR_FORMAT; }
            }
            unsigned int d = offset[k] + val;
            int delta = replier_centrer((unsigned char)d);
            int cur = prev + delta;
            planes[0 * N + i] = clamp_u8(cur);
            prev = cur;
        }
    } else {
        int *prevs = malloc(sizeof(int) * type);
        if (!prevs) { free(comp); free(planes); return DIF_ERR_ALLOC; }
        for (int c = 0; c < type; c++) {
            prevs[c] = first[c];
            planes[c * N] = (unsigned char)prevs[c];
        }
        for (size_t i = 1; i < N; i++) {
            for (int c = 0; c < type; c++) {
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
                int delta = replier_centrer((unsigned char)d);
                int cur = prevs[c] + delta;
                planes[c * N + i] = clamp_u8(cur);
                prevs[c] = cur;
            }
        }
        free(prevs);
    }

    free(comp);

    /* réinterleavage RGB */
    unsigned char *img = malloc(N * (size_t)type);
    if (!img) { free(planes); return DIF_ERR_ALLOC; }

    for (size_t i = 0; i < N; i++)
        for (int c = 0; c < type; c++)
            img[i * type + c] = planes[c * N + i];

    free(planes);

    /* restaure amplitude : multiplier par 2 et clamp à 255 */
    size_t total_bytes = N * (size_t)type;
    for (size_t _i = 0; _i < total_bytes; _i++) {
        unsigned int v = ((unsigned int)img[_i]) << 1;
        if (v > 255u) v = 255u;
        img[_i] = (unsigned char)v;
    }


    /* écriture PNM */
    FILE *out = fopen(chemin_image_pnm, "wb");
    if (!out) { free(img); return DIF_ERR_IO; }

    fprintf(out, type == 1 ? "P5\n" : "P6\n");
    fprintf(out, "%u %u\n255\n", width, height);
    if (fwrite(img, 1, N * (size_t)type, out) != N * (size_t)type) { fclose(out); free(img); return DIF_ERR_IO; }
    fclose(out);

    free(img);
    return DIF_OK;
}
