#Projet L3 Info - Codec d'images DIF

Binôme:
  - SAMASSEKOU Abdel Kader
  - MOKHTARI Rayane

#Compilation

Pour compiler le projet:
   cd app/
   make clean
   make

Ça génère la bibliothèque libdif.so dans le dossier CoDec/ et l'exécutable 
"encodeur" dans app/.



#Utilisation

Le programme prend en entrée soit une image (pour l'encoder), soit un fichier
.dif (pour le décoder)
Le programme détecte automatiquement ce qu'il doit faire en fonction de 
l'extension du fichier d'entrée (.dif = décodage, sinon = encodage).

Options disponibles:
   ./encodeur [options] fichier
   -h        Affiche l'aide
   -v        Mode verbeux (affiche plus d'infos pendant l'exécution)
   -t        Affiche le temps d'exécution
   -o <viewer> transforme un .dif en pnmm et affiche (viewer )

Note: Pour les formats autres que PGM/PPM (comme JPEG, PNG, etc.), le 
programme les convertir automatiquement.


## Exemple d’utilisation


### Encodage d’une image

./encodeur -v -t ../4K-earth.jpg
Conversion image → PNM : ../4K-earth.jpg → tmp_convert.pnm
Encodage PNM → DIF    : tmp_convert.pnm → ../4K-earth.jpg.dif
Taille brute          : 27 648 017 octets
Taille DIF            : 9 613 958 octets
Compression           : 34.77 %
Temps d'exécution     : 0.242 secondes

### Décodage d’une image

./encodeur -v -t ../4K-earth.jpg.dif
Décodage DIF → PNM : ../4K-earth.jpg.dif → ../4K-earth.jpg.dif.pnm
Temps d'exécution  : 0.325 secondes



#Structure du projet

.
├───app
│   │   makefile
│   │
│   ├───bin
│   └───src
│           main.c
│
└───CoDec
    │   makefile
    │
    ├───include
    │       codec.h
    │
    ├───lib
    └───src
            codec.c



#Fonctionnalités implémentées

Encodage PNM (PGM et PPM) vers format DIF
Décodage DIF vers PNM
Compression VLC avec quantificateur à 4 niveaux
Gestion d'erreurs (fichiers manquants, formats invalides, etc.)
Convertion automatique des formats (JPEG, PNG, GIF, etc.)
Affichage des statistiques de compression
Option -t -v -h



#Problèmes rencontrés et solutions

1. Gestion des commentaires dans les fichiers PNM

2. Ordre des canaux RGB (plan par plan vs entrelacé)

4. Compatibilité des fichiers .dif entre différents encodeurs