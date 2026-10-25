# Como a parte do jogo é gerada

O `gerar.cmd` (ou `gerar.ps1`) faz tudo sozinho; este documento explica cada etapa, para quem quiser entender, depurar ou fazer à mão. Os logs ficam em `out\gerar\<etapa>.log`. Uma etapa concluída deixa `out\gerar\<etapa>.ok` e é pulada na próxima vez. Para refazer, apague o `.ok` ou rode com `-Refazer`.

Só serve para **God of War: Ascension BCUS98232 (EUA) + update 1.04**: os endereços usados pelos scripts são dessa versão, e a etapa 2 confere o SHA-256 do executável.

## Parâmetros do `gerar.ps1`

| Parâmetro | O quê |
|---|---|
| `-Disco` | pasta que contém `PS3_GAME` (cópia do disco) |
| `-Update` | pasta `BCUS98232` do update instalado no RPCS3 (`RPCS3\dev_hdd0\game\BCUS98232`) ou o arquivo `.pkg` |
| `-Firmware` | pasta do RPCS3 com o firmware instalado, ou o `libsre.sprx` |
| `-ChavesRpcs3` | pasta com `key_vault.cpp` e `key_vault.h` do código do RPCS3 (padrão: baixa para `dl\rpcs3_chaves`) |
| `-Build` | `auto` (padrão), `avx512` ou `portatil` |
| `-Jobs` | núcleos usados (padrão: todos) |
| `-Refazer` | refaz todas as etapas |

Os três primeiros, se faltarem, são pedidos em janelas.

## Etapas

**1. Ferramentas.** Confere o Python (≥ 3.10) e o Visual Studio 2022 com "C++ Clang tools" e "C++ CMake tools" (acha a instalação com o `vswhere`). Instala o `pycryptodome` com o `pip` se faltar e baixa o FFmpeg 9 LGPL (builds do BtbN) para `dl\ffmpeg9\`.

**2. Jogo.**
- Cria `vfs\PS3_GAME` como *junction* para o `PS3_GAME` do disco, sem copiar nada.
- Copia o update para `vfs\game\BCUS98232`. Se for `.pkg`, extrai com `ps3recomp\tools\pkg_extract.py`.
- Copia `EBOOT.BIN` e `GOWA.SELF` para `jogo\` e decripta com `scripts\gpl\unself.py`:
  - o `EBOOT.BIN` primeiro;
  - depois o `GOWA.SELF`, com a KLIC procurada dentro do `EBOOT.elf` (`--klic-de`).
- Confere o SHA-256 do `jogo\GOWA.elf`: `813723ac4e091d79ea20ad2500b2e86cb0fd25f3736541431921273e7a77246d`.

**3. Firmware (libsre).** Decripta o `libsre.sprx` para `fw\libsre.prx` e o traduz:
- `lift_prx.py`, `prx_exports.py` e `ppu_lifter.py --raw` do `ps3recomp`, com `scripts\libsre_stubs_json.py` entre eles;
- copia o resultado para `port\lle_libsre\`.

O `fw\lle\libsre_image.bin` também é lido pelo jogo ao rodar.

**4. PPU.**
- `ppu_loader.py`, depois `ppu_lifter.py --code-end 0x661C14 --chunks 48`, que gera `port\recompiled\`: ~22 mil funções em 48 arquivos, ~185 MB.
- `gen_hle_nids.py --all`.
- Em seguida, os scripts do port:
  - `ppu_preambulo.py`: o código de suporte do cabeçalho;
  - `ppu_restrict.py`: otimização;
  - `remendos_ppu.py`: correções obrigatórias; sem elas o heap de texturas se corrompe;
  - `sonda.py`: marcas para os scripts de teste;
  - `gerar_imports_inc.py`.

**5. SPU.**
- `build_spu_workloads.py` gera em `out\spu_workloads\`; o conteúdo é copiado para `port\spu\`.
- Os tradutores do port:
  - `lift_gowa_jobmods.py`: 31 módulos de job;
  - `lift_gowa_pm.py`: o gerenciador de políticas;
  - `lift_gowa_tspm.py` e `lift_gowa_syssrv.py`: partes da libsre;
  - `lift_gowa_plugins.py`, com as listas de endereços de `dados\`.
- A tabela de funções folha (`folhas_spu.py`).
- `spu_num_remap.py`, **obrigatório**: o boot liga `GOWA_SPU_NUM_REMAP=3`, e a 7ª SPU se apresenta com o número de outra nas listas de job. Sem esse passe os números colidem e o jogo para no boot, esperando um job que nunca roda.
- `guarda_avx512.py`: os núcleos vetoriais compilam em AVX-512 ou AVX2.

**6. Compilar.** Pergunta ao clang se a CPU tem AVX-512 (F/BW/VL/DQ) e chama `scripts\build_portatil.cmd`:
- **com AVX-512**: `native`, em `port\build_native\`;
- **sem AVX-512**: `native rz`, com as contas de float das SPUs em arredondamento para zero, em `port\build_nativerz\`.

O código das SPUs sai com `-march=native` (a CPU de quem compila); o da PPU, com `x86-64-v3`. O PGO só entra se existir `port\pgo\gowa.profdata`.

**7. Jogar.** Copia as DLLs do FFmpeg para a pasta do exe e escreve o `jogar.cmd`.

## O que a geração automática ainda não faz

A nossa build de referência teve, por cima da tradução, passes de desempenho aplicados ao longo do desenvolvimento. Eles estão nos scripts, mas ainda não entram no `gerar.ps1`, porque dependem de ordem, de backups intermediários e de alguns ajustes à mão:

- promoção de registradores, saltos diretos e constantes da memória local, nos módulos SPU: `refaz_jm.sh` e `refaz_modulo.sh`;
- núcleos vetoriais dos laços mais pesados (`vetorizar_laco.py`, `vetorizar_ninho.py`, `vetorizar_chamadas.py`);
- no EDGE (módulo 0x72DF80): o gancho que leva a luz por vértice para a GPU (`/*LUZGPU*/`) e uma entrada extra;
- no gerenciador de políticas: três entradas extras;
- PGO (perfil de execução).

Por isso a build gerada é mais lenta que os números do README. A diferença medida está no README. Automatizar esses passes é a próxima etapa.

## Verificação

Na primeira versão, a geração do zero num clone limpo produziu:
- código da PPU idêntico ao da nossa build, em 50 de 51 arquivos; o diferente é a tabela de NIDs, que ganhou duas funções internas;
- libsre idêntica;
- tabelas das SPUs iguais, uma só com outra ordem;
- `GOWA.elf`, `EBOOT.elf` e `libsre.prx` idênticos byte a byte aos que usamos.
