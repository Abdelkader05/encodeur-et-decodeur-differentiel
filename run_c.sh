#!/bin/bash

set -e  # stoppe le script à la moindre erreur

# Arguments
INPUT_FILE=${1:-test/bulle.128.pgm}
OUTPUT_FILE=${2:-out.dif}
HEX_LINES=${3:-10}

echo "=== Compilation de l'encodeur ==="
make
echo "Compilation terminée."

echo
echo "=== Lancement de la compression ==="
./encodeur -c "$INPUT_FILE" "$OUTPUT_FILE"
echo "Compression terminée."

echo
echo "=== Source PGM : $INPUT_FILE (premières $HEX_LINES lignes) ==="
xxd "$INPUT_FILE" | head -n "$HEX_LINES"

echo
echo "=== Fichier compressé : $OUTPUT_FILE (premières $HEX_LINES lignes) ==="
xxd "$OUTPUT_FILE" | head -n "$HEX_LINES"

echo
echo "=== Tailles des fichiers ==="
ls -lh "$INPUT_FILE" "$OUTPUT_FILE"
