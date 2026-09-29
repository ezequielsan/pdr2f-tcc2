# Meta-heurísticas para o Problema da Dominação Romana 2-Forte (PDR2F)

Implementações em C++ do **BRKGA-MP** e do **BRKGA-MP-IPR** para o PDR2F,
construídas sobre a API [BRKGA-MP-IPR](https://github.com/ceandrade/brkga_mp_ipr_cpp)
de C. E. Andrade, além do modelo de programação linear inteira usado como
referência exata.

> Trabalho de Conclusão de Curso 2 — Engenharia de Software, UFC Quixadá.

## Estrutura

```
src/          código compartilhado (instância, decodificadores, distâncias, heurísticas)
apps/         programas principais (main_*)
configs/      parâmetros: uma pasta por algoritmo
pli/          modelo de programação linear inteira
instances/    instâncias, por classe
results/      saídas dos experimentos, por classe
scripts/      baterias de execução e análise
external/     API BRKGA-MP-IPR (não versionada)
docs/         anotações e material de apoio
```

## Instalação

```bash
git clone --depth 1 https://github.com/ceandrade/brkga_mp_ipr_cpp external/brkga_mp_ipr_cpp
make
```

## Execução

(a preencher)
