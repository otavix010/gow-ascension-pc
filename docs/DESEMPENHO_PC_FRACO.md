# Desempenho em PC fraco — o que medimos no notebook de teste

*24–25/10/2026. Notebook de teste: Core i5-10300H (4 núcleos / 8 threads, sem AVX-512, ~2,8 GHz sustentado no limite de potência), GTX 1650 4 GB, tela de 144 Hz. Cena: Capítulo 16 (a mais pesada que temos), sem o limitador de 60. Build gerada pelo `gerar.cmd` (portátil x86-64-v3 com as contas de float das SPUs em arredondamento para zero).*

## Resultado

| Quando | Cap. 16 | O que mudou |
|---|---|---|
| 24/10 início | 4,0 | primeira build portátil |
| 24/10 tarde | 5,7 | contas de float das SPUs em arredondamento para zero, núcleos vetoriais em AVX2 |
| 24/10 noite | 3,9 → empate com a nossa | passes de desempenho na geração (`scripts/otimizar.py`) |
| 25/10 | **~8 (à noite) a ~10 (de manhã)** | fixação de threads desligada com menos de 12 CPUs lógicas (+4%); correção da trava do leitor da GPU |

O mesmo notebook variou de ~7 para ~10 FPS entre a noite e a manhã, com a mesma build. Não sabemos o motivo (temperatura, modo de energia, outros programas). Por isso só valem comparações intercaladas.

## Onde está o limite

- **CPU, nas SPUs.** Umas 8 threads usam de 60 a 100% de CPU cada (as SPUs do EDGE, a principal, a RSX) em 8 CPUs lógicas. A thread principal passa boa parte do quadro esperando os jobs das SPUs.
- Por quadro, o trabalho do EDGE custa ~430 ms de núcleo no notebook contra ~54 ms no 9800X3D: **8×**. Clock (×1,9), AVX-512 → AVX2 (×1,5) e duas threads por núcleo (×1,6) explicam ~4,5×. O resto deve ser memória: o L2 do i5-10300H tem 256 KB por núcleo, o mesmo tamanho da memória local de **uma** SPU, e cada núcleo roda duas.
- A GPU não é o limite: na fase, a GTX 1650 fica em 13–29% de uso (435–1380 MHz, 5–18 W, pelo `nvidia-smi`), e a thread da RSX passa boa parte do tempo esperando comandos.

## O que testamos e não ajudou

Cada linha: duas ou mais rodadas intercaladas, FPS na mesma janela de 200 s.

| Teste | Resultado |
|---|---|
| `-march=skylake` (o que o `gerar.cmd` escolhe nesse notebook) em vez de x86-64-v3 | igual ou pior |
| Código SPU compilado em `-Os` (para caber melhor no cache) | +2%, dentro do ruído |
| System service do SPURS traduzido (`GOWA_SYSSRV_NATIVE=1`), com ou sem o PM de taskset (`GOWA_TSPM_NATIVE=1`) | igual |
| Giros da lock line (`GOWA_LL_SPIN=16`) | igual |
| Descarte de triângulos na GPU (`GOWA_EDGE_NOCULL=1`) | ruído (+4% e −6%) |
| 6 SPUs como no PS3 (`GOWA_SPURS_NSPU=6`) em vez de 7 | pior |
| `GOWA_PIN=2` (threads leves fora dos núcleos das pesadas) | bem pior (−30%) |
| Promoção de registradores e constantes nas tarefas do SPURS | igual |
| Geração de quadros (`GOWA_FRAMEGEN=1`) | igual no FPS do jogo; com a base tão baixa ela não interpola |
| Sem vsync (`GOWA_PRESENT_SYNC=0`) | igual |

## O que ajudou

- **Fixação de threads só com 12 ou mais CPUs lógicas.** Com ~9 threads pesadas e 8 CPUs lógicas, prender as 4 mais pesadas uma por núcleo tira do Windows a chance de usar a CPU lógica que fica livre. Sem fixar: 8,06 contra 7,75 (+4%, 3 de 3 pares). No 9800X3D (16 lógicas), fixar continua dando +8%. `GOWA_PIN=1` força e `GOWA_PIN=0` desliga.
- **Correção da trava do leitor da GPU.** Antes, 4 travas em 9 rodadas; depois, 44 rodadas na fase sem nenhuma.

## Como medir sem se enganar

- **Rodadas intercaladas** (A, B, A, B…), nunca uma série de cada.
- **FPS numa janela fixa** depois do início da fase: as amostras de 5 s de 40 s a 240 s depois de `go to playing` no `out\jogo.log` (linhas `[fps] ... jogo=`). A média de "até o fim" muda com a duração da rodada.
- **Carga de fundo**: o indexador de pesquisa do Windows chegou a ~14% da CPU, e um navegador aberto pesa. Anote o que mais estava rodando.
- **O perfil por amostragem (`GOWA_HOSTPROF`) exagera o tempo dentro de chamadas do sistema**: o `GetWriteWatch` do cache de texturas parecia 11% da thread da RSX, mas a medição direta deu ~1%. Confirme com cronômetro antes de otimizar.
- Problema aberto: o perfil por instrução (`GOWA_HOSTPROF_RIP=2`) derrubou o jogo nas duas vezes em que rodou no notebook; o perfil por função funciona.

## Para quem quiser ajudar

O que mais renderia em CPU sem AVX-512 é o código das SPUs do EDGE em AVX2 (os laços `F538`, `A880`, `D250`, `DCC0`, `E550` do módulo 0x72DF80 e os plugins 0x74C580/0x752710). Relatórios de outros PCs (CPU, núcleos, FPS no cap. 16, o fim do `out\jogo.log`) ajudam a montar a tabela do README.
