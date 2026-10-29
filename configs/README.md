# Configurações

Na API BRKGA-MP-IPR, **BRKGA-MP e BRKGA-MP-IPR são o mesmo código**: o que os
separa é o arquivo de parâmetros. Por isso há um único `src/` e duas pastas de
configuração.

```
brkga-mp/       multi-parent ligado, IPR desligado (ipr_interval 0)
brkga-mp-ipr/   multi-parent + Implicit Path Relinking
```

Dentro de cada pasta, um arquivo por decodificador, porque a representação muda
o que faz sentido no path relinking e no shaking:

| Decodificador | `pr_type` | `pr_distance_function_type` | `shaking_type` |
|---|---|---|---|
| limiares (*threshold*) | `DIRECT` | `CUSTOM` (Hamming sobre os 4 rótulos) | `CHANGE` |
| permutação | `PERMUTATION` | `KENDALLTAU` | `SWAP` |

## Estado atual: parâmetros ainda não calibrados

As pastas estão vazias de propósito. Os valores usados no TCC 1 vinham de uma
calibração por irace feita sobre **outra base de comparação**, que foi
descartada (ver `docs/`). Com o VNS de Djukanović et al. (2025) e o PLI como
novas referências, os parâmetros precisam ser recalibrados antes de qualquer
experimento definitivo. Enquanto isso não acontece, nenhum arquivo de
configuração aqui deve ser tratado como valor de referência.
