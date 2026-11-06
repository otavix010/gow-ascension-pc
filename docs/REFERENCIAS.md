# Otavix Engine PS3 — origem, licenças e referências

A engine PS3 da Otavix é trabalho autoral. Este documento diz de onde ela partiu e o que serviu de referência para construí-la. A ideia é ser transparente com quem joga e com quem fez o caminho antes de nós.

## 1. Base e componentes de terceiros (avisos exigidos por licença)

| Componente | Autor | Licença | Como entra |
|---|---|---|---|
| **ps3recomp** (tradutor PPU/SPU, runtime, bibliotecas de sistema), versão 0.15.0 importada no início do projeto | Ned Heller ([@sp00nznet](https://github.com/sp00nznet)) e colaboradores (ver `ps3recomp/CONTRIBUTORS.md`) | MIT | Base do runtime e do tradutor. O aviso MIT original (`ps3recomp/LICENSE`) acompanha a engine. |
| Contribuições de Caner Saka ([@canersaka](https://github.com/canersaka)), vindas do projeto Yakuza: Dead Souls EX e incorporadas ao ps3recomp | Caner Saka | MIT (contribuição ao ps3recomp) | Parte do desenho ao vivo do RSX, do passo a passo das SPUs e dos canais SPU. |
| **at3_standalone** (decodificador ATRAC3/ATRAC3plus) | Equipe do FFmpeg e do PPSSPP | LGPL 2.1 | Biblioteca separada e substituível. |
| **FFmpeg** (vídeo) | Projeto FFmpeg | LGPL (build `lgpl-shared`) | DLLs separadas, carregadas em tempo de execução. |
| stb_image, stb_image_write, stb_truetype | Sean Barrett | Domínio público / MIT | Embutidos. |

As ferramentas de extração (`scripts/gpl/unself.py`, `pup_libsre.py`) são GPL v2 e ficam **fora** da engine. São programas separados e abertos, que o gerador executa à parte.

## 2. Referências de comportamento (sem código copiado)

Estas fontes ajudaram a entender **como o PS3 se comporta**: estruturas, offsets, semântica de instruções e protocolos. A implementação da engine é nossa. Fatos medidos e documentados (tamanhos, offsets, resultados de instruções) não são código.

- **RPCS3** ([rpcs3.net](https://rpcs3.net), GPL v2). Foi referência de comportamento para o SPURS (layout da instância e dos tasksets), a semântica das instruções SPU/PPU, o RSX (programas de fragmento e de vértice), o formato NPDRM e o comportamento das bibliotecas de sistema. Também foi usado como oráculo em bancadas de teste, que ficam fora da engine. Nosso respeito e agradecimento à equipe do RPCS3: sem o trabalho deles, entender o PS3 levaria anos a mais.
- **nouveau / envytools**: documentação do NV40 (RSX), opcodes dos programas de fragmento e de vértice.
- **Cell Broadband Engine Architecture** e **SPU ISA** (IBM/Sony/Toshiba): a especificação do processador. As tabelas de estimativa `frest`/`frsqest` são os valores que a instrução produz no hardware.
- **PS3 Developer Wiki** ([psdevwiki.com](https://www.psdevwiki.com)) e a comunidade de homebrew do PS3: formatos SELF/PUP/PKG/EDAT e chamadas de sistema (lv2).
- **PowerISA** (IBM): a semântica das instruções PPU e VMX.
- **psnr** ([sp00nznet/psnr](https://github.com/sp00nznet/psnr), MIT), de Ned Heller: o protocolo do servidor de placares que o cliente do runtime (`libs/network/psnr`) fala.
- **FIPS 180-1** (SHA-1): o cálculo dos NIDs das tabelas de importação e exportação.

## 3. O que é autoral

Tudo o que não está na seção 1, entre outras partes:
- os passes de desempenho do código SPU (promoção de registradores, encadeamento, desvio indireto local, fusão de laços, vetorização);
- o EDGE e o General6 nativos;
- o SPURS em código próprio (fase 2);
- o desenho ao vivo do RSX e o tradutor de shaders para D3D12, na forma atual;
- a ferramenta de conferência de jobs;
- o gerador com marcas de MD5;
- as correções de coerência e de reservas das SPUs.
