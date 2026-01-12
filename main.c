

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codec.h"

int main(int argc, char *argv[]) {
    if ((argc >= 1 && strcmp(argv[1], "-h") == 0) || argc < 4) {
        fprintf(stderr, " ./diftool -c in.pnm out.dif   (compress)\n./diftool -d in.dif out.pnm   (decompress)\n");
        return 1;
    }
    int compress = 0;
    if (strcmp(argv[1], "-c") == 0) compress = 1;
    else if (strcmp(argv[1], "-d") == 0) compress = 0;
    else {
        fprintf(stderr, "Option invalide\n"); return 1;
    }

    const char *in = argv[2];
    const char *out = argv[3];

    int rc;
    if (compress) {
        printf("Compression: %s -> %s\n", in, out);
        rc = pnm_vers_dif(in, out);
    } else {
        printf("Décompression: %s -> %s\n", in, out);
        rc = dif_vers_pnm(in, out);
    }
    if (rc != 0) {
        fprintf(stderr, "Opération échouée (code %d)\n", rc);
        return rc;
    }
    printf("Terminé.\n");
    return 0;
}
