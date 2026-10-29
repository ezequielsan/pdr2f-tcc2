#!/bin/bash

INPUT_DIR="input"
OUTPUT_DIR="output"
JULIA_SCRIPT="pli.jl"

mkdir -p "$OUTPUT_DIR"

find "$INPUT_DIR" -type f -name "*.txt" | while IFS= read -r file; do

    # Caminho relativo em relação a input/
    relative_path="${file#$INPUT_DIR/}"

    # Diretório relativo
    relative_dir=$(dirname "$relative_path")

    # Nome do arquivo sem extensão
    filename=$(basename "$file" .txt)

    # Diretório correspondente em output/
    output_subdir="$OUTPUT_DIR/$relative_dir"

    # Arquivo de saída esperado
    output_file="$output_subdir/$filename.csv"

    # Cria a pasta de saída
    mkdir -p "$output_subdir"

    # Verifica se o resultado já existe
    if [ -f "$output_file" ]; then
        echo "Já processado, pulando:"
        echo "  $file"
        echo "  -> $output_file"
        echo "-----------------------------------"
        continue
    fi

    echo "Processando:"
    echo "  Entrada: $file"
    echo "  Saída:   $output_file"

    julia "$JULIA_SCRIPT" "$file" "$output_subdir"

    echo "-----------------------------------"

done