"""Liga as partes nativas do EDGE (port/edge/*.h) no código SPU traduzido (port/spu/...).

O código traduzido é gerado a cada geração, então os ganchos são aplicados depois dela. Dois tipos:
- 'entrada': na entrada da função, `if (<nativo>(ctx)) { SPU_RET(ctx); }`;
- 'entrada_segue': na entrada da função, `if (<nativo>(ctx)) { RELOAD; }` (o nativo faz parte do laço, ajusta os
  registradores em ctx->gpr e o traduzido segue com eles);
- 'antes': logo antes de um rótulo (roda uma vez, antes do laço), `if (<nativo>(ctx)) goto <destino>;`;
- 'depois_rotulo': logo depois de um rótulo, `if (<nativo>(ctx)) {}` (o nativo avança e o traduzido segue);
- 'multi': logo depois de um rótulo (pos='antes': logo antes; vk=...: antes do núcleo vk), o nativo devolve 0
  (segue o traduzido) ou k e salta para destinos[k-1].
O nativo devolve 0 quando o caso não é coberto, e o código traduzido segue como antes.

uso: python -I edge_nativo.py aplicar|tirar [raiz do port]"""
import os
import sys

GANCHOS = [
    # plugin 0x74C580 (empacotamento dos vértices); a ponte dos plugins relocáveis (spu_interp.c) acha o
    # 0x30020 pelo conteúdo e entra direto nele, então as duas entradas
    dict(tipo='entrada', pasta='gowapl_0074C580', fn='gowapl_0074C580_spu_func_00030000', cab='pl_0074C580.h', nat='pl_0074C580_nativo'),
    dict(tipo='entrada', pasta='gowapl_0074C580', fn='gowapl_0074C580_spu_func_00030020', cab='pl_0074C580.h', nat='pl_0074C580_nativo'),
    # plugin 0x752710 (descompressão de posição e vetores): a partir da entrada do laço, depois da preparação
    dict(tipo='entrada', pasta='gowapl_00752710', fn='gowapl_00752710_spu_func_000302E8', cab='pl_00752710.h', nat='pl_00752710_nativo'),
    # plugin 0x755540 (posições comprimidas em u16 -> float4): o laço inteiro, na entrada da função dele
    dict(tipo='entrada', pasta='gowapl_00755540', fn='gowapl_00755540_spu_func_00030310', cab='pl_00755540.h', nat='pl_00755540_nativo'),
    # plugin 0x74EF90 (posição u16 + 2 vetores comprimidos, os dois anteriores juntos): o laço inteiro e o epílogo
    dict(tipo='entrada', pasta='gowapl_0074EF90', fn='gowapl_0074EF90_spu_func_000304A0', cab='pl_0074EF90.h', nat='pl_0074EF90_nativo'),
    # plugin 0x75C570 (float4 -> float3 empacotado): as voltas 0..n-2; a última fica com o laço traduzido
    dict(tipo='entrada_segue', pasta='gowapl_0075C570', fn='gowapl_0075C570_spu_func_000300D0', cab='pl_0075C570.h', nat='pl_0075C570_nativo'),
    # D640 do job 0x72DF80: o laço de descarte de triângulos
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000D640__b', cab='fn_0000D640.h',
         nat='fn_0000D640_laco_nativo', rotulo='loc_0000D92C', destino='loc_0000DB70'),
    # E550 (skinning): o laço EC08 menos a última volta; entra antes do núcleo vk dele e salta para o laço
    # 02/11 (E4): devolve 2 quando fez também o último par -> depois do laço (loc_0000EDE8)
    dict(tipo='multi', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000E550__b', cab='fn_0000E550_ec08.h',
         nat='fn_0000E550_ec08_nativo', vk='/*VK 0000E550/0000EC08*/', rotulo='loc_0000EC08',
         destinos=['loc_0000EC08', 'loc_0000EDE8']),
    # DCC0/E3C4 (reconstrução dos triângulos): o laço inteiro em SIMD com as tabelas do jogo (o escalar perdia do laço
    # local por um desvio imprevisível por triângulo); saltando para depois do laço
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_e3c4.h',
         nat='fn_0000DCC0_e3c4_nativo', rotulo='loc_0000E3C4', destino='loc_0000E3C4_fim', fim='loc_0000E3C4'),
    # DCC0/E210 (índices por máscara: explícito do fluxo ou o próximo sequencial): o laço inteiro em SIMD por tabelas
    # de pshufb; antes do vk dele, saltando para depois do laço
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_e210.h',
         nat='fn_0000DCC0_e210_nativo', vk='/*VK 0000DCC0/0000E210*/', destino='loc_0000E210_fim', fim='loc_0000E210'),
    # CC98/CE60 (a faixa da LS sobe 4 bytes, em blocos de 128 de cima para baixo): um memmove
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000CC98__b', cab='fn_0000CC98_ce60.h',
         nat='fn_0000CC98_ce60_nativo', rotulo='loc_0000CE60', destino='loc_0000CE60_fim'),   # o rótulo do fim vem do laço local
    # D250 (projeção dos vértices, viewport e códigos de recorte): o laço D4DC inteiro, antes do vk dele
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000D250__b', cab='fn_0000D250.h',
         nat='fn_0000D250_nativo', vk='/*VK 0000D250/0000D4DC*/', destino='loc_0000D4DC_fim', fim='loc_0000D4DC'),
    # F538 (transformação dos vértices pela matriz): o laço F620 inteiro; antes do vk dele, saltando para depois do laço
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000F538__b', cab='fn_0000F538.h',
         nat='fn_0000F538_nativo', vk='/*VK 0000F538/0000F620*/', destino='loc_0000F620_fim', fim='loc_0000F620'),
    # A880 (normais: produto vetorial e normalização): o laço A950 inteiro, só na build portátil (AVX2 em SoA; em
    # AVX-512 o AoS empatava com o vk, que fica)
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000A880__b', cab='fn_0000A880.h',
         nat='fn_0000A880_nativo', vk='/*VK 0000A880/0000A950*/', destino='loc_0000A950_fim', fim='loc_0000A950'),
    # 8930/8B90 (compactação dos registros de 8 bytes): todas as entradas (02/11: salta para 8D50 com r8/r9; antes ficava a última entrada com o laço)
    dict(tipo='multi', pos='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_00008930__b', cab='fn_00008930.h',
         nat='fn_00008930_nativo', rotulo='loc_00008B90', destinos=['loc_00008B90', 'loc_00008D50']),
    # B1A8 (E4, 01/11): a função inteira -- um comando de carga de constante do programa de vértices (24 bytes) no
    # buffer de comandos da LS; buffer cheio (descarrega por DMA) fica com o traduzido
    dict(tipo='entrada', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000B1A8__b', cab='fn_0000B1A8.h',
         nat='fn_0000B1A8_nativo'),
    # B2D0 e B3D0 (E4, 01/11): as funções inteiras -- comandos de 12 e 60 bytes (recorte e viewport) no buffer da LS
    dict(tipo='entrada', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000B2D0__b', cab='fn_0000B2D0.h',
         nat='fn_0000B2D0_nativo'),
    dict(tipo='entrada', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000B3D0__b', cab='fn_0000B3D0.h',
         nat='fn_0000B3D0_nativo'),
    # BFB8 (E4, 01/11): a função inteira -- monta a estrutura de estado do lote a partir do descritor
    dict(tipo='entrada', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000BFB8__b', cab='fn_0000BFB8.h',
         nat='fn_0000BFB8_nativo'),
    # BBC8 (E4, 02/11): a função inteira -- normaliza no lugar os vetores de um fluxo (xyz * fi(frsqest(L)), w * 1,0),
    # 8 por volta; volta com valor fora do alcance do float do PC refeita pelas primitivas do runtime
    dict(tipo='entrada', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000BBC8__b', cab='fn_0000BBC8.h',
         nat='fn_0000BBC8_nativo'),
    # 8930/8988 (E4, 01/11): a primeira compactação dos registros de 8 bytes, as voltas comuns até antes da última
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_00008930__b', cab='fn_00008930_8988.h',
         nat='fn_00008930_8988_nativo', rotulo='loc_00008988', destino='loc_00008988'),
    # DCC0/E134 (E4, 01/11): decodificação delta em 8 faixas de 16 bits, no lugar, em SSE; salta para o fim do laço
    # (o rótulo do fim vem do laço local)
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_e134.h',
         nat='fn_0000DCC0_e134_nativo', rotulo='loc_0000E134', destino='loc_0000E134_fim'),
    # DCC0/E30C (E4, 01/11): cópia desalinhada de 16 em 16 bytes (memmove para frente); o rótulo do fim vem do laço local
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_e30c.h',
         nat='fn_0000DCC0_e30c_nativo', rotulo='loc_0000E30C', destino='loc_0000E30C_fim'),
    # CC98/CDBC (E4, 02/11): a varredura das entradas de tamanho variável, as voltas que continuam (logo depois do
    # rótulo: pega a entrada e as voltas que vêm da correção)
    dict(tipo='depois_rotulo', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000CC98__b', cab='fn_0000CC98_cdbc.h',
         nat='fn_0000CC98_cdbc_nativo', rotulo='loc_0000CDBC'),
    # plugin 0x75B710 (E4, 02/11): posições float3 de pares de 36 bytes -> float4 com w = 1,0; o laço 0x30154 inteiro
    dict(tipo='entrada', pasta='gowapl_0075B710', fn='gowapl_0075B710_spu_func_00030154', cab='pl_0075B710.h', nat='pl_0075B710_nativo'),
    # 9090 (E4, 02/11): os dois laços internos que enchem o buffer com palavras 0xFF000000 | endereço (9620: 2047 por
    # chamada); os rótulos do fim vêm dos laços locais
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_00009090__b', cab='fn_00009090_lacos.h',
         nat='fn_00009090_9620_nativo', rotulo='loc_00009620', destino='loc_00009620_fim'),
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_00009090__b', cab='fn_00009090_lacos.h',
         nat='fn_00009090_96e0_nativo', rotulo='loc_000096E0', destino='loc_000096E0_fim'),
    # CC98/CCD0 (E4, 02/11): os registros de 16 bytes {C0, w & 0xFF0, C2, palavra} das entradas não nulas, no topo CD1C
    dict(tipo='depois_rotulo', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000CC98__b', cab='fn_0000CC98_ccd0.h',
         nat='fn_0000CC98_ccd0_nativo', rotulo='loc_0000CD1C'),
    # 73E0/83E8 (E4, 02/11): o w do fluxo A sobre o fluxo B, no lugar, 128 bytes por volta; o rótulo do fim vem do laço local
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_000073E0__b', cab='fn_000073E0_83e8.h',
         nat='fn_000073E0_83e8_nativo', rotulo='loc_000083E8', destino='loc_000083E8_fim'),
    # 73E0/75D8, 7670, 7828 (E4, 02/11): marcas na tabela pelos índices u16, tabela de deslocamentos pela soma corrente
    # e a palavra 3 trocada pelo contador; as voltas 0..n-2 nativas, a última fica com o traduzido (salta ao rótulo)
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_000073E0__b', cab='fn_000073E0_lacos.h',
         nat='fn_000073E0_75d8_nativo', rotulo='loc_000075D8', destino='loc_000075D8'),
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_000073E0__b', cab='fn_000073E0_lacos.h',
         nat='fn_000073E0_7670_nativo', rotulo='loc_00007670', destino='loc_00007670'),
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_000073E0__b', cab='fn_000073E0_lacos.h',
         nat='fn_000073E0_7828_nativo', rotulo='loc_00007828', destino='loc_00007828'),
    # DCC0/DE6C (E4, 02/11): índices empacotados com largura <= 8 bits desempacotados em u16 no lugar, do último bloco
    # para o primeiro; o laço inteiro, saltando para loc_0000E0E4 (só o r10 do laço é lido depois)
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_de6c.h',
         nat='fn_0000DCC0_de6c_nativo', rotulo='loc_0000DE6C', destino='loc_0000E0E4'),
    # DCC0/DFF0 (E4, 02/11): o mesmo desempacotamento para largura 9..16; antes do vk do laço, saltando para E0E4
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_dff0.h',
         nat='fn_0000DCC0_dff0_nativo', vk='/*VK 0000DCC0/0000DFF0*/', destino='loc_0000E0E4'),
    # E550/F104 (E4, 02/11): normal e tangente pela matriz 3x3 de cada vértice (r66 = 2); o laço inteiro, antes do vk,
    # saltando para loc_0000F3FC
    dict(tipo='multi', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000E550__b', cab='fn_0000E550_f104.h',
         nat='fn_0000E550_f104_nativo', vk='/*VK 0000E550/0000F104*/', rotulo='loc_0000F104', destinos=['loc_0000F3FC']),
    # plugins dos atributos do vértice (E4, 02/11): inteiros -- os registros SET_VERTEX_DATA_ARRAY_FORMAT/OFFSET no buffer
    # de comandos (pl_atributos.h): 0x756880 (vértice de 54 bytes), 0x75CBC0 (12), 0x757DD0 (46)
    dict(tipo='entrada', pasta='gowapl_00756880', fn='gowapl_00756880_spu_func_0003000C', cab='pl_atributos.h',
         nat='pl_00756880_nativo'),
    dict(tipo='entrada', pasta='gowapl_0075CBC0', fn='gowapl_0075CBC0_spu_func_0003000C', cab='pl_atributos.h',
         nat='pl_0075CBC0_nativo'),
    dict(tipo='entrada', pasta='gowapl_00757DD0', fn='gowapl_00757DD0_spu_func_0003000C', cab='pl_atributos.h',
         nat='pl_00757DD0_nativo'),
    dict(tipo='entrada', pasta='gowapl_007538C0', fn='gowapl_007538C0_spu_func_00030000', cab='pl_atributos.h', nat='pl_007538C0_nativo'),
    dict(tipo='entrada', pasta='gowapl_00756C40', fn='gowapl_00756C40_spu_func_00030000', cab='pl_atributos.h', nat='pl_00756C40_nativo'),
    dict(tipo='entrada', pasta='gowapl_0075B9B0', fn='gowapl_0075B9B0_spu_func_00030000', cab='pl_atributos.h', nat='pl_0075B9B0_nativo'),
    dict(tipo='entrada', pasta='gowapl_0075BB70', fn='gowapl_0075BB70_spu_func_00030000', cab='pl_atributos.h', nat='pl_0075BB70_nativo'),
    dict(tipo='entrada', pasta='gowapl_0075CA80', fn='gowapl_0075CA80_spu_func_00030000', cab='pl_atributos.h', nat='pl_0075CA80_nativo'),
    # plugin 0x757930 (E4, 02/11): o laço 0x30230 até o fim (o 0x752710 com 1 vetor e vértice de 15 bytes)
    dict(tipo='entrada', pasta='gowapl_00757930', fn='gowapl_00757930_spu_func_00030230', cab='pl_00757930.h',
         nat='pl_00757930_nativo'),
    # plugin 0x751450 (E4, 02/11): o laço 0x303D8 até o fim (posição em campos de bits + 1 vetor smallest-three, 9 bytes)
    dict(tipo='entrada', pasta='gowapl_00751450', fn='gowapl_00751450_spu_func_000303D8', cab='pl_00751450.h',
         nat='pl_00751450_nativo'),
    # plugin 0x755A20 (E4, 02/11): o laço 0x302B8 até o fim (posição + 1 vetor smallest-three + cor RGBA u8, 19 bytes)
    dict(tipo='entrada', pasta='gowapl_00755A20', fn='gowapl_00755A20_spu_func_000302B8', cab='pl_00755A20.h',
         nat='pl_00755A20_nativo'),
    # plugin 0x74D7E0 (E4, 02/11): inteiro -- o empacotador do vértice de 46 bytes (o 0x74C580 sem o atributo 3)
    dict(tipo='entrada', pasta='gowapl_0074D7E0', fn='gowapl_0074D7E0_spu_func_00030000', cab='pl_0074D7E0.h', nat='pl_0074D7E0_nativo'),
    dict(tipo='entrada', pasta='gowapl_0074D7E0', fn='gowapl_0074D7E0_spu_func_00030008', cab='pl_0074D7E0.h', nat='pl_0074D7E0_nativo'),
    # plugin 0x74CEC0 (E4, 02/11): inteiro -- o empacotador do vértice de 50 bytes (o 0x74D7E0 com a cor u8 do atributo 20)
    dict(tipo='entrada', pasta='gowapl_0074CEC0', fn='gowapl_0074CEC0_spu_func_00030000', cab='pl_0074CEC0.h', nat='pl_0074CEC0_nativo'),
    dict(tipo='entrada', pasta='gowapl_0074CEC0', fn='gowapl_0074CEC0_spu_func_00030018', cab='pl_0074CEC0.h', nat='pl_0074CEC0_nativo'),
    # plugin 0x754B80 (E4, 02/11): o laço 0x30310 -- o do 0x755540 com vértice de 9 bytes (pl_00755540.h, pl755_laco)
    dict(tipo='entrada', pasta='gowapl_00754B80', fn='gowapl_00754B80_spu_func_00030310', cab='pl_00755540.h', nat='pl_00754B80_nativo'),
    # plugin 0x75AF20 (E4, 02/11): o laço 0x30160 -- float3 do vértice de 15 bytes + w constante
    dict(tipo='entrada', pasta='gowapl_0075AF20', fn='gowapl_0075AF20_spu_func_00030160', cab='pl_0075AF20.h', nat='pl_0075AF20_nativo'),
    # plugin 0x75B470 (E4, 02/11): o laço 0x30154 -- o copiador do 0x75AF20 com vértice de 22 bytes (não roda nas
    # gravações: conferido na bancada sintética, laço traduzido x nativo sobre as mesmas LS)
    dict(tipo='entrada', pasta='gowapl_0075B470', fn='gowapl_0075B470_spu_func_00030154', cab='pl_0075AF20.h', nat='pl_0075B470_nativo'),
    # plugin 0x75A380 (E4, 02/11): atributos do vértice (pl_atributos.h), gancho na 0x30064 (não roda nas gravações:
    # conferido na bancada sintética)
    dict(tipo='entrada', pasta='gowapl_0075A380', fn='gowapl_0075A380_spu_func_00030064', cab='pl_atributos.h', nat='pl_0075A380_nativo'),
    # plugin 0x75AA40 (E4, 02/11): inteiro -- vértice de 16 bytes (posição + 11:11:10); não roda nas gravações:
    # conferido na bancada sintética (as duas builds)
    dict(tipo='entrada', pasta='gowapl_0075AA40', fn='gowapl_0075AA40_spu_func_00030000', cab='pl_0075AA40.h', nat='pl_0075AA40_nativo'),
    # plugin 0x758B20 (E4, 02/11): inteiro -- vértice de 28 bytes (posição + 11:11:10 + atributo 31); não roda nas
    # gravações: conferido na bancada sintética (as duas builds)
    dict(tipo='entrada', pasta='gowapl_00758B20', fn='gowapl_00758B20_spu_func_00030000', cab='pl_00758B20.h', nat='pl_00758B20_nativo'),
    # plugin 0x758E70 (E4, 02/11): inteiro -- vértice de 24 bytes (posição + int16 x 4 + 11:11:10); não roda nas
    # gravações: conferido na bancada sintética (as duas builds)
    dict(tipo='entrada', pasta='gowapl_00758E70', fn='gowapl_00758E70_spu_func_00030000', cab='pl_00758E70.h', nat='pl_00758E70_nativo'),
    # plugin 0x74BA80 (E4, 02/11): inteiro -- vértice de 58 bytes (o 0x74C580 com a cor); não roda nas gravações:
    # conferido na bancada sintética (as duas builds)
    dict(tipo='entrada', pasta='gowapl_0074BA80', fn='gowapl_0074BA80_spu_func_00030000', cab='pl_0074BA80.h', nat='pl_0074BA80_nativo'),
    # plugin 0x755F80 (E4, 02/11): inteiro -- vértice de 36 bytes (posição + int16 x 4 + 11:11:10 + atributo 31); na
    # 0x30000 e na 0x30030 (onde o job chama direto, conferindo o que o 0x30000 prepararia); bancada sintética
    # DESLIGADO 02/11 (diverge num job real do chefe da Heca900, confere_h975 j0165, com a conta igual nas 58 chamadas
    # reais: mecanismo não achado; volta ao traduzido até entender)
    #dict(tipo='entrada', pasta='gowapl_00755F80', fn='gowapl_00755F80_spu_func_00030000', cab='pl_00755F80.h', nat='pl_00755F80_nativo'),
    #dict(tipo='entrada', pasta='gowapl_00755F80', fn='gowapl_00755F80_spu_func_00030030', cab='pl_00755F80.h', nat='pl_00755F80_n30'),
    # plugin 0x751B20 (E4, 02/11): o laço 0x30358 -- posição + 2 vetores smallest-three + cor (o 0x752710 com a cor do
    # 0x755A20); não roda nas gravações: conferido na bancada sintética (as duas builds)
    dict(tipo='entrada', pasta='gowapl_00751B20', fn='gowapl_00751B20_spu_func_00030358', cab='pl_00751B20.h', nat='pl_00751B20_nativo'),
    # plugins 0x755540 e 0x754B80 inteiros (E4, 02/11): na 0x30014, onde o job entra (a preparação + o laço; se recusar,
    # segue a preparação traduzida e o gancho do laço 0x30310)
    dict(tipo='entrada', pasta='gowapl_00755540', fn='gowapl_00755540_spu_func_00030014', cab='pl_00755540.h', nat='pl_00755540_inteiro'),
    dict(tipo='entrada', pasta='gowapl_00754B80', fn='gowapl_00754B80_spu_func_00030014', cab='pl_00755540.h', nat='pl_00754B80_inteiro'),
    # plugin 0x751450 inteiro (E4, 02/11): na 0x30014, onde o job entra (se recusar, segue o gancho do laço 0x303D8)
    dict(tipo='entrada', pasta='gowapl_00751450', fn='gowapl_00751450_spu_func_00030014', cab='pl_00751450.h', nat='pl_00751450_inteiro'),
    # plugin 0x752710 inteiro (E4, 02/11): na 0x30000 (se recusar, segue a preparação traduzida e o gancho do laço 0x302E8)
    dict(tipo='entrada', pasta='gowapl_00752710', fn='gowapl_00752710_spu_func_00030000', cab='pl_00752710.h', nat='pl_00752710_inteiro'),
    dict(tipo='entrada', pasta='gowapl_00752710', fn='gowapl_00752710_spu_func_00030020', cab='pl_00752710.h', nat='pl_00752710_n20'),
    # plugin 0x74EF90 inteiro (E4, 02/11): na 0x30090, onde o job chama direto (conferindo o que o 0x30000 deixaria; se
    # recusar, segue a preparação traduzida e o gancho do laço 0x304A0)
    dict(tipo='entrada', pasta='gowapl_0074EF90', fn='gowapl_0074EF90_spu_func_00030090', cab='pl_0074EF90.h', nat='pl_0074EF90_n90'),
    # copiadores inteiros (E4, 02/11): 0x75AF20 na 0x30000; 0x75B470 e 0x75B710 na 0x30000 e na 0x30018 (onde o job
    # chama direto); se recusarem, seguem a preparação traduzida e os ganchos dos laços
    dict(tipo='entrada', pasta='gowapl_0075AF20', fn='gowapl_0075AF20_spu_func_00030000', cab='pl_0075AF20.h', nat='pl_0075AF20_inteiro'),
    dict(tipo='entrada', pasta='gowapl_0075AF20', fn='gowapl_0075AF20_spu_func_00030010', cab='pl_0075AF20.h', nat='pl_0075AF20_n10'),
    dict(tipo='entrada', pasta='gowapl_0075B470', fn='gowapl_0075B470_spu_func_00030000', cab='pl_0075AF20.h', nat='pl_0075B470_inteiro'),
    dict(tipo='entrada', pasta='gowapl_0075B470', fn='gowapl_0075B470_spu_func_00030018', cab='pl_0075AF20.h', nat='pl_0075B470_n18'),
    dict(tipo='entrada', pasta='gowapl_0075B710', fn='gowapl_0075B710_spu_func_00030000', cab='pl_0075AF20.h', nat='pl_0075B710_inteiro'),
    dict(tipo='entrada', pasta='gowapl_0075B710', fn='gowapl_0075B710_spu_func_00030018', cab='pl_0075AF20.h', nat='pl_0075B710_n18'),
    # plugin 0x75C570 inteiro (E4, 02/11): na 0x30000 e na 0x30010 (onde o job chama direto); se recusar, segue a
    # preparação traduzida e o gancho do laço 0x300D0
    dict(tipo='entrada', pasta='gowapl_0075C570', fn='gowapl_0075C570_spu_func_00030000', cab='pl_0075C570.h', nat='pl_0075C570_inteiro'),
    dict(tipo='entrada', pasta='gowapl_0075C570', fn='gowapl_0075C570_spu_func_00030010', cab='pl_0075C570.h', nat='pl_0075C570_n10'),
    # plugin 0x757930 inteiro (E4, 02/11): na 0x30000 (se recusar, segue a preparação traduzida e o gancho do laço)
    dict(tipo='entrada', pasta='gowapl_00757930', fn='gowapl_00757930_spu_func_00030000', cab='pl_00757930.h', nat='pl_00757930_inteiro'),
    # plugin 0x755A20 inteiro (E4, 02/11): na 0x30000 (se recusar, segue a preparação traduzida e o gancho do laço)
    dict(tipo='entrada', pasta='gowapl_00755A20', fn='gowapl_00755A20_spu_func_00030000', cab='pl_00755A20.h', nat='pl_00755A20_inteiro'),
    # 9090/9458 (E4, 02/11): o desenho no buffer de comandos (0x1714, 0x181C endereço/formato do índice, 0x1808) e a
    # decisão seguinte (9888, 9958 = buffer cheio, ou 9570); vivos nas saídas por scripts_edge/vivos.py
    dict(tipo='multi', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_00009090__b', cab='fn_00009090_9458.h',
         nat='fn_00009090_9458_nativo', rotulo='loc_00009458', destinos=['loc_00009570', 'loc_00009888', 'loc_00009958']),
]
# laços de um bloco só com os registradores em variáveis locais (laco_local.py: o cabeçalho port/edge/ll_*.h é gerado
# aqui, do próprio módulo): o núcleo faz o laço inteiro e salta para <rótulo>_fim, depois do desvio; vem antes do vk
LACOS_LOCAIS = [
    # 30/10: os outros laços de um bloco (8 linhas ou mais) sem vk na função; as funções que começam no próprio rótulo
    # (entrada direta) também
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A950__b', 'loc_0000A950'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007040__b', 'loc_000071C8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000071C8__b', 'loc_000071C8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007BB4__b', 'loc_00007BB4'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000EC08__b', 'loc_0000EC08'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000F24C__b', 'loc_0000F24C'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000EE28__b', 'loc_0000EE28'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007ED8__b', 'loc_00007ED8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00006AC8__b', 'loc_00006DE0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000F620__b', 'loc_0000F620'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000BBC8__b', 'loc_0000BD54'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000BD54__b', 'loc_0000BD54'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000D4DC__b', 'loc_0000D4DC'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E3C4__b', 'loc_0000E3C4'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E210__b', 'loc_0000E210'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_000083E8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000083E8__b', 'loc_000083E8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000CC98__b', 'loc_0000CE60'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000CE60__b', 'loc_0000CE60'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000053B0__b', 'loc_00005720'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00005720__b', 'loc_00005720'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_00007670'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007670__b', 'loc_00007670'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_00007828'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007828__b', 'loc_00007828'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004A54__b', 'loc_00004CA0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004CA0__b', 'loc_00004CA0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000DCC0__b', 'loc_0000E134'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E134__b', 'loc_0000E134'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_000075D8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000075D8__b', 'loc_000075D8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E804__b', 'loc_0000E804'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009990__b', 'loc_00009A00'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009A00__b', 'loc_00009A00'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_00009B90'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009B90__b', 'loc_00009B90'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000DCC0__b', 'loc_0000E30C'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E30C__b', 'loc_0000E30C'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000AFF0__b', 'loc_0000B100'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000B100__b', 'loc_0000B100'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00005D90__b', 'loc_00005DA8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00005DA8__b', 'loc_00005DA8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009090__b', 'loc_000096E0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000096E0__b', 'loc_000096E0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004A54__b', 'loc_00004EB8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004EB8__b', 'loc_00004EB8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004F88__b', 'loc_000050B0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000050B0__b', 'loc_000050B0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009090__b', 'loc_00009620'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A490'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A5A0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A6C0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A7C8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A490__b', 'loc_0000A490'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A5A0__b', 'loc_0000A5A0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A6C0__b', 'loc_0000A6C0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A7C8__b', 'loc_0000A7C8'),
    # 30/10: nos laços que já têm vk, o vk ganha (A880/A950, D250/D4DC, F538/F620 pioram com o local; E210 e os do
    # E550 empatam): ficam sem o local
]
for _pasta, _fn, _rot in LACOS_LOCAIS:
    _id = _fn.split('_spu_func_')[1].replace('__b', '') + '_' + _rot[4:]
    GANCHOS.append(dict(tipo='laco_local', pasta=_pasta, fn=_fn, rotulo=_rot, cab=f'll_{_id}.h', nat=f'll_{_id}'))
MARCA = '/* GOWA_EDGE_NATIVO */'
INC = '#include "../../runtime/spu/spu_helpers.h"\n'


def arquivo(raiz, pasta):
    return os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c')


def corpo_da_funcao(s, fn):
    for cab in (f'static SPU_PN void {fn}(spu_context* ctx) {{\n', f'void {fn}(spu_context* ctx) {{\n'):
        if cab in s:
            i = s.index(cab) + len(cab)
            return i, s.index('\n}\n', i)
    raise ValueError('função não achada: ' + fn)


def chamada_direta(raiz):
    """01/11 (E4): nos pontos de chamada de uma função com nativo inteiro (gancho 'entrada' no módulo do job), o nativo
    é chamado direto e, se cobrir o caso, a chamada completa do runtime (host_depth, drain, restauração da imagem) é
    pulada. Linhas marcadas: 'tirar' as remove e a chamada original fica intacta."""
    import re
    nats = {g['fn']: g['nat'] for g in GANCHOS if g['tipo'] == 'entrada' and g['pasta'] == 'gowajm_0072DF80'}
    p = arquivo(raiz, 'gowajm_0072DF80')
    # tira as linhas desta passada de uma aplicação anterior (os rótulos são renumerados)
    linhas = [l for l in open(p, encoding='latin-1').read().split(chr(10)) if not (MARCA in l and 'nat_dir_' in l)]
    out, n = [], 0
    for i, l in enumerate(linhas):
        m = re.match(r'^(\s*)ctx->gpr\[0\] = spu_link\((0x[0-9A-F]+)\); \{ int32_t _si = \(int32_t\)ctx->image_id; ctx->host_depth\+\+; (gowajm_0072DF80_spu_func_[0-9A-F]{8}__b)\(ctx\); spu_drain_call', l)
        if m and m.group(3) in nats and MARCA not in l and not (out and out[-1].endswith(MARCA) and 'nat_dir_' in out[-1]):
            rot = f'nat_dir_{n}'
            out.append(f'{m.group(1)}ctx->gpr[0] = spu_link({m.group(2)}); if (GOWA_NAT({nats[m.group(3)]}, ctx)) goto {rot}; {MARCA}')
            out.append(l)
            out.append(f'{rot}: ; {MARCA}')
            n += 1
            continue
        out.append(l)
    open(p, 'w', encoding='latin-1', newline='').write(chr(10).join(out))
    print(f'chamada direta: {n} pontos')


def main():
    modo = sys.argv[1]
    raiz = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..')
    if modo == 'tirar':
        for pasta in sorted({g['pasta'] for g in GANCHOS}):
            p = arquivo(raiz, pasta)
            s = open(p, encoding='latin-1').read()
            n = s.count(MARCA)
            s = '\n'.join(l for l in s.split('\n') if MARCA not in l)
            open(p, 'w', encoding='latin-1', newline='').write(s)
            print(f'{pasta}: {n} linhas tiradas')
        return
    for g in GANCHOS:
        p = arquivo(raiz, g['pasta'])
        s = open(p, encoding='latin-1').read()
        if g['tipo'] == 'laco_local':
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import laco_local
            sys.argv = ['laco_local', p, g['fn'], g['rotulo'], os.path.join(raiz, 'port', 'edge', g['cab']), g['nat']]
            laco_local.main()
        # 02/11: GOWA_NAT (GOWA_EDGE_SEM desliga nativos pelo nome) vem do edge_sem.h, antes do cabeçalho do nativo
        linha_inc = f'#include "../../../port/edge/edge_sem.h" {MARCA}\n#include "../../../port/edge/{g["cab"]}" {MARCA}\n'
        if f'port/edge/{g["cab"]}"' not in s:
            if INC in s:
                s = s.replace(INC, INC + linha_inc, 1)
            else:   # o módulo inclui os helpers pelo spu_recomp.h
                inc2 = '#include "spu_recomp.h"\n'
                assert s.count(inc2) == 1, (p, 'include')
                s = s.replace(inc2, inc2 + linha_inc, 1)
        i, fim = corpo_da_funcao(s, g['fn'])
        corpo = s[i:fim]
        if g['tipo'] in ('entrada', 'entrada_segue'):
            j = corpo.index('RELOAD;\n') + len('RELOAD;\n') if 'RELOAD;\n' in corpo else 0
            if g['tipo'] == 'entrada':
                gancho = f'        if (GOWA_NAT({g["nat"]}, ctx)) {{ SPU_RET(ctx); }} {MARCA}\n'
            else:
                assert j, (g['fn'], 'sem RELOAD')
                gancho = f'        if (GOWA_NAT({g["nat"]}, ctx)) {{ RELOAD; }} {MARCA}\n'
            if corpo[j:j + len(gancho)] == gancho:
                print(f'{g["fn"]}: já aplicado'); continue
            corpo = corpo[:j] + gancho + corpo[j:]
        elif g['tipo'] == 'antes_vk':
            k = corpo.index(g['vk'])
            k = corpo.rindex('\n', 0, k) + 1
            gancho = f'        if (GOWA_NAT({g["nat"]}, ctx)) goto {g["destino"]}; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} antes do vk: já aplicado'); continue
            corpo = corpo[:k] + gancho + corpo[k:]
            if g.get('fim'):   # rótulo depois do desvio do laço, para o nativo que faz o laço inteiro
                d = corpo.index(f'goto {g["fim"]};', k + len(gancho))
                d = corpo.index('\n', d) + 1
                corpo = corpo[:d] + f'{g["fim"]}_fim: {MARCA}\n' + corpo[d:]
        elif g['tipo'] == 'depois_rotulo':   # 02/11 (E4): logo depois do rótulo; o nativo avança e o traduzido segue
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            gancho = f'        if (GOWA_NAT({g["nat"]}, ctx)) {{}} {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} {g["rotulo"]} depois: já aplicado'); continue
            k = corpo.index(rot) + len(rot)
            corpo = corpo[:k] + gancho + corpo[k:]
        elif g['tipo'] == 'multi':   # 02/11 (E4): logo depois do rótulo; o nativo devolve 0 (segue o traduzido) ou k: destinos[k-1]
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            saidas = ' '.join(f'if (_k == {n + 1}) goto {d};' for n, d in enumerate(g['destinos']))
            gancho = f'        {{ const int _k = GOWA_NAT({g["nat"]}, ctx); {saidas} }} {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} {g["rotulo"]} multi: já aplicado'); continue
            if g.get('vk'):   # antes do núcleo vk do laço (que fica logo antes do rótulo)
                k = corpo.index(g['vk'])
                k = corpo.rindex(chr(10), 0, k) + 1
            else:
                k = corpo.index(rot) + (0 if g.get('pos') == 'antes' else len(rot))
            corpo = corpo[:k] + gancho + corpo[k:]
        elif g['tipo'] == 'laco_local':
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            gancho = f'        if (GOWA_NAT({g["nat"]}, ctx)) goto {g["rotulo"]}_fim; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} {g["rotulo"]} local: já aplicado'); continue
            k = corpo.index(rot)
            ant = corpo.rindex('\n', 0, k - 1) + 1   # a linha do vk, se houver, fica depois do gancho
            if '/*VK ' in corpo[ant:k]:
                k = ant
            corpo = corpo[:k] + gancho + corpo[k:]
            d = corpo.index(f'goto {g["rotulo"]};', k + len(gancho) + len(rot))
            d = corpo.index('\n', d) + 1
            corpo = corpo[:d] + f'{g["rotulo"]}_fim: {MARCA}\n' + corpo[d:]
        else:
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            gancho = f'        if (GOWA_NAT({g["nat"]}, ctx)) goto {g["destino"]}; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} antes de {g["rotulo"]}: já aplicado'); continue
            k = corpo.index(rot)
            if g.get('fim'):   # o nativo faz o laço inteiro: entra antes do vk (se houver) e sai depois do desvio
                ant = corpo.rindex('\n', 0, k - 1) + 1
                if '/*VK ' in corpo[ant:k]:
                    k = ant
            corpo = corpo[:k] + gancho + corpo[k:]
            if g.get('fim'):
                d = corpo.index(f'goto {g["fim"]};', k + len(gancho) + len(rot))
                d = corpo.index('\n', d) + 1
                corpo = corpo[:d] + f'{g["fim"]}_fim: {MARCA}\n' + corpo[d:]
        s = s[:i] + corpo + s[fim:]
        open(p, 'w', encoding='latin-1', newline='').write(s)
        print(f'{g["fn"]}: aplicado ({g["tipo"]})')


_modo, _raiz = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..'))
# 01/11 (E4): as funções folha puras do job com registradores locais (funcao_local.py), gancho 'entrada' cada uma
if _modo == 'aplicar':
    import subprocess
    _fl = subprocess.run([sys.executable, '-I', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'funcao_local.py'),
                          arquivo(_raiz, 'gowajm_0072DF80'), os.path.join(_raiz, 'port', 'edge', 'fl_jm72.h'), 'gowajm_0072DF80'],   # 'geral' (funções com chamadas, sincronizando em volta delas) PERDEU no A/B de 02/11: f1 3,6-3,9 -> 4,1-4,3 ms, cap16 5,7-6,3 -> 6,4-6,9
                         capture_output=True, text=True, check=True).stdout.split()
    _com_gancho = {g['fn'] for g in GANCHOS}   # quem já tem nativo (ou laço local) fica com ele
    for _k in range(0, len(_fl), 2):
        if _fl[_k] in _com_gancho: continue
        GANCHOS.append(dict(tipo='entrada', pasta='gowajm_0072DF80', fn=_fl[_k], cab='fl_jm72.h', nat='fl_' + _fl[_k + 1]))
    print(f'funções locais: {len(_fl) // 2}')
main()   # o main troca o sys.argv (laco_local)
if _modo == 'aplicar':
    chamada_direta(_raiz)
