# God of War Ascension nativo — relatório dos 60 FPS e caminhos para 120

*23/10/2026, 10h. Máquina: Ryzen Zen 5, 8 núcleos / 16 threads (`-march=znver5`). Cenas de teste: fase 1 (tutorial) e capítulo 16 (a mais pesada que temos).*

## 1. Onde chegamos

| Cena | Com o limitador de 60 | Sem limitador | Observação |
|---|---|---|---|
| Capítulo 16 | **60,0** cravado | 60,9 | Todas as medidas da última rodada em 60,0 |
| Fase 1 | 59,2 de média (mediana 60) | **74–75** (mediana 75,6) | Cai só num trecho de ~25 s do roteiro (175–200 s) |

O capítulo 16 chegou aos 60. A fase 1 tem folga de sobra, 75 sem limite, mas ainda tem um trecho com engasgos:
- **Engasgos de criação de PSO: resolvidos hoje.** Cada pipeline novo levava até 65 ms no driver, na thread da RSX. Com a biblioteca de PSOs em disco, a partir da segunda execução todos vêm prontos (máximo de 2,2 ms).
- **O que sobra: streaming de textura.** As texturas novas desse trecho são decodificadas na thread da RSX e seguram o quadro por 33–69 ms algumas vezes por segundo. É o próximo alvo, e é o que falta para a fase 1 ficar cravada em 60.

Todas as mudanças de código SPU são conferidas bit a bit contra a versão de referência antes de entrar.

## 2. Evolução

| Quando | Fase 1 | Cap. 16 | Principais mudanças |
|---|---|---|---|
| 17/10 manhã | 3 → 6–10 | — | Primeira jogabilidade |
| 17→18/10 | ~13 → ~23 | — | Módulos de job e policy module das SPUs nativos, correção do heap de VRAM e do timer do Windows 11 |
| 18/10 | ~30–37 | — | Mais código SPU nativo, PPU otimizado |
| 19/10 | ~41–42 | — | CPU e GPU em paralelo (`LD_PIPE`), espera curta com PAUSE, sono real |
| 20/10 | ~43 | ~27,8 | Threads fixadas por núcleo (+8,3%), pacote de correções visuais |
| 21/10 | ~50–51 | ~36 → ~40 | Primeiros núcleos AVX-512 nos laços do EDGE, plugins retraduzidos, saltos diretos entre funções |
| 22/10 | ~55 → ~58 | ~41 → ~50 | Luz do EDGE na GPU, prioridades do SPURS, RSX sem esperar no flip (+13%), hash de textura só nas páginas sujas |
| **23/10** | **~59 (75 sem limite)** | **~49 → 60 (60,9 sem limite)** | Ver a seção 3 |

*Antes de 20/10 a medida contava apresentações, não quadros do jogo, então os números antigos não são estritamente comparáveis. Do capítulo 16 só há medidas a partir de 20/10.*

No capítulo 16 a evolução foi de **~28 para 60 em três dias e meio** (+115%).

## 3. O que rendeu nesta madrugada e manhã (23/10)

Cada linha é um A/B intercalado no capítulo 16, com 4 a 6 medidas de cada lado:

| Mudança | Ganho |
|---|---|
| Núcleo AVX-512 que calcula de uma vez as 4 chamadas da cadeia de ruído do General6 | +3,0% |
| General6 fora das SPUs do EDGE (vai para as SPUs 4, 5 e 6) | +1,4% |
| Parâmetros de DMA escritos direto no contexto, sem chamar o runtime | +3,2% |
| Caminho enxuto do comando de DMA (sem ~50 checagens de diagnóstico por transferência) + filtro na busca de overlays | +2,0% |
| 21 operações SPU que no PC eram laços escalares (multiplicações, extensões de sinal, deslocamentos de 128 bits, conversões inteiro→float) | **+6,6%** |
| Plugins do EDGE que ainda rodavam no interpretador (entrada em +0x18) agora nativos | **+6,8%** |
| Mais 58 operações SPU escritas direto em instruções vetoriais (somas, comparações, deslocamentos...) | **+7,5%** |
| Biblioteca de PSOs em disco | engasgos de até 65 ms na fase 1 → 2 ms |

O que **não** rendeu e ficou desligado:
- **Trava de lock line separada por linha:** cortou 79% da espera, mas o FPS empatou, porque essa espera não estava no caminho crítico.
- **Promoção de registradores no job do EDGE:** deu −10%. Foi a terceira medição negativa, então foi abandonada.
- **Limitador de quadros do lado do jogo:** empatou com o atual.

A lição do dia: o maior ganho veio de **custos gerais do tradutor**, como operações SPU mal traduzidas para o PC e código caindo no interpretador. Otimizar uma função específica rendeu menos.

## 4. Onde está o quadro hoje (linha do tempo, sem limitador)

| | Capítulo 16 (60,3 quadros/s) | Fase 1 (~70 quadros/s) |
|---|---|---|
| SPUs 0–3 (EDGE), trabalho por quadro | **16,6 ms**: job EDGE 9,3, render 2,7, PM do EDGE 2,3, parada 1,3 | 14,3 ms: job EDGE 5,7, render 2,6, PM 1,4, General6 1,0, parada 2,7 |
| Thread principal do PPU | ~75% ocupada (~12 ms) | ~85% ocupada (~12 ms) |
| Thread da RSX | praticamente nunca para (0,7% do tempo esperando) | para ~20% do tempo |

No capítulo 16 **três coisas estão no limite ao mesmo tempo**: as SPUs do EDGE, a thread da RSX e, logo atrás, a thread principal. Daqui para frente, ganhar FPS exige acelerar as três, não só uma.

## 5. Caminhos para 120 FPS

120 FPS = **8,3 ms por quadro**. Hoje, no capítulo 16:

| Recurso | Hoje (por quadro) | Para 120 | Corte necessário |
|---|---|---|---|
| SPUs 0–3 (EDGE) | 16,6 ms | ≤ 8,3 ms | −50% |
| Thread principal (PPU) | ~12 ms | ≤ 8,3 ms | −30% |
| Thread da RSX | ~16 ms | ≤ 8,3 ms | −50% |

Os caminhos, do mais imediato ao mais estrutural:

**A. Continuar o que está rendendo** (dias; estimo +15–30% no capítulo 16)
- Tirar do interpretador as ~83 mil entradas por segundo dos módulos de render. Já está preparado: retradução de 5 módulos com as entradas extras.
- Vetorizar o que ainda sai escalar nos laços quentes. O laço do plugin 30600, o mais pesado do EDGE, ainda tem ~670 instruções por volta.
- Reduzir o custo do despacho de desvio indireto e do dreno do trampolim (~4% das SPUs) e o que resta no caminho de DMA.
- Decodificar textura fora da thread da RSX (resolve também o trecho da fase 1).

**B. EDGE em 6 SPUs em vez de 4** (semanas; +30–50% na parte do EDGE)
- O jogo configura o EDGE para exatamente 4 SPUs (um anel de saída e um label por SPU). Já foi testado rodar o EDGE em mais SPUs e travou por isso.
- Seria preciso mexer na configuração do EDGE no código PPU do jogo.
- A máquina tem 16 threads de hardware e usamos ~9 pesadas. Mas quando duas threads pesadas dividem um núcleo (SMT), cada uma fica mais lenta (medido em 20/10), então o ganho real depende disso.

**C. Thread principal do PPU** (semanas; necessário para passar de ~80)
- Ela precisa cair de ~12 ms para menos de 8,3 ms.
- Caminhos: perfil do código PPU traduzido, as mesmas técnicas das SPUs (operações VMX restantes, operações escalares), e menos custo nas chamadas de sistema/HLE.

**D. Thread da RSX** (semanas; necessário para passar de ~65 no capítulo 16)
- Dividir em duas threads em linha de produção: uma lê o FIFO e o estado, a outra monta e envia os comandos D3D12.
- Levar para a GPU a conversão de vértices (`rsx_vertex_fetch_all` + remapeamento, ~10% da thread), com os shaders lendo os buffers do jogo diretamente.
- Decodificar texturas em threads auxiliares (o mesmo do item A).

**E. EDGE de verdade na GPU** (meses; o maior ganho possível)
- Trocar os jobs de geometria do EDGE (skinning, descarte de triângulos, iluminação) por compute shaders, tirando ~9 ms por quadro das SPUs 0–3.
- É engenharia reversa grande. A "opção (a)" de ontem moveu só a luz e o teto medido foi +2,3%; o resto do EDGE é que pesa.

**F. A lógica do jogo a 120** (verificação, não otimização)
- No PS3 o jogo rodava com quadros variáveis (30–60), então a expectativa era que usasse o tempo real de cada quadro. **24/10: você notou que, sem o limitador (como nos testes), o jogo fica acelerado** — no boot e nos menus, a ~240 FPS, ele anda bem mais rápido que o tempo real. Então ao menos parte da lógica avança por quadro. Para 120 reais vai ser preciso descobrir onde o jogo mede o tempo e adaptar (animações, física, cenas, QTEs e áudio); até lá, o limitador de 60 mantém a velocidade certa.
- O limitador já aceita 120 (`GOWA_FLIP_HZ=120`).

**G. Geração de quadros** (o plano do launcher futuro)
- DLSS/FSR frame generation mostraria 120 na tela a partir de 60 reais. É o caminho mais curto para "120 na tela", mas não são 120 quadros reais (a latência continua a de 60).
- Fica para o launcher, depois do port completo, como você planejou.

**H. Hardware**
- 120 reais exigem paralelismo. Com 8 núcleos, o que já usamos (7 SPUs + principal + RSX) ocupa a máquina.
- Uma CPU de 12–16 núcleos permitiria os caminhos B e D sem dividir núcleo.

**Avaliação:** com A + C + D, o capítulo 16 deve chegar a ~80–90 FPS nativos. Para 120 reais na cena mais pesada, quase certamente é preciso B ou E (mais capacidade para o EDGE) junto com a divisão da thread da RSX. Na fase 1, que já faz 75 sem limite, 120 fica mais perto: A + D podem bastar para ~100.

## 6. Próximos passos imediatos

1. Streaming de textura fora da thread da RSX, para a fase 1 ficar cravada em 60.
2. Retraduzir os 5 módulos de render com as entradas extras (preparado) e o `0x727200`.
3. Perfil novo e os próximos pontos quentes.
4. A partir de agora, os A/Bs rodam com o limitador desligado (`GOWA_FLIP_HZ=0`), para enxergar ganhos acima de 60.

*Detalhes de cada passo no `PROGRESSO.md` (seção 23/10).*

---

## 7. Atualização da noite (23/10)

**Números agora:**
- **Capítulo 16:** 60 com o limitador, 61–63 sem limite.
- **Fase 1:** 59,4 com o limitador (decodificação de textura B8 ~8× mais rápida) e ~75 sem limite.

**O que a tarde mostrou: o teto do capítulo 16 é a latência de uma cadeia, não a falta de capacidade de um recurso só.**

1. **Teste de sensibilidade** (atrasos artificiais):
   - atrasar as SPUs do EDGE derruba o FPS (−18%);
   - atrasar a RSX ou a thread principal não muda nada.
2. **Mas acelerar as SPUs do EDGE também não sobe o FPS.** Os builds da tarde (vc1 a vc5) cortaram ~4% do trabalho do EDGE e do PM por quadro (medido na linha do tempo), e o FPS ficou em 61–63. O tempo liberado nas SPUs 0–3 vira espera e render puxado das outras SPUs.
3. **O quadro (16,5 ms) é uma cadeia.** Ela começa no fim do lote de EDGE, que acorda a thread principal. Depois, em série:

   | Etapa | Começa (ms desde o flip) | Termina | Duração |
   |---|---|---|---|
   | General6 | 5,2 | 11,3 | **6,2 ms, a maior** |
   | RenJobSet | 11,3 | 13,5 | 2,2 ms |
   | RenderJobs | — | 13,5 | — |
   | RSX até o flip | 13,5 | 16,5 | 3,0 ms |

4. **A thread principal passa ~50% do quadro esperando:**
   - ~34% em laços que conferem os jobs do WWS;
   - ~9% esperando a referência da RSX;
   - o resto em outros pontos.

   As SPUs 4/5/6 ficam 17–45% ociosas.

**O que isso muda no caminho para 120.** Cortar trabalho sem encurtar a cadeia não aparece no FPS. As alavancas passam a ser:
- **encurtar a etapa do General6** (`0x727200` + `0x71EE00`), deixando o código dele mais rápido ou espalhando-o por mais SPUs;
- **sobrepor quadros:** começar o General6 do quadro seguinte antes (depende de como o jogo encadeia, a investigar);
- **a parte da RSX no fim da cadeia** (~3 ms do fim dos jobs ao flip, e a principal esperando a RSX).

Os itens B a E da seção 5 continuam valendo para os 120 reais, mas a ordem muda: a latência da cadeia vem antes do volume de trabalho do EDGE.

**Feito na tarde/noite** (cortes de custo geral, todos conferidos; no FPS ficaram dentro do ruído de ±1,5):
- conta de float sem passar pela pilha;
- **131 entradas novas** em 18 módulos de job para o idioma de "entrar em seção crítica" do gerenciador de jobs. Ele caía no interpretador a cada passagem; sumiram as entradas mais frequentes do General6 e do RenJobSet;
- DMA enxuto também dentro do gerenciador de jobs (antes, todo DMA do EDGE passava pelo caminho lento) e listas de DMA no caminho enxuto;
- gancho desnecessário em todo PUT das SPUs desligado.

**Os dois resultados que fecharam a noite:**
- **Fase 1 sem limite: 70,15 → 74,93 (+6,8%)** com esses cortes (b81 × vc5, 4 de cada, as 4 do vc5 acima das 4 do b81). Eles aparecem onde a cadeia não segura o quadro.
- **Travas de lock line por linha** (64 travas em vez de uma única), agora padrão do jogo. No capítulo 16: **59,43 → 61,03 (+2,7%)**, e nenhuma execução caiu. Sem elas, 3 de 6 execuções despencavam para 46–52 no meio da medição, quando o tráfego atômico entre as SPUs sobe.

*Detalhes, números de cada A/B e backups no `PROGRESSO.md` (23/10, tarde e noite).*

## 8. Madrugada de 24/10

**Resumo.** No capítulo 16, sem limitador, a média passou de **60,8 (vc6) para 67,1 (rx9)**, cerca de **+10%**. As execuções também ficaram estáveis (64,7–67,7, sem o modo lento de ~59). Na fase 1, sem limitador, foi de **73,7 para 83,2 (+13%)**. Com o limitador de 60: fase 1 59,8 e capítulo 16 59,3–60,0 (uma queda breve a 56,4 numa execução, depois 60 cravado). **O `gowa.exe` agora é o rx9** (cópia em `gowa_rx9.exe`).

**O que a linha do tempo mostrou no começo da noite.** Com as SPUs mais rápidas, a **thread da RSX ficou ~92% ocupada** o quadro inteiro, com trabalho de verdade. Ela tinha virado um segundo teto, ao lado da cadeia das SPUs.

**Cortes na RSX** (cada um tem uma chave `=0` para voltar ao antigo):
- uma máquina de estado antiga que ninguém lia ainda processava cada palavra do FIFO (~8% da RSX) e agora é pulada;
- pacotes de registradores do FIFO vão em bloco para o motor de desenho;
- cada desenho copia só a faixa de constantes que o shader lê (antes, 8 KB por desenho);
- cache do estado do D3D12: só chama o driver quando algo muda (eram ~15 chamadas por desenho);
- texturas decodificadas uma vez por desenho (eram ~80 decodificações) e tabela de texturas reaproveitada entre desenhos;
- **vértices enviados por atributo:** os bytes do jogo vão direto para a GPU no formato deles (só com troca de bytes), sem converter para float4 na CPU;
- **índices dos segmentos do EDGE** escritos direto da memória do jogo, sem as três tabelas intermediárias.

Resultado: a **RSX caiu de ~92% para ~77% ocupada**, saiu do teto, e o FPS subiu +2% (A/B `abrx3`/`abrx6`).

**Com a RSX folgada, o teto voltou às SPUs do EDGE (0–3), ~92% ocupadas.** Duas mudanças renderam:
- **Render fora da SPU 3, na SPU 6** (+3,4%). A 7ª SPU passa a ler o número 3 no gerenciador de jobs, então assume o lugar da SPU 3 nas listas do General6, do RenJobSet e do RenderJobs. A SPU 3 fica só para o EDGE. Tirar o render de todas as quatro foi pior: as SPUs 4–6 não dão conta.
- **Saltos diretos durante esperas de DMA** (+2,8%). Cada espera de DMA do EDGE, e a janela logo depois que ele religa as interrupções, passava pelo laço central uma instrução por vez: ~37 milhões de voltas por segundo. Agora esses saltos contam no atraso de interrupção e seguem direto. Em passos, a interrupção chega no mesmo ponto de antes, e por isso não se repete o problema de 21/10, quando saltos diretos nesse trecho tinham derrubado o jogo para 22 FPS.

**Testado e não adotado:**
- system service do SPURS traduzido. Era 90% do código SPU ainda interpretado: caiu de ~650 para ~48 milhões de instruções a cada 10 s, mas estava no caminho ocioso e o FPS não mudou. Fica disponível com `GOWA_SYSSRV_NATIVE=1`;
- General6 também na SPU 4: encurta a etapa dele, mas atrasa os tasksets e o RenderJobs, e o FPS piora;
- fixação de threads modo 3: deixa as execuções estáveis, mas mais lentas.

**Sua observação: sem limitador o jogo fica acelerado.** Os testes rodam sem o limitador. No boot e nos menus, a ~240 FPS, o jogo anda bem mais rápido que o tempo real, então ao menos parte da lógica avança por quadro. Em uso normal o limitador de 60 mantém a velocidade certa. **Para 120 reais vai ser preciso adaptar o tempo do jogo** (seção 5, item F).

**Próximos passos para os 120:**
- o teto agora são as 4 SPUs do EDGE, presas a 4 pelo próprio jogo (anéis de saída por SPU);
- sobram duas frentes: a sobrecarga por job (gerenciador de jobs + runtime, ~25% dessas SPUs) e os próprios jobs do EDGE (~57%);
- e o tempo do jogo a 120.

*Detalhes, A/Bs e backups no `PROGRESSO.md` (23–24/10, madrugada).*

**Manhã de 24/10** (o PC desligou às 04:40 e às 08:53 e a luz caiu às 06:33; as medições perto disso foram refeitas):
- **O `gowa.exe` é o rx9** desde as 06:54. Depois do religamento das 11:50 a máquina mede o rx9 em ~68–69 no cap. 16, sem limitador.
- Mexi no despacho das SPUs, sempre com o mesmo comportamento: uma frente enxuta para os desvios indiretos (`GOWA_IB_FAST`), o stub de salvar/restaurar registradores memorizado (`GOWA_TOPLS_MEMO`) e **três entradas traduzidas que faltavam**, achadas com o diagnóstico novo `GOWA_SMC_HIST`. Uma é o tratador de interrupção do PM do EDGE, cujo vetor o PM grava em tempo de execução. Outra é a volta de cada job do EDGE para o PM. A terceira é uma função de 4 instruções do EDGE chamada por ponteiro. Juntas, essas três passavam ~570 mil vezes por segundo pelo micro-interpretador.
- Resultado: o despacho caiu de 7,0% para 5,2% das threads do EDGE, mas o FPS **empatou** (rx9 68,05 × rx16 68,48).
- Por quê: as 4 SPUs do EDGE ficam 80–94% ocupadas e esperam trabalho no resto do tempo. A thread principal gira ~4,4 ms por quadro esperando os jobs do EDGE terminarem. O teto do cap. 16 é a **cadeia** (a principal manda os jobs, o EDGE processa, a principal espera), não só a vazão das SPUs. Cortes de ~2% de CPU nelas ficam abaixo do ruído do A/B (±1,5%).
- Também empataram: orçamento de saltos 128 e atraso de interrupção 32.

