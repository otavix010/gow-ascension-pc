# God of War: Ascension — port nativo para PC

> **Projeto de fãs, sem fins lucrativos, sem relação com a Sony.** *God of War* é marca registrada da Sony Interactive Entertainment. Este repositório **não contém o jogo** nem o firmware: você precisa da **sua própria cópia** do jogo (BCUS98232, com o update 1.04) e do firmware oficial do PS3.

![Capítulo 16 a 66 FPS num Ryzen 7 9800X3D](docs/imagens/cap16_9800x3d.png)

[English summary below](#english-summary)

## O que é

Um port **nativo** do *God of War: Ascension* (PS3, 2013) para Windows. Não é um emulador: o código do jogo é **traduzido para C/C++ e compilado** para x86-64 (recompilação estática), em cima do [ps3recomp](https://github.com/sp00nznet/ps3recomp) do sp00nznet.

Como o código do jogo não pode ser distribuído, **o seu PC gera e compila a parte do jogo a partir da sua cópia**. Um comando só faz tudo.

| | |
|---|---|
| ![Fase 1, luta com a Fúria](docs/imagens/fase1_dodge.png) | ![Fase 1, close da Fúria](docs/imagens/fase1_furia.png) |

## Como jogar

**1. O que você precisa ter**
- A cópia do **disco** do *God of War: Ascension* **BCUS98232** (EUA), numa pasta que contenha `PS3_GAME`.
- O **update 1.04**: instalado no [RPCS3](https://rpcs3.net) (fica em `RPCS3\dev_hdd0\game\BCUS98232`) ou o arquivo `.pkg`.
- O **RPCS3 com o firmware oficial do PS3 instalado** (*File > Install Firmware*, com o `PS3UPDAT.PUP` do site da PlayStation).

**2. Instale as ferramentas (uma vez só)**, num terminal:
```
winget install Python.Python.3.12
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
```

**3. Baixe este repositório** (botão *Code > Download ZIP*, ou `git clone`) e extraia numa pasta **sem espaços no caminho**, por exemplo `C:\gow-ascension-pc`.

**4. Clique duas vezes em `gerar.cmd`** e escolha, nas janelas, a pasta do disco, a do update (ou o `.pkg`) e a do RPCS3. Ele faz o resto sozinho:
- confere as ferramentas e baixa o FFmpeg;
- prepara os arquivos do jogo, sem copiar os 34 GB do disco;
- decripta o executável;
- traduz e compila, escolhendo a build certa para a sua CPU.

Num Ryzen 7 9800X3D (8 núcleos) levou ~4 minutos; num PC de 4 núcleos leva bem mais. Se parar no meio, rode de novo: ele continua de onde parou.

**5. Jogue com `jogar.cmd`.** O log fica em `out\jogo.log` e os saves em `saves\`.

Opções (defina antes de abrir o `jogar.cmd`): `GOWA_FLIP_HZ` (limitador; padrão 60, `0` desliga, mas o jogo acelera), `GOWA_RES_SCALE` (1 = 720p, 2 = 1440p, 3 = 4K), `GOWA_AUDIO_MUTE=1` (sem som).

Detalhes de cada etapa, e como fazer à mão: [docs/COMO_GERAR.md](docs/COMO_GERAR.md).

## Como funciona

O PS3 tem um processador central (PPU, PowerPC), seis co-processadores vetoriais (SPUs) e uma GPU (RSX):

- **PPU**: o executável do jogo é traduzido para C++ função por função pelo tradutor do ps3recomp, com os remendos do port aplicados por scripts.
- **SPU**: os programas das SPUs (jobs do SPURS, o EDGE de geometria, as tarefas de malha, o gerenciador de políticas) também viram C. Os laços mais pesados podem virar **núcleos vetoriais** gerados por scripts (AVX-512 ou AVX2), conferidos bit a bit. O código que o jogo carrega nas SPUs em tempo de execução e que não foi traduzido roda num interpretador.
- **SPURS**: a biblioteca de agendamento das SPUs (libsre) roda o código do firmware oficial (LLE).
- **RSX**: os comandos da GPU são desenhados em **Direct3D 12**, com os shaders do RSX convertidos para HLSL.
- **Áudio**: WASAPI, até 8 canais. Vídeos (MPEG-2) com FFmpeg.

## Estado atual

- Abre, passa pelos vídeos e menus, carrega saves e roda a **fase 1** e o **Capítulo 16** (a parte mais pesada que testamos) com imagem correta.
- **60 FPS travados** nas duas cenas num Ryzen 7 9800X3D, com o limitador ligado (padrão).
- O jogo inteiro, do começo ao fim, **ainda não foi verificado**.
- Só a versão **BCUS98232 (EUA) 1.04**: o `gerar.ps1` confere o executável e recusa outras.

## Desempenho medido

**Capítulo 16** (a cena mais pesada) e **fase 1**, em FPS de jogo, **sem o limitador de 60** (é como dá para comparar máquinas). **O hardware muda tudo**: o gargalo é a CPU, e CPUs com AVX-512 rodam bem mais rápido.

| PC | CPU | GPU | Build | Cap. 16 | Fase 1 |
|---|---|---|---|---|---|
| Principal | Ryzen 7 9800X3D (8/16, AVX-512) | RTX 5080 | AVX-512 | **~68** | **~84** |
| PC teste 2 | Core i5-11400F (6/12, AVX-512) | RX 9060 XT | AVX-512 (Rocket Lake) | **~32** (1ª execução) | — |
| Notebook | Core i5-10300H (4/8, sem AVX-512, ~2,8 GHz sustentado) | GTX 1650 | sem AVX-512 | **~5,7** | — |

DESEMPENHO_GERADO

![Capítulo 16 no notebook com a build sem AVX-512](docs/imagens/cap16_notebook_portatil.png)

## Requisitos

- **Windows 10/11 64 bits** e GPU com **Direct3D 12**.
- **CPU com AVX2** (Intel Haswell em diante, AMD Zen em diante). **Com AVX-512** (Ryzen 7000/9000, Intel 11ª geração desktop) é muito mais rápido; o `gerar.ps1` escolhe sozinho.
- 16 GB de RAM. Espaço: ~3 GB para a geração e a build (o disco do jogo não é copiado).

## Problemas conhecidos

- **Engasgo na primeira entrada de cada área**: os pipelines de shader são compilados na hora e ficam em cache; da segunda vez some.
- **Sem o limitador de 60** o jogo **acelera**, porque parte da lógica avança por quadro.
- **Trava do leitor da GPU em PC lento**: corrigida nesta versão (o leitor executava blocos de comandos que a PPU ainda escrevia). Validada em 5 de 5 execuções no notebook, mas ainda vale relatar se acontecer.
- Travas raras de carregamento ainda em investigação.
- PC fraco (4 núcleos, sem AVX-512) roda, mas abaixo de 10 FPS nas partes pesadas.

## Licença e créditos

- **Nosso código e ferramentas: [MIT](LICENSE).** O RPCS3 (GPL-2.0) foi usado como referência de comportamento; o código foi revisado linha a linha contra o do RPCS3 e nenhum trecho foi copiado. A única coincidência literal são duas tabelas de valores medidos do hardware, herdadas do ps3recomp.
- **[ps3recomp](https://github.com/sp00nznet/ps3recomp)**, de Ned Heller (sp00nznet), MIT: o tradutor, o runtime e o motor de desenho em que o port se apoia. Ele vem dentro deste repositório, em `ps3recomp/`: o primeiro commit importa a v0.15.0 sem mudanças, e as nossas alterações vêm em commits separados logo depois.
- **`scripts/gpl/unself.py` é GPL-2.0** (`scripts/gpl/COPYING`): o decriptador de SELF, que segue o `unself.cpp` do [RPCS3](https://github.com/RPCS3/rpcs3) e roda como programa separado.
- **Chaves**: nenhuma chave foi criada por nós. As de decriptação de executáveis são lidas do código-fonte do RPCS3: o `gerar.ps1` baixa dois arquivos dele (`key_vault.cpp/.h`, numa versão fixa) para `dl\rpcs3_chaves`, ou usa os que você indicar com `-ChavesRpcs3`. A KLIC do jogo sai da sua própria cópia. O `ps3recomp` embutido traz, como o original, chaves públicas de formatos do PS3.
- **[FFmpeg](https://ffmpeg.org)** (LGPL), baixado pelo `gerar.ps1` dos builds do BtbN, para os vídeos.
- God of War e God of War: Ascension são marcas da Sony Interactive Entertainment. Este projeto não é afiliado nem endossado pela Sony.

---

## English summary

A **native PC port** of *God of War: Ascension* (PS3), made by **static recompilation**: the game's PPU and SPU code is translated to C/C++ and compiled for x86-64 on top of [ps3recomp](https://github.com/sp00nznet/ps3recomp). The GPU command stream is rendered with Direct3D 12, and SPURS runs the official libsre module (LLE).

**Bring your own disc**: no game code or data and no firmware here. You need your own copy (BCUS98232 + update 1.04, installed in RPCS3 or as the .pkg) and RPCS3 with the official PS3 firmware installed. Install Python and the Visual Studio 2022 Build Tools with Clang (commands above), download this repository to a path without spaces, double-click **`gerar.cmd`** (it asks for the folders, then decrypts your copy, translates and builds; ~4 minutes on an 8-core Ryzen 7 9800X3D), then play with **`jogar.cmd`**.

Status: boots, menus, chapter 1 and chapter 16 play with correct image, 60 FPS locked on a Ryzen 7 9800X3D; the full game is not verified yet. Performance is CPU-bound and much better with AVX-512: ~68 FPS uncapped in chapter 16 on a 9800X3D, ~32 on an i5-11400F, ~5.7 on a 4-core laptop without AVX-512.

License: our code is MIT; ps3recomp is MIT (Ned Heller); `scripts/gpl/unself.py` is GPL-2.0 (follows RPCS3's unself.cpp). Decryption keys are read from RPCS3's source (downloaded at a pinned commit). *God of War* is a trademark of Sony Interactive Entertainment; this is a non-commercial fan project, not affiliated with or endorsed by Sony.
