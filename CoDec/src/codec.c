/*
 * codec.c
 * Squelette complet du CoDec DIF : stubs documentés + utilitaires de base
 * - Implémentations minimales et commentaires pour chaque fonction principale
 * - TODO: remplir chaque TODO pour obtenir un encodeur/décodeur complet
 */

#include "../include/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Structures internes ---------- */
typedef struct { 
    unsigned char* ptr; /* pointeur sur l’octet courant */
    size_t cap; /* capacité en lecture/écriture */
} BitStream;



unsigned char centrer_replier(unsigned char x) {
    if (x < 0) return (unsigned char)(-2 * x - 1);
    return (unsigned char) (2 * x);
}

int replier_centre(unsigned char y) {
    if (y & 1) {
        return - ((int) y + 1) / 2;
    } else {
        return ((int) y) / 2;
    }
}


int lire_pnm(const char *chemin, ImagePNM *out) {
    return DIF_ERR_UNIMPLEMENTED;
}

void liberer_pnm(ImagePNM *img) {
    if (!img) return;
    free(img->donnees);
    img->donnees = NULL;
}

int ecrire_pnm(const char *chemin, const ImagePNM *img) {
    return DIF_ERR_UNIMPLEMENTED;
}


int pnm_vers_dif(const char* chemin_image_pnm, const char* chemin_dif) {
    /* 1) Lire PNM
     * 2) Réduire amplitude (>>1)
     * 3) Calcul des deltas plan-par-plan
     * 4) Repliement pair/impair
     * 5) Encodage VLC (écrire bits)
     * 6) Écrire header + premier pixel(s) + buffer compressé
     */
    fprintf(stderr, "pnm_vers_dif: non implémentée (stub)\n");
    return DIF_ERR_UNIMPLEMENTED;
}

int dif_vers_pnm(const char* chemin_dif, const char* chemin_image_pnm) {
    /* 1) Lire header + premiers pixels
     * 2) Lire buffer compressé
     * 3) Décode bit-à-bit (préfixe puis p_k bits)
     * 4) Dépliement des deltas
     * 5) Reconstituer pixels réduits, multiplier par 2, écrire PNM
     */
    fprintf(stderr, "dif_vers_pnm: non implémentée (stub)\n");
    return DIF_ERR_UNIMPLEMENTED;
}

