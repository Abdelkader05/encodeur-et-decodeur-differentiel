#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int splitpath(char *fullpath, char **dir, char **file, char **ext) {
    int indice_file = 0;
    int * indice_ext = NULL;
    for (int i = 0; fullpath[i] != '\0'; i++) {
        if (fullpath[i] == '/') indice_file = i + 1;
        if (fullpath[i] == '.' && i > indice_file) *indice_ext = i;
    }
    
    *file = fullpath + indice_file ;
    if (indice_ext != NULL && fullpath[*indice_ext] != '\0') {
        fullpath[*indice_ext] = '\0';
        *ext = fullpath + *indice_ext;
    }
    else {
        *ext = NULL;
    }
    if (indice_file == 0) {
        *dir = NULL;
    } else {
        *dir = fullpath;
        fullpath[indice_file - 1] = '\0';
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s fichier\n", argv[0]);
        return 1;
    }
    
    char *dir = NULL, *file = NULL, *ext = NULL;
    splitpath(argv[1], &dir, &file, &ext);
    printf("dir: %s\n", dir ? dir : "(aucun)");
    printf("file: %s\n", file);
    printf("ext: %s\n", ext);
    return 0;
}
