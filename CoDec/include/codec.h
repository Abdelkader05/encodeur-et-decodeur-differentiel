#ifndef _MTRACKC_H_
#define _MTRACKC_H_

/*
traite en entrée un fichier image au format PNM : PGM (niveaux de gris, 1 octet/pixel) ou
PPM (couleurs RGB, 3 octets/pixel)
• produit en sortie un fichier au format DIF (expliqué et détaillé dans la suite).
• return : 0 en cas de succes, un entier >0 en cas d’échec.*/
int pnmtodif(const char* pnminput, const char* difoutput);


/*
traite en entrée un fiher au format DIF.
• produit en sortie un fichier au format PNM (PGM ou PPM).
• return : 0 en cas de succes, un entier >0 en cas d’échec.*/
int diftopnm(const char* difinput, const char* pnmoutput);

#endif