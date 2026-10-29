#!/usr/bin/env bash
# limpar-zone-identifier.sh
# Uso: ./limpar-zone-identifier.sh /caminho/da/pasta   (padrão: pasta atual)

DIR="${1:-.}"

echo "Arquivos encontrados em: $DIR"
find "$DIR" -type f -name '*:Zone.Identifier' -print

read -rp "Apagar todos esses arquivos? [s/N] " resp
if [[ "$resp" == [sS] ]]; then
  find "$DIR" -type f -name '*:Zone.Identifier' -delete
  echo "Concluído."
else
  echo "Cancelado."
fi