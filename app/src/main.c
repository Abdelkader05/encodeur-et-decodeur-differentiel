#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "codec.h"

static void help(const char *prog)
{
    printf("Usage: %s [options] fichier\n", prog);
    printf("Options:\n");
    printf("  -h           afficher cette aide\n");
    printf("  -v           mode verbeux\n");
    printf("  -o <viewer>  ouvrir l'image PNM après décodage\n");
}

static long file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

static int has_ext(const char *name, const char *ext)
{
    size_t ln = strlen(name), le = strlen(ext);
    if (ln < le) return 0;
    return strcmp(name + ln - le, ext) == 0;
}

static int is_pnm(const char *name)
{
    return has_ext(name, ".pgm") ||
           has_ext(name, ".ppm") ||
           has_ext(name, ".pnm");
}

int main(int argc, char *argv[])
{
    int verbose = 0;
    const char *viewer = NULL;
    const char *input = NULL;


    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            help(argv[0]);
            return 0;
        }
        else if (!strcmp(argv[i], "-v")) {
            verbose = 1;
        }
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            viewer = argv[++i];
        }
        else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    if (!input) {
        fprintf(stderr, "Erreur: aucun fichier fourni\n");
        help(argv[0]);
        return 1;
    }

    /* =========================================================
     * .dif → décodage
     * ========================================================= */
    if (has_ext(input, ".dif")) {
        char out[256];
        snprintf(out, sizeof out, "%s.pnm", input);

        if (verbose)
            printf("Décodage DIF → PNM : %s → %s\n", input, out);

        if (diftopnm(input, out) != DIF_OK) {
            fprintf(stderr, "Erreur décodage DIF\n");
            return 1;
        }

        if (viewer) {
            char cmd[512];
            snprintf(cmd, sizeof cmd, "%s %s &", viewer, out);
            system(cmd);
            return 0;
        }
    }

    /* =========================================================
     * image → DIF
     * ========================================================= */
    else {
        char pnm_file[256];
        const char *pnm_input = input;
        int tmp_pnm = 0;

        /* --- si ce n'est pas déjà du PNM, on convertit --- */
        if (!is_pnm(input)) {
            snprintf(pnm_file, sizeof pnm_file, "tmp_convert.pnm");

            char cmd[512];
            snprintf(cmd, sizeof cmd,
                     "convert \"%s\" \"%s\" 2>/dev/null",
                     input, pnm_file);

            if (verbose)
                printf("Conversion image → PNM : %s → %s\n",
                       input, pnm_file);

            if (system(cmd) != 0 || access(pnm_file, F_OK) != 0) {
                fprintf(stderr, "Erreur : conversion impossible (%s)\n", input);
                return 1;
            }

            pnm_input = pnm_file;
            tmp_pnm = 1;
        }

        char out[256];
        snprintf(out, sizeof out, "%s.dif", input);

        long in_size = file_size(pnm_input);

        if (verbose)
            printf("Encodage PNM → DIF : %s → %s\n", pnm_input, out);

        if (pnmtodif(pnm_input, out) != DIF_OK) {
            fprintf(stderr, "Erreur encodage PNM\n");
            if (tmp_pnm) remove(pnm_input);
            return 1;
        }

        long out_size = file_size(out);

        if (in_size > 0 && out_size > 0) {
            double ratio = 100.0 * out_size / in_size;
            printf("Taille brute : %ld octets\n", in_size);
            printf("Taille DIF   : %ld octets\n", out_size);
            printf("Compression  : %.2f %%\n", ratio);
        }

        if (tmp_pnm)
            remove(pnm_input);
    }

    return 0;
}
