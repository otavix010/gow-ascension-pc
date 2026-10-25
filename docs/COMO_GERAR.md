# Como gerar a parte do jogo a partir da sua cópia

Este repositório não traz nada do jogo. O `gowa.exe` contém o código do jogo traduzido, então **cada jogador gera e compila o seu**, a partir da própria cópia e do firmware oficial do PS3.

> **Estado atual (primeira versão pública).** O caminho abaixo é o que usamos de fato, mas **ainda não é um clique só**. Há passos manuais, e alguns ganhos de desempenho vieram de ajustes que ainda não viraram script (listados no fim). A automação completa é a próxima etapa: o port vai traduzir e compilar sozinho na primeira abertura, como o port do FIFA. Se algo der errado, abra uma issue com o log.

Só funciona com **God of War: Ascension BCUS98232 (EUA) + update 1.04**: os endereços usados pelos scripts e pelo port são dessa versão.

## 0. O que instalar

- Windows 10/11 64 bits.
- **Visual Studio 2022 Build Tools** com "Desenvolvimento para desktop com C++" e **"C++ Clang tools for Windows"** (o clang-cl compila o ps3recomp; o `cl` não).
- **CMake** 3.20 ou mais novo e **Ninja**, os dois no PATH.
- **Python 3.12** (os scripts só usam a biblioteca padrão).
- **Git for Windows**, para o Git Bash: alguns scripts são `.sh`.
- **RPCS3**: para instalar o update e o firmware e para decriptar os executáveis.
- **FFmpeg 9.0** "lgpl-shared" para Windows (builds do BtbN, `ffmpeg-n9.0-latest-win64-lgpl-shared-9.0`). Ele serve aos vídeos do jogo.
- Disco: ~35 GB para os arquivos do jogo e ~10 GB para o código gerado e a build.
- Clone (ou baixe o ZIP) num caminho sem espaços, por exemplo `C:\gow-ascension-pc`:
  ```
  git clone https://github.com/otavix010/gow-ascension-pc C:\gow-ascension-pc
  ```

Nos comandos abaixo, a pasta atual é a raiz do clone.

## 1. O jogo

1. **Disco**: copie o conteúdo do disco BCUS98232 para `vfs\PS3_GAME\` (`PARAM.SFO` e `USRDIR\` com os `.PSARC`, ~34 GB).
2. **Update 1.04**: instale o pacote do update no RPCS3 (*File > Install Packages*). Da pasta `dev_hdd0\game\BCUS98232\` do RPCS3, copie:
   - `PARAM.SFO`, `ICON0.PNG`, `PS3LOGO.DAT`, `TROPDIR\`, `USRDIR\PATCHES\` e `USRDIR\TPPS*.PSARC` para `vfs\game\BCUS98232\`;
   - `USRDIR\EBOOT.BIN` e `USRDIR\GOWA.SELF` para `jogo\`.
3. **Decriptar o `GOWA.SELF`**: no RPCS3, use *Utilities > Decrypt PS3 Binaries* no `jogo\GOWA.SELF` e salve o resultado como `jogo\GOWA.elf`.
   - Ele pede uma **KLIC**. Rode o jogo uma vez no RPCS3: a KLIC aparece no `RPCS3.log`, na linha com `KLIC`.
   - Nós usamos um script próprio para essa etapa. O caminho pela interface do RPCS3 ainda não foi testado do zero; se ele falhar, abra uma issue.
4. **Confira** (tem que bater; senão os endereços do port não servem):
   ```
   certutil -hashfile jogo\GOWA.elf SHA256
   813723ac4e091d79ea20ad2500b2e86cb0fd25f3736541431921273e7a77246d   (8.716.728 bytes)
   ```

## 2. O firmware (a libsre roda por LLE)

A biblioteca de agendamento das SPUs (libsre) vem do firmware oficial. O port roda o código dela de verdade.

1. No RPCS3, instale o firmware oficial do PS3 (*File > Install Firmware*, arquivo `PS3UPDAT.PUP` do site da PlayStation; usamos o 4.93).
2. Copie `dev_flash\sys\external\libsre.sprx` (da pasta do RPCS3) para `fw\`. Decripte com *Utilities > Decrypt PS3 Binaries* e salve como `fw\libsre.prx`.
3. Traduza a libsre:
   ```
   cd ps3recomp
   python tools\lift_prx.py ..\fw\libsre.prx --base 0x30000000 --output ..\fw\lle
   python tools\prx_exports.py ..\fw\libsre.prx --symbol libsre --out ..\fw\lle\libsre_exports.c --json ..\fw\lle\libsre_exports_c.json
   cd ..
   python scripts\libsre_stubs_json.py fw\lle\libsre_imports.json fw\lle\libsre_hle_stubs.json
   cd ps3recomp
   python tools\ppu_lifter.py ..\fw\lle\libsre_image.bin --raw --base 0x30000000 --toc 0x30039AB0 --code-end 0x3001DB18 --symbol-prefix libsre_ --functions ..\fw\lle\libsre_functions.json --hle-stubs ..\fw\lle\libsre_hle_stubs.json -o ..\fw\lle\recomp
   cd ..
   ```
4. Copie para `port\lle_libsre\` (crie a pasta):
   - `fw\lle\recomp\ppu_recomp_000.cpp`, renomeado para `libsre_recomp.cpp`;
   - `fw\lle\recomp\ppu_recomp.h`;
   - `fw\lle\libsre_exports.c`.

   O `fw\lle\libsre_image.bin` também é lido pelo jogo ao rodar.

## 3. O código da PPU (processador central)

```
python ps3recomp\tools\ppu_loader.py jogo\GOWA.elf -o out
scripts\rodar_lifter_v2.cmd
python scripts\ppu_preambulo.py
python scripts\ppu_restrict.py aplicar
python scripts\remendos_ppu.py aplicar
python scripts\sonda.py por func_001C05A4 loc_0039C710 loc_00415914 func_00415120
python scripts\gerar_imports_inc.py
```

- **`rodar_lifter_v2.cmd`**: traduz o `GOWA.elf` para `port\recompiled\`: ~22 mil funções em 48 arquivos, ~185 MB, o log em `out\lifter_v2.log`.
- **`ppu_preambulo.py`**: põe no cabeçalho gerado o código de suporte do port.
- **`ppu_restrict.py`**: otimização.
- **`remendos_ppu.py`**: correções de comportamento do jogo, **obrigatórias**: sem elas o heap de texturas se corrompe.
- **`sonda.py`**: marca os pontos que os scripts de teste usam para saber em que tela o jogo está.

## 4. O código das SPUs (co-processadores)

Obrigatórios para compilar:

```
python ps3recomp\tools\build_spu_workloads.py --input jogo\GOWA.elf --input fw\libsre.prx --lifted port\spu --out port\spu\spu_workloads.c --constructor --allow-partial
python scripts\lift_gowa_jobmods.py
python scripts\lift_gowa_pm.py
python scripts\lift_gowa_tspm.py
python scripts\lift_gowa_syssrv.py
python scripts\lift_gowa_plugins.py dados\plugins_getcode_edge_cap16.txt
python scripts\lift_gowa_plugins.py dados\plugins_getcode_w11.txt --wids 11 --inc port\gowa_plugins_w11.inc --tabela g_gowa_plugin_fns_w11
python scripts\folhas_spu.py port\spu\gowajm_006FC080\spu_recomp.c gowajm_006FC080_ --inc 6FC080 port\gowa_jobmods_hot.inc
python scripts\guarda_avx512.py aplicar
```

- O `build_spu_workloads.py` roda com `port\spu\` vazio, a não ser pela pasta `gowanat_tspm`, que já vem no repositório.
- O `folhas_spu.py` só acrescenta as linhas da tabela; o cabeçalho `static const ... = {` e o `};` de `port\gowa_jobmods_hot.inc` ainda são escritos à mão.
- O `guarda_avx512.py` é obrigatório para a build sem AVX-512 e inofensivo na outra.

Passes de desempenho, opcionais e na ordem. Cada um regrava o código e deixa um backup `.sem_*`:

```
bash scripts/refaz_modulo.sh 72DF80
bash scripts/refaz_modulo.sh pm
bash scripts/refaz_jm.sh <VA dos módulos de job comuns>
```

Eles aplicam promoção de registradores, chamadas internas, constantes da memória local, saltos diretos e os núcleos vetoriais gerados (`vetorizar_laco.py`, `vetorizar_ninho.py`, `vetorizar_chamadas.py`).

## 5. Compilar

| CPU | Comando | Pasta |
|---|---|---|
| Com AVX-512 (Ryzen 7000/9000, Intel 11ª geração desktop) | `scripts\build_portatil.cmd znver5` (AMD) ou `scripts\build_portatil.cmd rocketlake` (Intel) | `port\build_znver5` / `port\build_rocketlake` |
| Sem AVX-512 (qualquer CPU com AVX2: Intel Haswell+, AMD Zen+) | `scripts\build_portatil.cmd x86-64-v3 rz` | `port\build_v3rz` |

- Se o FFmpeg não estiver em `dl\ffmpeg9\ffmpeg-n9.0-latest-win64-lgpl-shared-9.0`, defina antes `set FFMPEG_DIR=<pasta do FFmpeg>`.
- Copie as DLLs do FFmpeg (`avcodec-63`, `avutil-61`, `swresample-7`, `swscale-10`) para a pasta do `gowa.exe`.
- **PGO, opcional, +~4%**: compile com `-DGOWA_PGO=gen`, rode o jogo ~7 minutos com `LLVM_PROFILE_FILE=<pasta>\gowa-%p.profraw` e `GOWA_PGO_DUMP_S=400`, junte os perfis com `llvm-profdata merge -o port\pgo\gowa.profdata` e compile de novo. O `build_portatil.cmd` usa o perfil sozinho quando ele existe.

## 6. Rodar

Com a pasta de trabalho na pasta do exe:

```
set PS3_VFS_ROOT=C:\gow-ascension-pc\vfs
gowa.exe C:\gow-ascension-pc\jogo\GOWA.elf > ..\..\out\run.log 2>&1
```

| Variável | Padrão | Para quê |
|---|---|---|
| `PS3_VFS_ROOT` | (obrigatória) | pasta com `PS3_GAME\` e `game\BCUS98232\` |
| `PS3_LIBSRE_IMAGE` | `fw\lle\libsre_image.bin` da raiz | imagem da libsre |
| `PS3_SAVE_ROOT` | `gamedata\dev_hdd0\home\00000001\savedata`, na pasta de trabalho | saves |
| `GOWA_FLIP_HZ` | 60 | limitador; 0 = sem limite (o jogo acelera) |
| `GOWA_RES_SCALE` | 1 | 1 = 720p … 3 = 4K |
| `PS3_WINDOW_MONITOR` | — | outro valor que não 0/1 abre a janela no monitor secundário |
| `GOWA_AUDIO_MUTE` | — | `1` = sem som |

- O cache de shaders e pipelines fica em `scratch\`, na pasta de trabalho.
- O log é a saída do console; redirecione para um arquivo.

## O que ainda não virou script

Até esses itens virarem passes automáticos, quem gera do zero tem uma versão que roda, mas um pouco mais lenta que a nossa:

- No módulo de job 0x72DF80 (o EDGE de geometria):
  - o gancho `/*LUZGPU*/`, que leva a luz por vértice para a GPU; sem ele, ~−10% no cap. 16;
  - a entrada extra 0x7030.
- No gerenciador de políticas (PM) 0x79E500: as entradas 0x3368, 0xA2C e 0xA68.
- Os ganchos de diagnóstico `GOWA_PULA_LUZ` e `GOWA_SLOW_JOBMOD`, sem efeito no jogo.
- A configuração exata do nosso build principal (`-march=znver5` com PGO) está na tabela do passo 5, mas os ganhos dos passes do passo 4 dependem da ordem e de backups antigos.
