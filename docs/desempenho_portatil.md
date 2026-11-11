# Desempenho no portátil (Linux / SteamOS)

Resultados do God of War: Ascension no **Portátil** da nossa bancada, a versão **Linux** (beta) do port.

## Aparelho

| | |
|---|---|
| Modelo | Lenovo Legion Go S (83N6) |
| CPU | AMD Ryzen Z1 Extreme, 8 núcleos / 16 threads |
| GPU | Radeon 780M integrada (RDNA3) |
| RAM | 32 GB de fábrica: 8 GB reservados para a GPU (VRAM) e ~23 GB para o sistema. A VRAM reservada é configurável; ainda vamos testar com mais (estouro de RAM/VRAM é uma das causas das quedas) |
| Sistema | SteamOS 3.10 |
| Proton | GE-Proton11-7 |
| Port | build portátil (AVX2), 6 SPUs (padrão em aparelho com bateria), resolução interna 1280x720 |
| TDP | 15 W (principal) e 30 W |

## Como foi medido

Sem o limitador de 60 (`GOWA_FLIP_HZ=0`). Cada medida são 15 amostras de 5 s, depois de carregar o checkpoint, com o Kratos parado na cena. Os nomes são os checkpoints internos do jogo. A ferramenta mede média e mínimo por janela de 5 s (não há 1% low). O Capítulo 16 e a fase 1 (usados na tabela dos PCs) **não** foram medidos no portátil.

## Resultados

| Cena (checkpoint) | TDP | FPS |
|---|---|---|
| Começo do jogo (Stint500), 3 rodadas | 15 W | 46–49 |
| Começo do jogo, rodada longa de 25 min | 15 W | média 48,0; mínimo 42,7; sem queda com o tempo |
| Lan260 e Fury300 | 15 W | ~44 (mínimo ~39–41) |
| Lan260 e Fury300 | 30 W | 65–68 (mínimo ~59–67); com o limitador de 60, travado em 60 |
| Combate pesado com muitos inimigos (Cyn300) | 15 W | ~24 (mínimo ~23); o áudio corta (até 25% dos blocos atrasados) |
| Combate pesado (Cyn300) | 25–30 W | em medição |

Para comparar: nessa mesma cena de combate pesado, o PC 1 (Ryzen 7 9800X3D) roda a ~48 FPS sem limitador.

## Estabilidade

Nenhum travamento nem queda do jogo em cerca de 140 rodadas de teste. O jogo gerado e aberto pelo Otavix Launcher também abriu a 60 FPS.

## Onde ficam os números brutos

Uma linha por rodada, no próprio aparelho de teste: `~/gowa-linux-teste/banc/resultados.txt`. O jogo também grava um resumo em `desempenho.json` ao lado do executável (CPU, GPU, FPS médio, mínimo e máximo).

O combate pesado é o que estamos otimizando agora; esta página será atualizada.
