#!/bin/bash

set -e  # stoppe le script à la moindre erreur

# Arguments
INPUT_FILE=${2:-out.dif}
OUTPUT_FILE=${1:-decoder.pnm}
#INPUT_FILE=${2:-dif/bulle.128.dif}
#OUTPUT_FILE=${1:-decoderTEST.pnm}
HEX_LINES=${3:-10}

make

echo
./encodeur -d "$INPUT_FILE" "$OUTPUT_FILE"

echo
xxd "$INPUT_FILE" | head -n "$HEX_LINES"

echo
xxd "$OUTPUT_FILE" | head -n "$HEX_LINES"

echo
echo "=== Tailles des fichiers ==="
ls -lh "$INPUT_FILE" "$OUTPUT_FILE"
