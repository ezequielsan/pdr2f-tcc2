#!/usr/bin/env bash
###############################################################################
# teste_leitor.sh: testes do leitor de matriz de adjacencia (PDR2F_Instance).
#
# Cada caso monta um arquivo sintetico e confere o que o instance_info
# devolve. Cobre as variacoes de formato presentes na base (separador,
# ponto flutuante, diagonal 0 ou 1) e os casos de borda que nao aparecem
# nela, mas quebrariam o leitor em silencio.
#
# Uso:  make instance_info && ./scripts/teste_leitor.sh
###############################################################################

set -u

BIN=./instance_info
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

passou=0
falhou=0

# ok <nome> <arquivo> <n> <m> <isolados>
ok() {
    local nome="$1" arq="$2" n="$3" m="$4" iso="$5"
    local linha
    linha=$("$BIN" "$arq" 2>&1 | tail -1)
    local gn gm gi
    gn=$(echo "$linha" | cut -d, -f2)
    gm=$(echo "$linha" | cut -d, -f3)
    gi=$(echo "$linha" | cut -d, -f7)
    if [ "$gn" = "$n" ] && [ "$gm" = "$m" ] && [ "$gi" = "$iso" ]; then
        printf '  ok   %-44s n=%-5s m=%-6s isolados=%s\n' "$nome" "$gn" "$gm" "$gi"
        passou=$((passou + 1))
    else
        printf '  FALHA %-43s esperado n=%s m=%s iso=%s | obtido n=%s m=%s iso=%s\n' \
               "$nome" "$n" "$m" "$iso" "$gn" "$gm" "$gi"
        falhou=$((falhou + 1))
    fi
}

# erro <nome> <arquivo>: o leitor DEVE recusar o arquivo com uma excecao
# tratada (codigo de saida 2). Qualquer outro codigo e' falha: 0 significa que
# o arquivo invalido foi aceito, e codigos acima de 128 indicam sinal, isto e',
# o programa quebrou em vez de recusar.
erro() {
    local nome="$1" arq="$2" codigo
    "$BIN" "$arq" > /dev/null 2>&1
    codigo=$?
    if [ "$codigo" -eq 2 ]; then
        printf '  ok   %-44s recusado com excecao tratada\n' "$nome"
        passou=$((passou + 1))
    elif [ "$codigo" -eq 0 ]; then
        printf '  FALHA %-43s arquivo invalido foi ACEITO\n' "$nome"
        falhou=$((falhou + 1))
    elif [ "$codigo" -gt 128 ]; then
        printf '  FALHA %-43s QUEBROU com sinal %s em vez de recusar\n' \
               "$nome" "$((codigo - 128))"
        falhou=$((falhou + 1))
    else
        printf '  FALHA %-43s codigo de saida inesperado: %s\n' "$nome" "$codigo"
        falhou=$((falhou + 1))
    fi
}

echo "=============================================================="
echo " Testes do leitor de matriz de adjacencia"
echo "=============================================================="

# --- 1. formato Harwell-Boeing/geoinstances: inteiro, diagonal 0, espaco ---
printf '0 1 0\n1 0 1\n0 1 0\n' > "$TMP/int_diag0.txt"
ok "inteiro, diagonal 0, separado por espaco" "$TMP/int_diag0.txt" 3 2 0

# --- 2. formato random/wireless: float, diagonal 1.0 (auto-laco a ignorar) ---
printf '1.0 1.0 0.0\n1.0 1.0 1.0\n0.0 1.0 1.0\n' > "$TMP/float_diag1.txt"
ok "float, diagonal 1.0 (ignorada)" "$TMP/float_diag1.txt" 3 2 0

# --- 3. formato do repositorio do VNS: virgula + espaco ---
printf '1.0, 1.0, 0.0\n1.0, 1.0, 1.0\n0.0, 1.0, 1.0\n' > "$TMP/virgula.txt"
ok "separado por virgula e espaco" "$TMP/virgula.txt" 3 2 0

# --- 4. CRLF do Windows ---
printf '0 1 0\r\n1 0 1\r\n0 1 0\r\n' > "$TMP/crlf.txt"
ok "fim de linha CRLF" "$TMP/crlf.txt" 3 2 0

# --- 5. tabulacao como separador ---
printf '0\t1\t0\n1\t0\t1\n0\t1\t0\n' > "$TMP/tab.txt"
ok "separado por tabulacao" "$TMP/tab.txt" 3 2 0

# --- 6. linhas em branco (no meio, no fim) e espacos sobrando ---
printf '0 1 0 \n\n1 0 1\n0 1 0\n\n\n' > "$TMP/brancos.txt"
ok "linhas em branco e espacos sobrando" "$TMP/brancos.txt" 3 2 0

# --- 7. sem o fim de linha final ---
printf '0 1 0\n1 0 1\n0 1 0' > "$TMP/sem_nl.txt"
ok "sem quebra de linha no fim do arquivo" "$TMP/sem_nl.txt" 3 2 0

# --- 8. diagonal com valor nao nulo DEVE ser ignorada, nao virar auto-laco ---
printf '1 0 0\n0 1 0\n0 0 1\n' > "$TMP/so_diagonal.txt"
ok "matriz identidade: 3 isolados, nenhuma aresta" "$TMP/so_diagonal.txt" 3 0 3

# --- 9. grafo sem nenhuma aresta (caso esztergom e oceania) ---
printf '0 0 0 0\n0 0 0 0\n0 0 0 0\n0 0 0 0\n' > "$TMP/sem_aresta.txt"
ok "matriz nula: 4 isolados" "$TMP/sem_aresta.txt" 4 0 4

# --- 10. isolados misturados com componente conexa (caso alameda) ---
printf '0 1 0 0\n1 0 0 0\n0 0 0 0\n0 0 0 0\n' > "$TMP/misto.txt"
ok "uma aresta e dois isolados" "$TMP/misto.txt" 4 1 2

# --- 11. grafo completo K4 ---
printf '0 1 1 1\n1 0 1 1\n1 1 0 1\n1 1 1 0\n' > "$TMP/k4.txt"
ok "K4: 6 arestas" "$TMP/k4.txt" 4 6 0

# --- 12. matriz 1x1 ---
printf '0\n' > "$TMP/um.txt"
ok "matriz 1x1" "$TMP/um.txt" 1 0 1

# --- 13. linha curta: deve ser RECUSADA ---
printf '0 1 0\n1 0\n0 1 0\n' > "$TMP/curta.txt"
erro "linha com menos valores que n" "$TMP/curta.txt"

# --- 14. linha longa: deve ser RECUSADA ---
printf '0 1 0\n1 0 1 1\n0 1 0\n' > "$TMP/longa.txt"
erro "linha com mais valores que n" "$TMP/longa.txt"

# --- 15. todas as linhas mais longas que n: deve ser RECUSADO ---
# Caso critico: n vem da CONTAGEM DE LINHAS, nao do tamanho da primeira linha.
# Sem a guarda de limites no leitor, isto escrevia fora do vetor de listas de
# adjacencia (era um segmentation fault).
printf '0 1 1 1 1\n1 0 1 1 1\n1 1 0 1 1\n' > "$TMP/larga.txt"
erro "3 linhas com 5 valores cada" "$TMP/larga.txt"

# --- 16. arquivo vazio: deve ser RECUSADO ---
: > "$TMP/vazio.txt"
erro "arquivo vazio" "$TMP/vazio.txt"

# --- 17. arquivo inexistente: deve ser RECUSADO ---
erro "arquivo inexistente" "$TMP/nao_existe.txt"

echo "--------------------------------------------------------------"
echo " passou: $passou    falhou: $falhou"
echo "=============================================================="

[ "$falhou" -eq 0 ]
