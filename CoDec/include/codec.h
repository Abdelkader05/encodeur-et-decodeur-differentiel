

#ifndef CODEC_H
#define CODEC_H

#include <stddef.h>
#include <stdint.h>


/* Magic numbers pour format DIF */
#define DIF_MAGIC_GRAY  0xD1FFu
#define DIF_MAGIC_COLOR 0xD3FFu

#define DIF_OK               0

/* Erreur d'entrée/sortie :
 * - fichier introuvable
 * - erreur fopen / fread / fwrite
 */
#define DIF_ERR_IO            1

/* Erreur de format :
 * - fichier PNM invalide (magic incorrect, ...)
 * - fichier DIF invalide (magic DIF inconnu, header corrompu)
 * - données compressées incohérentes
 */
#define DIF_ERR_FORMAT        2

/* Erreur d'allocation mémoire :
 * - échec malloc / calloc / realloc
 * - mémoire insuffisante pour buffers ou images
 */
#define DIF_ERR_ALLOC         3

//pour tester
#define DIF_ERR_UNIMPLEMENTED 10

/*encodeur PNM DIF
 * - chemin_image_pnm : chemin du fichier PGM/PPM (binaire P5/P6)
 * - chemin_dif : chemin du fichier DIF à créer
 * Retourne 0 (DIF_OK) ou code d'erreur > 0
 */
int pnm_vers_dif(const char *chemin_image_pnm, const char *chemin_dif);

/*décodeur DIF  PNM
 * - chemin_dif : chemin du fichier DIF
 * - chemin_image_pnm : chemin du fichier PGM/PPM à écrire
 */
int dif_vers_pnm(const char *chemin_dif, const char *chemin_image_pnm);


/* Représentation d'une image PNM en mémoire */
typedef struct {
    uint16_t largeur;
    uint16_t hauteur;
    uint8_t type; /* 1 = PGM, 3 = PPM */
    unsigned char *donnees; /* taille = largeur * hauteur * type */
} ImagePNM;



// Fonctions utilitaires exposées (repliement pair/impair) 
unsigned char replier_delta(int delta);
int deplier_delta(unsigned char y);

// Fonctions lecture et écriture basiques
int lire_pnm(const char *chemin, ImagePNM *out);
void liberer_pnm(ImagePNM *img);
int ecrire_pnm(const char *chemin, const ImagePNM *img);


#endif