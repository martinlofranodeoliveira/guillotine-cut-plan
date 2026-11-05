# Guillotine Cut Plan

Versão de desenvolvimento **0.1.0**, por [Lofrano Tecnologia](https://lofranotecnologia.com.br).

Heurística original e determinística em C++20 para retângulos, com espessura de corte e certificados de guilhotina verificáveis. Destina-se a experimentos de planejamento: **não controla máquinas, não fornece instruções de uso físico, não garante otimalidade e não prova inviabilidade**. Exemplos e testes são sintéticos. Não há modelo, API, telemetria, rede, relógio ou persistência em tempo de execução.

[English](README.md) · [Contrato](docs/contract.md) · [Projeto](docs/design.md) · [Dependências](docs/dependencies.md)

## Instalação, testes e demonstração

Use GCC 14.3, GNU Make, Bash e os cabeçalhos nlohmann/json **3.11.2**. A biblioteca geométrica usa apenas a biblioteca padrão C++20. Em Debian Bookworm:

```sh
apt-get update
apt-get install --no-install-recommends nlohmann-json3-dev=3.11.2-2
make
bash scripts/test.sh
make demo
make demo-json
```

A instalação de pacotes requer um ambiente autorizado para gerenciamento de pacotes; o programa não instala nada. O script executa `make test` e `make sanitize-test`, incluindo AddressSanitizer e UndefinedBehaviorSanitizer sem supressões. Os executáveis ficam em `build/`. Consulte o documento de dependências para o checksum fornecido do cabeçalho e a limitação referente ao digest da imagem.

Saída legível da demonstração:

```text
PASS sheets=1 used_area=54 kerf_area=6
```

Saída para processamento automatizado:

```json
{"ok":true,"summary":{"cuts":1,"kerf_area":6,"placed_items":2,"remaining_area":0,"remaining_leaves":0,"sheets":1,"stock_area":60,"used_area":54}}
```

A demonstração exige as posições exatas, reproduz o corte em uma grade independente, compara os totais com o verificador e só retorna sucesso se houver concordância. Não é um benchmark.

## Linha de comando

```sh
./build/cutplan plan --input examples/kerf.json --human
./build/cutplan plan --input examples/kerf.json > build/plan.json
./build/cutplan verify --input build/plan.json
./build/cutplan verify --input - < build/plan.json
```

A entrada é um arquivo explícito ou stdin explícito (`--input -`). A saída padrão de `plan` contém `request` e `certificate` em um objeto JSON canônico, aceito diretamente por `verify`. Um plano completo só é emitido após verificação. `--human` mostra somente o resumo verificado. `verify` produz o mesmo envelope de resumo mostrado acima. Códigos de saída: 0 sucesso, 2 erro de entrada/planejamento/verificação/E/S, 64 uso incorreto. Falhas têm `ok:false`, com `code`, `message` e `unplaced`, sem certificado parcial.

No exemplo, A ocupa `[0,6) × [0,6)`, a faixa `[6,7) × [0,6)` é perdida pelo corte e B ocupa `[7,10) × [0,6)`. Se B tiver largura 4, a heurística usa duas chapas; não pode ocupar a faixa de corte.

## Uso da biblioteca

```cpp
#include <guillotine/planner.hpp>
#include <guillotine/verify.hpp>

const guillotine::Request pedido{
    {4, 3}, 0, 1, {{"peca", 3, 4, true}}
};
auto resultado = guillotine::plan(pedido);
if (const auto* certificado = std::get_if<guillotine::Certificate>(&resultado)) {
    auto verificacao = guillotine::verify_certificate(pedido, *certificado);
    // Examine Summary ou Error antes de usar os totais.
} else {
    const auto& erro = std::get<guillotine::Error>(resultado);
    (void)erro;
}
```

Compile com `-std=c++20 -Iinclude` e `src/validation.cpp src/planner.cpp src/verify.cpp src/summary.cpp`. A rotação desse exemplo é 90 graus. Entradas não são alteradas, chamadas são reentrantes e não compartilham estado mutável. `summarize` também verifica o certificado antes de calcular o resumo.

## Decisões e limitações

Uma floresta plana e limitada evita JSON recursivo profundo. Regiões inteiras semiabertas permitem contato exato entre bordas. O verificador foi escrito separadamente do planejador e reconstrói as partições. Os testes incluem uma grade celular independente, comparações de sobreposição aos pares e todas as 81 combinações ordenadas de dois retângulos com lados em {1,2,3} sobre chapa 3×3 com corte zero.

A ordem é área decrescente, maior lado decrescente e ID ASCII crescente. Entre folhas existentes, escolhe-se menor desperdício, com desempates documentados por chapa, coordenadas e orientação. Só se abre nova chapa quando nenhuma folha existente aceita a peça. A peça é colocada no canto superior esquerdo, com corte vertical antes do horizontal. Não há busca com retrocesso.

Limites: 128 peças, 32 chapas idênticas, dimensões de 1 a 1.000.000 e espessura de corte de 0 a 1.000.000, em unidades inteiras definidas pelo chamador. IDs são ASCII únicos. O JSON segue um esquema restrito e limitado, não uma interface genérica de processamento JSON. Números fracionários ou exponenciais, chaves duplicadas, campos desconhecidos e profundidade excessiva são rejeitados.

O modelo não representa tolerâncias, deformação, mecânica da lâmina, orientação das fibras, fixação, restrições operacionais ou segurança física. O certificado demonstra apenas a geometria inteira documentada. A heurística pode gastar mais chapas que o necessário ou falhar apesar de existir outra disposição viável. Não há alegação de conformidade ampla com normas nem de prontidão para produção.

Repositório: [martinlofranodeoliveira/guillotine-cut-plan](https://github.com/martinlofranodeoliveira/guillotine-cut-plan). Contato geral: [site oficial](https://lofranotecnologia.com.br). Licença MIT, copyright 2026 Lofrano Tecnologia.
