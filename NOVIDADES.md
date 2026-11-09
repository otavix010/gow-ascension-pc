# Novidades do port

O Otavix Launcher mostra este arquivo na página "Novidades" do jogo. Cada atualização tem um título
"## <data> · <versão>" e o texto em português (### pt), inglês (### en) e, quando houver, espanhol (### es).
As mais novas ficam em cima.

## 09/11/2026 · engine 0.11.4
### pt
- **Correção:** a pele do Kratos e outras superfícies (como a cabeça de leão da ponte) não ficam mais com manchas e listras pretas nas placas RTX 30, 40 e 50 e nas AMD.
- **Correção:** com a resolução interna em 2x a 4x, não aparecem mais riscos brancos nas bordas nem o contorno claro em volta dos personagens.
- **Correção:** a tela não fica mais preta do vídeo de abertura em diante para quem ajustou o tamanho ou a posição da tela nas opções do jogo.
- **Correção:** a geração não para mais na etapa 7 quando o antivírus apaga parte do FFmpeg: ele é baixado de novo.
- **Correção:** com o disco do jogo numa unidade que não existe mais (HD externo ou pendrive desligado), a geração mostra uma mensagem clara em vez de um erro do PowerShell.
- **Desempenho:** os shaders pré-compilados foram refeitos para a engine nova.
### en
- **Fix:** Kratos's skin and other surfaces (such as the lion head on the bridge) no longer show black blotches and stripes on RTX 30, 40 and 50 series and AMD cards.
- **Fix:** with the internal resolution at 2x to 4x, white streaks on edges and the bright outline around characters no longer appear.
- **Fix:** the screen no longer goes black from the opening video onward for players who adjusted the screen size or position in the game options.
- **Fix:** generation no longer stops at step 7 when the antivirus deletes part of FFmpeg: it is downloaded again.
- **Fix:** with the game disc on a drive that no longer exists (external drive or USB stick disconnected), generation shows a clear message instead of a PowerShell error.
- **Performance:** the precompiled shaders were rebuilt for the new engine.
### es
- **Corrección:** la piel de Kratos y otras superficies (como la cabeza de león del puente) ya no tienen manchas y rayas negras en las tarjetas RTX 30, 40 y 50 y en las AMD.
- **Corrección:** con la resolución interna en 2x a 4x, ya no aparecen rayas blancas en los bordes ni el contorno claro alrededor de los personajes.
- **Corrección:** la pantalla ya no se queda negra desde el video de apertura en adelante para quien ajustó el tamaño o la posición de la pantalla en las opciones del juego.
- **Corrección:** la generación ya no se detiene en el paso 7 cuando el antivirus borra parte de FFmpeg: se descarga de nuevo.
- **Corrección:** con el disco del juego en una unidad que ya no existe (disco externo o memoria USB desconectados), la generación muestra un mensaje claro en lugar de un error de PowerShell.
- **Rendimiento:** los shaders precompilados se rehicieron para la engine nueva.

## 08/11/2026 · shaders
### pt
- **Desempenho:** menos travadas ao entrar em áreas novas: os shaders pré-compilados passaram a cobrir o jogo inteiro (de 1,9 mil para 13,4 mil). Basta atualizar pelo launcher.
### en
- **Performance:** fewer stutters when entering new areas: the precompiled shaders now cover the whole game (from 1.9k to 13.4k). Just update through the launcher.
### es
- **Rendimiento:** menos tirones al entrar en zonas nuevas: los shaders precompilados ahora cubren todo el juego (de 1,9 mil a 13,4 mil). Basta con actualizar desde el launcher.

## 08/11/2026 · 73a3bc9
### pt
- **Correção:** a geração não para mais sempre no passo do firmware quando um download anterior das chaves veio incompleto: o arquivo é baixado de novo.
- **Correção:** depois de uma atualização do Visual Studio, a compilação não para mais com "CMake Error at CMakeLists.txt:2 (project)".
### en
- **Fix:** generation no longer always stops at the firmware step when a previous download of the keys came incomplete: the file is downloaded again.
- **Fix:** after a Visual Studio update, compiling no longer stops with "CMake Error at CMakeLists.txt:2 (project)".
### es
- **Corrección:** la generación ya no se detiene siempre en el paso del firmware cuando una descarga anterior de las claves llegó incompleta: el archivo se descarga de nuevo.
- **Corrección:** después de una actualización de Visual Studio, la compilación ya no se detiene con "CMake Error at CMakeLists.txt:2 (project)".

## 08/11/2026 · engine 0.11.3
### pt
- **Correção:** o jogo não trava mais de repente com a imagem e o som parados (podia acontecer logo nos primeiros minutos, inclusive no menu).
### en
- **Fix:** the game no longer suddenly freezes with the image and sound stopped (it could happen within the first few minutes, even in the menu).
### es
- **Corrección:** el juego ya no se congela de repente con la imagen y el sonido detenidos (podía ocurrir en los primeros minutos, incluso en el menú).

## 08/11/2026 · 3a6433d
### pt
- **Correção:** no capítulo 19, a imagem do jogo não fica mais parada com rastros e luzes piscando quando os inimigos aparecem.
### en
- **Fix:** in chapter 19, the game image no longer freezes with trails and flickering lights when enemies show up.
### es
- **Corrección:** en el capítulo 19, la imagen del juego ya no se queda congelada con estelas y luces parpadeando cuando aparecen los enemigos.

## 07/11/2026 · engine 0.11.2
### pt
- **Correção:** em sessões longas, o jogo não para mais de gravar os checkpoints depois de algumas horas.
- **Correção:** o som não some mais no meio do jogo, e o carregamento seguinte não fica mais infinito.
- **Correção:** nas cenas, o Kratos e partes do cenário não somem mais por um instante (piscadas).
### en
- **Fix:** in long sessions, the game no longer stops saving checkpoints after a few hours.
- **Fix:** the sound no longer cuts out mid-game, and the next loading screen no longer hangs forever.
- **Fix:** in cutscenes, Kratos and parts of the scenery no longer vanish for a moment (flickering).
### es
- **Corrección:** en sesiones largas, el juego ya no deja de guardar los puntos de control después de algunas horas.
- **Corrección:** el sonido ya no desaparece en mitad del juego, y la carga siguiente ya no se queda infinita.
- **Corrección:** en las escenas, Kratos y partes del escenario ya no desaparecen por un instante (parpadeos).

## 07/11/2026 · engine 0.11.1
### pt
- **Correção:** em PCs com 16 GB de RAM ou menos, o jogo não fecha mais com o erro 0xC0000409 depois de alguns minutos; quando a memória do sistema fica apertada, o jogo passa a manter menos texturas carregadas ao mesmo tempo.
### en
- **Fix:** on PCs with 16 GB of RAM or less, the game no longer closes with error 0xC0000409 after a few minutes; when system memory runs low, the game now keeps fewer textures loaded at the same time.
### es
- **Corrección:** en PC con 16 GB de RAM o menos, el juego ya no se cierra con el error 0xC0000409 después de unos minutos; cuando la memoria del sistema se queda corta, el juego mantiene menos texturas cargadas al mismo tiempo.

## 07/11/2026 · c1fafcd
### pt
- **Ajuste:** mudança no processamento do capítulo 13 (ponte de Delfos). A câmera presa depois da cena do Amuleto de Ouroboros ainda pode acontecer; para destravar, use o amuleto de novo no fim da ponte. A correção completa está em andamento.
### en
- **Adjustment:** processing change in chapter 13 (the Delphi bridge). The camera can still get stuck after the Amulet of Uroboros scene; to unstick it, use the amulet again at the end of the bridge. The full fix is in progress.
### es
- **Ajuste:** cambio en el procesamiento del capítulo 13 (el puente de Delfos). La cámara todavía puede quedarse atascada después de la escena del Amuleto de Uróboros; para destrabarla, usa el amuleto de nuevo al final del puente. La corrección completa está en curso.

## 06/11/2026 · df2fcfc
### pt
- **Diagnóstico:** o log do jogo registra mais detalhes quando o jogo fica parado esperando o processamento do SPURS, para corrigirmos as travadas e as pausas longas em algumas áreas.
### en
- **Diagnostics:** the game log records more detail when the game stalls waiting for SPURS processing, so we can fix the freezes and long pauses in some areas.
### es
- **Diagnóstico:** el registro del juego guarda más detalles cuando el juego se queda esperando el procesamiento de SPURS, para que podamos corregir los congelamientos y las pausas largas en algunas zonas.

## 06/11/2026 · 56378a7
### pt
- **Motor novo:** o jogo passa a rodar sobre a Otavix Engine PS3, o nosso motor próprio, com o mesmo desempenho e a mesma imagem da versão anterior; as próximas correções e otimizações já saem sobre ele.
- **Instalação:** a engine vem pronta num pacote baixado pelo gerador; a primeira atualização refaz a tradução do jogo (alguns minutos a mais, uma vez só). Disco, saves e opções continuam como estão.
### en
- **New engine:** the game now runs on the Otavix Engine PS3, our own engine, with the same performance and the same image as the previous version; upcoming fixes and optimizations will be built on it.
- **Installation:** the engine comes ready in a package downloaded by the generator; the first update redoes the game's translation (a few extra minutes, only once). Disc, saves and options stay as they are.
### es
- **Motor nuevo:** el juego pasa a funcionar sobre Otavix Engine PS3, nuestro propio motor, con el mismo rendimiento y la misma imagen que la versión anterior; las próximas correcciones y optimizaciones ya se harán sobre él.
- **Instalación:** el motor viene listo en un paquete que descarga el generador; la primera actualización rehace la traducción del juego (unos minutos más, una sola vez). El disco, las partidas guardadas y las opciones siguen como están.

## 06/11/2026 · f5f7dc9
### pt
- **Instalação:** o gerador não deixa mais rodar duas gerações ao mesmo tempo na mesma pasta; a segunda para com um aviso, em vez de travar a primeira.
### en
- **Installation:** the generator no longer lets two generations run at the same time in the same folder; the second one stops with a notice instead of breaking the first.
### es
- **Instalación:** el generador ya no permite dos generaciones al mismo tiempo en la misma carpeta; la segunda se detiene con un aviso en lugar de romper la primera.

## 05/11/2026 · 330a716
### pt
- **Desempenho:** fim dos congelamentos de alguns segundos ao entrar em áreas novas: o cache de shaders agora é salvo em segundo plano.
### en
- **Performance:** no more multi-second freezes when entering new areas: the shader cache is now saved in the background.
### es
- **Rendimiento:** se acabaron los congelamientos de algunos segundos al entrar en zonas nuevas: la caché de shaders ahora se guarda en segundo plano.

## 05/11/2026 · 832fc54
### pt
- **Instalação:** o gerador agora encontra o Python mesmo quando ele foi instalado sem a opção "Add python.exe to PATH".
- **Instalação:** se a pasta do jogo mudou de nome ou de lugar, o gerador a reconhece quando dá; se não der, a mensagem explica que a pasta não existe mais.
### en
- **Installation:** the generator now finds Python even when it was installed without the "Add python.exe to PATH" option.
- **Installation:** if the game folder was renamed or moved, the generator recognizes it when it can; if not, the message explains that the folder no longer exists.
### es
- **Instalación:** el generador ahora encuentra Python aunque se haya instalado sin la opción "Add python.exe to PATH".
- **Instalación:** si la carpeta del juego cambió de nombre o de lugar, el generador la reconoce cuando puede; si no, el mensaje explica que la carpeta ya no existe.

## 04/11/2026 · ffc6e00
### pt
- **Estabilidade:** corrigida a falha em que, em sessões longas, o jogo ficava cada vez mais lento e o som começava a cortar (processadores de som e física paravam um a um).
- **Desempenho:** o gerador de ruído do General6 roda nativo e 137 rotinas das SPUs passam a ser otimizadas (menos carga em processadores mais fracos).
- **Instalação:** a próxima geração refaz a tradução das SPUs sozinha e demora alguns minutos a mais.
### en
- **Stability:** fixed an issue where, in long sessions, the game got progressively slower and audio started cutting out (sound and physics processors stopped one by one).
- **Performance:** the General6 noise generator runs natively and 137 SPU routines are now optimized (less load on weaker CPUs).
- **Installation:** the next generation redoes the SPU translation on its own and takes a few extra minutes.
### es
- **Estabilidad:** corregido el fallo por el que, en sesiones largas, el juego se volvía cada vez más lento y el sonido empezaba a cortarse (los procesadores de sonido y física se detenían uno a uno).
- **Rendimiento:** el generador de ruido del General6 se ejecuta de forma nativa y 137 rutinas de las SPU pasan a estar optimizadas (menos carga en procesadores más débiles).
- **Instalación:** la siguiente generación rehace sola la traducción de las SPU y tarda unos minutos más.

## 04/11/2026 · 6ad8889
### pt
- **Instalação:** o arquivo TLOUCOUNTDOWN.SELF, que faz parte do update 1.04 oficial, não é mais retirado da pasta do update, e volta para lá se tiver sido retirado.
### en
- **Installation:** the TLOUCOUNTDOWN.SELF file, which is part of the official 1.04 update, is no longer moved out of the update folder, and goes back there if it was.
### es
- **Instalación:** el archivo TLOUCOUNTDOWN.SELF, que forma parte de la actualización 1.04 oficial, ya no se saca de la carpeta de la actualización, y vuelve allí si se había sacado.

## 03/11/2026 · f2c6830
### pt
- **Som:** menos cortes e chiado em processadores mais fracos (Xeon e CPUs com menos núcleos): parte do processamento de áudio que ainda rodava interpretada agora é traduzida, e o despacho entre as rotinas de som ficou mais leve.
- **Instalação:** quando uma atualização muda a tradução das SPUs, a próxima geração a refaz sozinha; desta vez ela demora alguns minutos a mais.
### en
- **Audio:** fewer dropouts and crackling on weaker processors (Xeon and CPUs with fewer cores): audio processing that still ran interpreted is now translated, and dispatch between the sound routines is lighter.
- **Installation:** when an update changes the SPU translation, the next generation redoes it on its own; this time it takes a few extra minutes.
### es
- **Sonido:** menos cortes y chasquidos en procesadores más débiles (Xeon y CPUs con menos núcleos): parte del procesamiento de audio que aún se interpretaba ahora está traducida, y el despacho entre las rutinas de sonido es más ligero.
- **Instalación:** cuando una actualización cambia la traducción de las SPU, la siguiente generación la rehace sola; esta vez tarda unos minutos más.

## 03/11/2026 · 74fb89b
### pt
- **Instalação:** quando a pasta do update 1.04 traz o jogo inteiro (como a pasta BCUS98232 do RPCS3), a geração deixa no port só os arquivos do update e move o resto para vfsora_do_update, sem apagar; isso evita o carregamento infinito nesse caso.
- **Instalação:** se a compilação parar por restos de uma geração anterior, o gerador limpa a pasta da compilação e compila de novo sozinho.
- **Instalação:** quando o disco do jogo estava num HD externo ou pendrive desconectado, a mensagem agora diz qual unidade falta, em vez de um erro técnico.
### en
- **Installation:** when the 1.04 update folder contains the whole game (like RPCS3's BCUS98232 folder), generation keeps only the update files in the port and moves the rest to vfsora_do_update, without deleting anything; this avoids the endless loading in that case.
- **Installation:** if the build stops because of leftovers from a previous generation, the generator cleans the build folder and builds again on its own.
- **Installation:** when the game disc was on an unplugged external drive or USB stick, the message now says which drive is missing instead of a technical error.
### es
- **Instalación:** cuando la carpeta de la actualización 1.04 trae el juego completo (como la carpeta BCUS98232 de RPCS3), la generación deja en el port solo los archivos de la actualización y mueve el resto a vfsora_do_update, sin borrar nada; así se evita la carga infinita en ese caso.
- **Instalación:** si la compilación se detiene por restos de una generación anterior, el generador limpia la carpeta de compilación y vuelve a compilar solo.
- **Instalación:** cuando el disco del juego estaba en un disco externo o pendrive desconectado, el mensaje ahora indica qué unidad falta, en lugar de un error técnico.

## 03/11/2026 · a822b21
### pt
- **Controle:** um controle Xbox (ou compatível) é reconhecido em qualquer porta do Windows; antes, só o primeiro slot do XInput chegava ao jogo.
- **Salvamento:** o progresso é gravado num arquivo temporário e só substitui o anterior quando termina, e fechar o jogo espera um salvamento em andamento; fechar no meio da gravação não deixa mais o save incompleto.
- **Áudio:** o buffer de saída passa de 20 para 40 ms, o que evita cortes nos dispositivos de áudio que aceitam filas muito curtas.
- **Instalação:** quando o download das chaves do RPCS3 falha por certificado do Windows, a geração tenta de novo pelo curl.exe do sistema.
### en
- **Controller:** an Xbox (or compatible) controller is recognized in any Windows slot; before, only the first XInput slot reached the game.
- **Saving:** progress is written to a temporary file and only replaces the previous one once complete, and closing the game waits for a save in progress; closing mid-write no longer leaves an incomplete save.
- **Audio:** the output buffer goes from 20 to 40 ms, which prevents dropouts on audio devices that accept very short queues.
- **Installation:** when downloading the RPCS3 keys fails because of a Windows certificate, generation retries through the system's curl.exe.
### es
- **Control:** un mando de Xbox (o compatible) se reconoce en cualquier puerto de Windows; antes, solo la primera ranura de XInput llegaba al juego.
- **Guardado:** el progreso se escribe en un archivo temporal y solo reemplaza al anterior cuando termina, y cerrar el juego espera un guardado en curso; cerrar a mitad de la escritura ya no deja la partida incompleta.
- **Audio:** el búfer de salida pasa de 20 a 40 ms, lo que evita cortes en los dispositivos de audio que aceptan colas muy cortas.
- **Instalación:** cuando la descarga de las claves de RPCS3 falla por un certificado de Windows, la generación vuelve a intentarlo con el curl.exe del sistema.

## 02/11/2026 · 6207b51
### pt
- **Instalação:** a opção "Automático" da versão do executável passa a usar sempre a build portátil (AVX2), a mais testada; a nativa (AVX-512) continua disponível por escolha. Quem já tinha gerado no Automático com a nativa passa à portátil na próxima atualização.
### en
- **Installation:** the "Automatic" executable version option now always uses the portable build (AVX2), the most tested one; the native build (AVX-512) remains available by choice. Players who generated with Automatic on the native build switch to the portable one on the next update.
### es
- **Instalación:** la opción "Automático" de la versión del ejecutable pasa a usar siempre la build portátil (AVX2), la más probada; la nativa (AVX-512) sigue disponible por elección. Quien ya había generado en Automático con la nativa pasa a la portátil en la próxima actualización.

## 02/11/2026 · 0b4037a
### pt
- **Instalação:** a compilação do decodificador de áudio ATRAC não falha mais em alguns discos (erro "AV_RB32/AV_RL32 não declarados").
### en
- **Installation:** compiling the ATRAC audio decoder no longer fails on some drives ("AV_RB32/AV_RL32 undeclared" error).
### es
- **Instalación:** la compilación del decodificador de audio ATRAC ya no falla en algunos discos (error "AV_RB32/AV_RL32 no declarados").

## 02/11/2026 · 2cca492
### pt
- **Instalação:** a geração não para mais quando o Windows nega o privilégio para criar links físicos (projeto em drive externo); o jogo passa a ler o disco de onde ele está.
- **Instalação:** em processadores que o Windows informa sem nome, o gerador volta a explicar a falta de AVX2 em vez de parar com um erro.
### en
- **Installation:** generation no longer stops when Windows denies the privilege to create hard links (project on an external drive); the game now reads the disc from where it is.
- **Installation:** on processors that Windows reports without a name, the generator once again explains the missing AVX2 instead of stopping with an error.
### es
- **Instalación:** la generación ya no se detiene cuando Windows niega el privilegio para crear enlaces físicos (proyecto en una unidad externa); el juego pasa a leer el disco desde donde está.
- **Instalación:** en procesadores que Windows informa sin nombre, el generador vuelve a explicar la falta de AVX2 en lugar de detenerse con un error.

## 02/11/2026 · 9870923
### pt
- **Desempenho:** mais partes do processamento gráfico do PS3 (EDGE) voltam a rodar como código nativo do PC, agora com a correção de um erro de conta encontrado na revisão; nos nossos testes, as lutas ficaram mais fluidas e com o fps mais estável.
- **Opção nova:** "Processamento gráfico nativo (EDGE)", na página Jogo. Se o cenário ficar preto, riscado ou com peças fora do lugar, desligue para usar a versão traduzida (mais lenta) e conte para nós pelo relato; não precisa gerar o jogo de novo.
- **Áreas novas:** enquanto os shaders de uma área nova compilam, o jogo não repete mais buscas no disco a cada desenho.
### en
- **Performance:** more of the PS3 graphics processing (EDGE) runs as native PC code again, now with a fix for a calculation error found in review; in our testing, fights became smoother with a steadier frame rate.
- **New option:** "Native graphics processing (EDGE)", on the Game page. If the scenery turns black, streaked or has pieces out of place, turn it off to use the translated version (slower) and tell us through a report; no need to make the game again.
- **New areas:** while a new area's shaders compile, the game no longer repeats disk lookups on every draw.
### es
- **Rendimiento:** más partes del procesamiento gráfico de la PS3 (EDGE) vuelven a ejecutarse como código nativo de PC, ahora con la corrección de un error de cálculo encontrado en la revisión; en nuestras pruebas, los combates quedaron más fluidos y con los fps más estables.
- **Opción nueva:** "Procesamiento gráfico nativo (EDGE)", en la página Juego. Si el escenario se ve negro, con rayas o con piezas fuera de lugar, desactívalo para usar la versión traducida (más lenta) y cuéntanoslo en un reporte; no hace falta generar el juego de nuevo.
- **Áreas nuevas:** mientras se compilan los shaders de un área nueva, el juego ya no repite búsquedas en el disco en cada dibujo.

## 02/11/2026 · f4fead2
### pt
- **Controle:** o DualSense e o DualShock 4 agora vibram também quando conectados por Bluetooth (antes só vibravam pelo cabo USB).
- **Instalação:** a geração do port não para mais quando o antivírus impede apagar o arquivo temporário do FFmpeg.
### en
- **Controller:** the DualSense and DualShock 4 now also vibrate when connected over Bluetooth (before, they only vibrated over a USB cable).
- **Installation:** port generation no longer stops when the antivirus prevents deleting the temporary FFmpeg file.
### es
- **Control:** el DualSense y el DualShock 4 ahora también vibran cuando están conectados por Bluetooth (antes solo vibraban con el cable USB).
- **Instalación:** la generación del port ya no se detiene cuando el antivirus impide borrar el archivo temporal de FFmpeg.

## 02/11/2026 · 46fa730
### pt
- **Correção:** desfeita a atualização anterior do processamento gráfico (EDGE), que deixava o cenário preto ou riscado em alguns PCs; o jogo volta à versão anterior enquanto investigamos.
### en
- **Fix:** the previous graphics processing (EDGE) update was rolled back, as it left the scenery black or streaked on some PCs; the game returns to the previous version while we investigate.
### es
- **Corrección:** se deshizo la actualización anterior del procesamiento gráfico (EDGE), que dejaba el escenario negro o con rayas en algunos PC; el juego vuelve a la versión anterior mientras investigamos.

## 02/11/2026 · 8ddaf1a
### pt
- **Desempenho:** mais partes do processamento gráfico do PS3 (EDGE) rodam como código nativo do PC; nos testes, o trabalho delas caiu pela metade e o uso do processador no jogo caiu cerca de 9%.
### en
- **Performance:** more of the PS3 graphics processing (EDGE) now runs as native PC code; in testing, its work was cut in half and in-game processor use dropped by about 9%.
### es
- **Rendimiento:** más partes del procesamiento gráfico de la PS3 (EDGE) se ejecutan como código nativo de PC; en las pruebas, su trabajo se redujo a la mitad y el uso del procesador en el juego bajó cerca de un 9%.

## 01/11/2026 · 03fee28
### pt
- **Estabilidade:** o jogo não fecha mais quando o compilador de shaders do Windows falha num shader; o efeito afetado é pulado.
### en
- **Stability:** the game no longer closes when the Windows shader compiler fails on a shader; the affected effect is skipped.
### es
- **Estabilidad:** el juego ya no se cierra cuando el compilador de shaders de Windows falla en un shader; el efecto afectado se omite.

## 01/11/2026 · 43491b9
### pt
- **Gráficos:** corrigidas as mãos amarelas e a pele manchada do Kratos em placas com pouca memória de vídeo (texturas que não eram refeitas).
### en
- **Graphics:** fixed Kratos' yellow hands and blotchy skin on cards with little video memory (textures that were not being redone).
### es
- **Gráficos:** corregidas las manos amarillas y la piel manchada de Kratos en tarjetas con poca memoria de vídeo (texturas que no se rehacían).

## 01/11/2026 · 830dab8
### pt
- **Carregamento:** corrigida a trava rara na tela de carregamento; o jogo agora segue sozinho em vez de ficar parado.
- **Controles:** clones de PS4, controles USB/DirectInput e Switch Pro agora funcionam, além dos de Xbox, DualSense e DualShock 4. Se os botões vierem trocados, envie a linha "[controle] HID genérico" do `jogo.log`.
- **Geração:** mensagem clara quando o caminho da pasta do jogo é longo demais para extrair o FFmpeg (sugere `C:\OtavixPorts`).
### en
- **Loading:** fixed the rare hang on the loading screen; the game now continues on its own instead of staying stuck.
- **Controllers:** PS4 clones, generic USB/DirectInput controllers and the Switch Pro now work, besides Xbox, DualSense and DualShock 4 controllers. If the buttons come swapped, send the "[controle] HID genérico" line from `jogo.log`.
- **Generation:** clear message when the game folder path is too long to extract FFmpeg (suggests `C:\OtavixPorts`).
### es
- **Carga:** corregido el cuelgue raro en la pantalla de carga; el juego ahora continúa solo en lugar de quedarse parado.
- **Mandos:** los clones de PS4, los mandos USB/DirectInput y el Switch Pro ahora funcionan, además de los de Xbox, DualSense y DualShock 4. Si los botones vienen cambiados, envíe la línea "[controle] HID genérico" del `jogo.log`.
- **Generación:** mensaje claro cuando la ruta de la carpeta del juego es demasiado larga para extraer FFmpeg (sugiere `C:\OtavixPorts`).

## 01/11/2026 · 2c172ea
### pt
- **Memória:** quando o Windows demora a liberar memória, o jogo espera em vez de fechar. Se ela faltar de vez, o log (`jogo.log`) mostra "[memória] SEM MEMÓRIA"; nesse caso aumente o arquivo de paginação do Windows (ou deixe-o gerenciado pelo sistema) e feche outros programas antes de jogar.
- **Geração:** uma pasta de compilação antiga, feita com outro compilador, agora é refeita com o do Visual Studio instalado (corrige o erro STL1000 com o Visual Studio 18).
- **Coerência:** as escritas vetoriais do processador principal agora respeitam as reservas das SPUs, como no console.
- A geração refaz a tradução do código do PPU uma vez, por isso demora mais que o normal.
### en
- **Memory:** when Windows is slow to free memory, the game now waits instead of closing. If memory runs out for good, the log (`jogo.log`) shows "[memória] SEM MEMÓRIA"; in that case increase the Windows paging file (or let the system manage it) and close other programs before playing.
- **Generation:** an old build folder made with another compiler is now rebuilt with the installed Visual Studio's compiler (fixes the STL1000 error with Visual Studio 18).
- **Coherence:** vector writes from the main processor now respect the SPUs' reservations, as on the console.
- Generation translates the PPU code again once, so it takes longer than usual.
### es
- **Memoria:** cuando Windows tarda en liberar memoria, el juego espera en lugar de cerrarse. Si la memoria se agota del todo, el registro (`jogo.log`) muestra "[memória] SEM MEMÓRIA"; en ese caso aumente el archivo de paginación de Windows (o déjelo administrado por el sistema) y cierre otros programas antes de jugar.
- **Generación:** una carpeta de compilación antigua, hecha con otro compilador, ahora se rehace con el del Visual Studio instalado (corrige el error STL1000 con Visual Studio 18).
- **Coherencia:** las escrituras vectoriales del procesador principal ahora respetan las reservas de las SPU, como en la consola.
- La generación vuelve a traducir el código de la PPU una vez, por eso tarda más de lo normal.

## 31/10/2026 · 7dcbe7c
### pt
- **Travamento na tela de carregamento:** corrigida uma disputa entre o processador principal e as SPUs ao limpar a fila de trabalhos, que podia deixar o jogo parado na carga.
- A geração refaz a tradução do código do PPU uma vez, por isso demora mais que o normal.
### en
- **Freeze on the loading screen:** fixed a race between the main processor and the SPUs when clearing the job queue, which could leave the game stuck while loading.
- Generation translates the PPU code again once, so it takes longer than usual.
### es
- **Bloqueo en la pantalla de carga:** corregida una disputa entre el procesador principal y las SPU al limpiar la cola de trabajos, que podía dejar el juego detenido en la carga.
- La generación vuelve a traducir el código de la PPU una vez, por eso tarda más de lo normal.

## 31/10/2026 · 2b834bc
### pt
- **Erro de link ao gerar de novo (símbolo indefinido):** o gerador confere o conteúdo de cada arquivo do código e recompila o que mudou, mesmo quando a atualização chega com data antiga. A primeira geração depois desta atualização recompila tudo.
- **Compilação interrompida:** arquivos de compilação corrompidos (PC desligado no meio da geração) são refeitos.
- **Visual Studio:** com duas instalações, o gerador escolhe uma só; num Build Tools sem o Clang ou o CMake, acrescenta os componentes na instalação existente.
- **Python incompleto:** um Python sem parte da biblioteca padrão é ignorado, e o gerador procura os outros instalados.
- **Downloads:** o FFmpeg, o update e o firmware mostram o progresso durante o download.
- **Vibração:** DualSense e DualShock 4 ligados por USB vibram; o motor pequeno passa a vibrar em todos os controles.
- Para aplicar, gere o jogo novamente pelo launcher.
### en
- **Link error when generating again (undefined symbol):** the generator checks the content of each code file and recompiles what changed, even when the update arrives with an old date. The first generation after this update recompiles everything.
- **Interrupted build:** corrupted build files (PC shut down during generation) are rebuilt.
- **Visual Studio:** with two installs, the generator picks only one; on Build Tools without Clang or CMake, it adds the components to the existing install.
- **Incomplete Python:** a Python missing part of the standard library is skipped, and the generator looks for the other installed ones.
- **Downloads:** FFmpeg, the update and the firmware show progress while downloading.
- **Vibration:** DualSense and DualShock 4 connected by USB vibrate; the small motor now vibrates on every controller.
- To apply, generate the game again from the launcher.
### es
- **Error de enlace al generar de nuevo (símbolo no definido):** el generador comprueba el contenido de cada archivo del código y recompila lo que cambió, aunque la actualización llegue con fecha antigua. La primera generación después de esta actualización recompila todo.
- **Compilación interrumpida:** los archivos de compilación dañados (PC apagado durante la generación) se rehacen.
- **Visual Studio:** con dos instalaciones, el generador elige solo una; en un Build Tools sin Clang o CMake, agrega los componentes a la instalación existente.
- **Python incompleto:** un Python al que le falta parte de la biblioteca estándar se ignora, y el generador busca los otros instalados.
- **Descargas:** FFmpeg, el update y el firmware muestran el progreso durante la descarga.
- **Vibración:** DualSense y DualShock 4 conectados por USB vibran; el motor pequeño ahora vibra en todos los mandos.
- Para aplicar, genera el juego de nuevo desde el launcher.

## 31/10/2026 · 6dd163d
### pt
- **Geração:** se a compilação falhar ao gerar de novo, o launcher mostra o erro em vez de seguir com a versão anterior do jogo.
### en
- **Generation:** if compiling fails when generating again, the launcher shows the error instead of continuing with the previous version of the game.
### es
- **Generación:** si la compilación falla al generar de nuevo, el launcher muestra el error en lugar de seguir con la versión anterior del juego.

## 31/10/2026 · cd7c3d2
### pt
- **Desempenho:** o processamento de geometria (EDGE) roda em código nativo do PC, conferido bit a bit contra o original: cerca de +10% de fps no capítulo 16 num i5-10300H.
- **PCs com até 8 núcleos lógicos:** o jogo usa 6 SPUs em vez de 7, com cerca de +6% de fps.
- **Áudio:** a mixagem de som tem prioridade no processador em PCs com 4 ou mais núcleos lógicos, com menos cortes em PCs lentos.
- Para aplicar, gere o jogo novamente pelo launcher.
### en
- **Performance:** geometry processing (EDGE) runs as native PC code, checked bit for bit against the original: about +10% fps in chapter 16 on an i5-10300H.
- **PCs with up to 8 logical cores:** the game uses 6 SPUs instead of 7, for about +6% fps.
- **Audio:** sound mixing gets processor priority on PCs with 4 or more logical cores, with fewer drops on slow PCs.
- To apply, generate the game again from the launcher.
### es
- **Rendimiento:** el procesamiento de geometría (EDGE) se ejecuta en código nativo de PC, comprobado bit a bit contra el original: cerca de +10% de fps en el capítulo 16 en un i5-10300H.
- **PC con hasta 8 núcleos lógicos:** el juego usa 6 SPU en vez de 7, con cerca de +6% de fps.
- **Audio:** la mezcla de sonido tiene prioridad en el procesador en PC con 4 o más núcleos lógicos, con menos cortes en PC lentos.
- Para aplicar, genera el juego de nuevo desde el launcher.

## 31/10/2026 · 17db39a
### pt
- **Compilação com Visual Studio 2022 mais antigo:** corrigido o erro `__mmask16` na versão Portátil.
- **Datas de arquivo no futuro:** o gerador corrige as datas e, se necessário, refaz a pasta de compilação. Resolve o erro "build.ninja still dirty after 100 tries".
- **Falha na etapa 5_spu (código 3221225477):** o passo agora é repetido até 3 vezes quando o Python fecha sem gravar nada.
- **Instalação do Windows SDK:** o gerador solicita permissão de administrador, aguarda o fim da instalação e valida o SDK antes de compilar. Sem permissão, exibe as instruções de instalação manual.
- Para aplicar as correções, gere o jogo novamente pelo launcher.
### en
- **Build with older Visual Studio 2022 installs:** fixed the `__mmask16` error in the Portable version.
- **File dates in the future:** the generator fixes the dates and, if needed, recreates the build folder. Resolves the "build.ninja still dirty after 100 tries" error.
- **Failure in step 5_spu (code 3221225477):** the step is now retried up to 3 times when Python exits without writing anything.
- **Windows SDK installation:** the generator requests administrator permission, waits for the installation to finish and validates the SDK before building. Without permission, it shows the manual installation instructions.
- To apply the fixes, generate the game again from the launcher.
### es
- **Compilación con Visual Studio 2022 más antiguo:** corregido el error `__mmask16` en la versión Portátil.
- **Fechas de archivo en el futuro:** el generador corrige las fechas y, si es necesario, recrea la carpeta de compilación. Resuelve el error "build.ninja still dirty after 100 tries".
- **Fallo en el paso 5_spu (código 3221225477):** el paso ahora se repite hasta 3 veces cuando Python se cierra sin escribir nada.
- **Instalación del Windows SDK:** el generador solicita permiso de administrador, espera a que termine la instalación y valida el SDK antes de compilar. Sin permiso, muestra las instrucciones de instalación manual.
- Para aplicar las correcciones, genera el juego de nuevo desde el launcher.

## 30/10/2026 · d17aa34
### pt
- **A geração parava em "6_compilar" com "Split-Path ... o argumento é nulo":** acontecia quando o gerador não achava um Python (por exemplo, só o Python da Microsoft Store). Agora ele aceita esse Python e, sem nenhum, a compilação segue com o PATH do seu PC. Gere de novo pelo launcher.
- **Geração retomada mais robusta:** uma etapa marcada como feita cujos arquivos sumiram agora é refeita sozinha, e o gerador confere de novo o Python e o FFmpeg e completa o PATH do Windows quando faltar.
- **Travadinhas em sessões longas:** com a memória de vídeo cheia, o cache de texturas agora libera as mais antigas de verdade (antes só baixava o limite), o que deve reduzir os engasgos.
### en
- **Generation stopped at "6_compilar" with "Split-Path ... the argument is null":** it happened when the generator could not find a Python (for example, only the Microsoft Store one). It now accepts that Python and, with none, the build goes on with your PC's PATH. Generate again from the launcher.
- **More robust resumed generation:** a step marked as done whose files went missing is now redone by itself, and the generator checks Python and FFmpeg again and completes the Windows PATH when it is missing.
- **Stutters in long sessions:** with video memory full, the texture cache now really frees the oldest textures (before, it only lowered the limit), which should reduce the hitches.
### es
- **La generación se detenía en "6_compilar" con "Split-Path ... el argumento es nulo":** ocurría cuando el generador no encontraba un Python (por ejemplo, solo el de Microsoft Store). Ahora acepta ese Python y, sin ninguno, la compilación sigue con el PATH de tu PC. Genera de nuevo desde el launcher.
- **Generación retomada más robusta:** un paso marcado como hecho cuyos archivos desaparecieron ahora se rehace solo, y el generador revisa de nuevo Python y FFmpeg y completa el PATH de Windows cuando falte.
- **Tirones en sesiones largas:** con la memoria de vídeo llena, la caché de texturas ahora libera de verdad las más antiguas (antes solo bajaba el límite), lo que debería reducir los tirones.

## 30/10/2026 · dc55a5c
### pt
- **Escolha da versão do executável:** a nova opção "Versão do executável" do launcher escolhe entre Automático (o padrão: o gerador escolhe pelo seu processador), Portátil (AVX2, a que quase todos usam) e Nativa (compilada para o seu processador). Trocar de versão gera de novo só a compilação. Se a Nativa não compilar no seu PC, o gerador faz a Portátil no lugar e avisa.
- **O jogo fechava ao abrir com o erro 0xC000001D num processador sem nome comercial ("Genuine Intel CPU 0000", amostra de engenharia):** esse processador anunciava instruções que não executa, e a versão Nativa era compilada com elas. No Automático, ele agora fica com a versão Portátil. Gere de novo pelo launcher.
- **Tela preta ou branca:** o report só leva o fim do log, e as medidas da tela não chegavam nele. Agora, ao fechar o jogo pela janela, o fim do log traz um resumo dessas medidas. Se a sua imagem fica preta ou branca, abra o jogo, espere a tela ficar assim, feche pela janela (não pelo Gerenciador de Tarefas) e mande um report.
### en
- **Choosing the executable version:** the launcher's new "Executable version" option picks between Automatic (the default: the generator picks by your processor), Portable (AVX2, the one almost everyone uses) and Native (built for your processor). Switching versions only redoes the build step. If Native does not build on your PC, the generator makes the Portable one instead and says so.
- **The game closed on start with error 0xC000001D on a processor without a retail name ("Genuine Intel CPU 0000", an engineering sample):** that processor advertised instructions it does not run, and the Native version was built with them. On Automatic it now gets the Portable version. Generate again from the launcher.
- **Black or white screen:** the report only carries the end of the log, and the screen measurements did not reach it. Now, when you close the game through its window, the end of the log has a summary of those measurements. If your picture goes black or white, open the game, wait until the screen looks like that, close it through the window (not through Task Manager) and send a report.
### es
- **Elegir la versión del ejecutable:** la nueva opción "Versión del ejecutable" del launcher elige entre Automático (el predeterminado: el generador elige según tu procesador), Portátil (AVX2, la que usa casi todo el mundo) y Nativa (compilada para tu procesador). Cambiar de versión solo rehace la compilación. Si la Nativa no compila en tu PC, el generador hace la Portátil en su lugar y lo avisa.
- **El juego se cerraba al abrir con el error 0xC000001D en un procesador sin nombre comercial ("Genuine Intel CPU 0000", muestra de ingeniería):** ese procesador anunciaba instrucciones que no ejecuta, y la versión Nativa se compilaba con ellas. En Automático ahora queda con la versión Portátil. Genera de nuevo desde el launcher.
- **Pantalla negra o blanca:** el report solo lleva el final del log, y las medidas de la pantalla no llegaban a él. Ahora, al cerrar el juego desde la ventana, el final del log trae un resumen de esas medidas. Si tu imagen se pone negra o blanca, abre el juego, espera a que la pantalla quede así, ciérralo desde la ventana (no desde el Administrador de tareas) y manda un report.

## 30/10/2026 · cf4be45
### pt
- **A geração parava em "6_compilar" com "'cmake' não é reconhecido":** acontecia quando o Visual Studio estava sem o componente "C++ CMake tools". Agora o gerador usa outro CMake instalado no PC ou instala o componente sozinho (o Windows pede permissão). Gere de novo pelo launcher.
- **A geração parava ao baixar o update com "Falha na execução do programa 'curl.exe': Acesso negado":** quando o antivírus ou o Windows não deixa o curl rodar, o download agora é feito pelo próprio PowerShell.
- **Memória de vídeo cheia, travadinhas e o jogo fechando em placas de 12 GB ou menos:** o jogo agora acompanha quanta memória de vídeo o Windows reserva para ele e, perto do limite, troca as texturas menos usadas em vez de continuar carregando novas.
### en
- **Generation stopped at "6_compilar" with "'cmake' is not recognized":** it happened when Visual Studio lacked the "C++ CMake tools" component. The generator now uses another CMake installed on the PC or installs the component by itself (Windows asks for permission). Generate again from the launcher.
- **Generation stopped while downloading the update with "Program 'curl.exe' failed to run: Access is denied":** when the antivirus or Windows does not let curl run, the download is now done by PowerShell itself.
- **Video memory full, stutters and the game closing on 12 GB cards or smaller:** the game now follows how much video memory Windows reserves for it and, near the limit, swaps out the least used textures instead of loading more.
### es
- **La generación se detenía en "6_compilar" con "'cmake' no se reconoce":** pasaba cuando Visual Studio no tenía el componente "C++ CMake tools". Ahora el generador usa otro CMake instalado en el PC o instala el componente solo (Windows pide permiso). Genera de nuevo desde el launcher.
- **La generación se detenía al descargar el update con "Error al ejecutar el programa 'curl.exe': Acceso denegado":** cuando el antivirus o Windows no deja ejecutar curl, la descarga ahora la hace el propio PowerShell.
- **Memoria de vídeo llena, tirones y el juego cerrándose en tarjetas de 12 GB o menos:** el juego ahora sigue cuánta memoria de vídeo le reserva Windows y, cerca del límite, cambia las texturas menos usadas en lugar de seguir cargando nuevas.

## 30/10/2026 · 964fdee
### pt
- **Tela preta com som depois da logo:** ainda não acontece em nenhum PC de teste (nem no renderizador de referência da Microsoft). Quando a imagem fica preta, o jogo agora registra no log cada desenho daquele momento e os programas que se carregaram dentro dele (sobreposições e filtros, como RivaTuner, Discord, ReShade ou filtros da placa). Se isso acontece com você, abra o jogo, espere uns 30 segundos com a tela preta, feche e mande um report com o log.
### en
- **Black screen with sound after the logo:** it still does not happen on any test PC (not even on Microsoft's reference renderer). When the picture goes black, the game now writes to the log every draw of that moment and the programs that loaded inside it (overlays and filters such as RivaTuner, Discord, ReShade or graphics card filters). If this happens to you, open the game, wait about 30 seconds on the black screen, close it and send a report with the log.
### es
- **Pantalla negra con sonido después del logo:** todavía no ocurre en ningún PC de prueba (ni en el renderizador de referencia de Microsoft). Cuando la imagen se pone negra, el juego ahora registra en el log cada dibujo de ese momento y los programas que se cargaron dentro de él (superposiciones y filtros como RivaTuner, Discord, ReShade o filtros de la tarjeta). Si te pasa, abre el juego, espera unos 30 segundos con la pantalla negra, ciérralo y manda un report con el log.

## 30/10/2026 · e8a5a83
### pt
- **A geração parava em "2_jogo" com "A parameter cannot be found that matches parameter name 'File'":** acontecia quando o arquivo do jogo escolhido vinha sem a pasta no caminho. Corrigido.
- **A geração parava em "6_compilar" com "...ps3recomp foi inesperado neste momento":** acontecia quando a pasta do jogo tinha parênteses no nome, como "GodofWarAscension(USA)", ou espaços. Corrigido; gere de novo pelo launcher.
- **Menos trabalho jogado fora nas SPUs:** o antisserrilhado que o jogo calcula nas SPUs (MLAA) já não aparecia na imagem, mas continuava processando o quadro inteiro. Agora ele quase não trabalha, e sobra processador para o resto do jogo, o que ajuda mais em PCs de 4 núcleos.
- **Controle não reconhecido:** o log agora diz quais controles o jogo encontrou e se cada um é suportado. São suportados os controles XInput (Xbox e compatíveis), o DualShock 4 e o DualSense. Um controle genérico precisa do modo XInput dele, do Steam Input ou do DS4Windows. Se o seu não funcionar, mande um report com o log.
### en
- **Generation stopped at "2_jogo" with "A parameter cannot be found that matches parameter name 'File'":** it happened when the picked game file came without its folder in the path. Fixed.
- **Generation stopped at "6_compilar" with "...ps3recomp was unexpected at this time":** it happened when the game folder had parentheses in its name, like "GodofWarAscension(USA)", or spaces. Fixed; generate again from the launcher.
- **Less wasted work on the SPUs:** the anti-aliasing the game computes on the SPUs (MLAA) no longer showed in the picture but still processed the whole frame. It now does almost nothing, leaving processor time for the rest of the game, which helps most on 4-core PCs.
- **Controller not recognized:** the log now says which controllers the game found and whether each is supported. Supported are XInput controllers (Xbox and compatible), the DualShock 4 and the DualSense. A generic controller needs its XInput mode, Steam Input or DS4Windows. If yours does not work, send a report with the log.
### es
- **La generación se detenía en "2_jogo" con "A parameter cannot be found that matches parameter name 'File'":** pasaba cuando el archivo del juego elegido venía sin la carpeta en la ruta. Corregido.
- **La generación se detenía en "6_compilar" con "...ps3recomp no se esperaba en este momento":** pasaba cuando la carpeta del juego tenía paréntesis en el nombre, como "GodofWarAscension(USA)", o espacios. Corregido; genera de nuevo desde el launcher.
- **Menos trabajo desperdiciado en las SPU:** el antialiasing que el juego calcula en las SPU (MLAA) ya no aparecía en la imagen, pero seguía procesando el cuadro entero. Ahora casi no trabaja y queda procesador para el resto del juego, lo que ayuda más en PC de 4 núcleos.
- **Mando no reconocido:** el log ahora dice qué mandos encontró el juego y si cada uno es compatible. Son compatibles los mandos XInput (Xbox y compatibles), el DualShock 4 y el DualSense. Un mando genérico necesita su modo XInput, Steam Input o DS4Windows. Si el tuyo no funciona, manda un report con el log.

## 30/10/2026 · e316534
### pt
- **A geração parava em "6_compilar" com "o gowa.exe não foi gerado":** em alguns PCs o ambiente do Visual Studio não era montado ("'cmake' não é reconhecido", "A sintaxe do nome do arquivo... está incorreta") ou faltava o Windows SDK ("could not open 'kernel32.lib'"). Agora o gerador monta esse ambiente sem depender do PATH do seu Windows, instala sozinho o Windows 11 SDK quando ele falta (o Windows pede a permissão de administrador) e, se a compilação parar no meio, continua com menos arquivos ao mesmo tempo (usa menos memória). Gere de novo pelo launcher.
- **Erros da compilação mais fáceis de achar:** o log da compilação ficou bem menor e as linhas de erro vão para o fim dele, que é a parte que o launcher envia nos reports.
- **Tela preta com som depois da logo da Santa Monica:** ainda não reproduz nos PCs de teste. Quando a imagem fica preta, o jogo agora registra no log em qual etapa do desenho ela se perde. Se isso acontece com você, mande um report com o log depois desta atualização.
### en
- **Generation stopped at "6_compilar" with "gowa.exe was not made":** on some PCs the Visual Studio environment was not set up ("'cmake' is not recognized", "The filename, directory name, or volume label syntax is incorrect") or the Windows SDK was missing ("could not open 'kernel32.lib'"). The generator now sets up that environment without depending on your Windows PATH, installs the Windows 11 SDK by itself when it is missing (Windows asks for administrator permission) and, if the build stops halfway, carries on with fewer files at a time (less memory). Generate again from the launcher.
- **Build errors easier to find:** the build log is much smaller and the error lines go to its end, which is the part the launcher sends with reports.
- **Black screen with sound after the Santa Monica logo:** it still does not happen on the test PCs. When the picture goes black, the game now writes to the log at which drawing step it gets lost. If this happens to you, send a report with the log after this update.
### es
- **La generación se detenía en "6_compilar" con "no se generó el gowa.exe":** en algunos PC el entorno de Visual Studio no se montaba ("'cmake' no se reconoce", "La sintaxis del nombre de archivo... no es correcta") o faltaba el Windows SDK ("could not open 'kernel32.lib'"). Ahora el generador monta ese entorno sin depender del PATH de tu Windows, instala solo el Windows 11 SDK cuando falta (Windows pide permiso de administrador) y, si la compilación se detiene a mitad, sigue con menos archivos a la vez (usa menos memoria). Genera de nuevo desde el launcher.
- **Errores de compilación más fáciles de encontrar:** el log de la compilación es mucho más pequeño y las líneas de error van al final, que es la parte que el launcher envía en los reports.
- **Pantalla negra con sonido después del logo de Santa Monica:** todavía no ocurre en los PC de prueba. Cuando la imagen se pone negra, el juego ahora registra en el log en qué paso del dibujo se pierde. Si te pasa, manda un report con el log después de esta actualización.

## 29/10/2026 · ccd3641
### pt
- **Tela preta e o jogo fecha sozinho, logo depois de abrir:** em alguns PCs faltava um arquivo do disco, ou ele estava cortado, depois de o jogo já ter sido gerado (pasta trocada, antivírus, OneDrive que deixou o arquivo só na nuvem). Agora o jogo confere a cópia do disco toda vez que abre e, se faltar algo, mostra quais arquivos estão com problema em vez da tela preta.
- **"Escolher o disco de novo" com o jogo já gerado:** agora o disco escolhido sempre é ligado ao jogo quando é outra pasta ou quando a cópia antiga tem arquivo faltando, sem gerar tudo de novo.
- **Escolheu a pasta de update/DLC do RPCS3 (dev_hdd0\game\BCUS98232):** o gerador agora acha sozinho o disco pela lista de jogos do RPCS3 (ou pelas pastas dele) e segue.
### en
- **Black screen and the game closes by itself right after opening:** on some PCs a disc file was missing or cut short after the game had been made (folder changed, antivirus, OneDrive keeping the file only in the cloud). The game now checks the disc copy every time it opens and, if something is missing, shows which files are the problem instead of a black screen.
- **"Pick the disc again" with the game already made:** the picked disc is now always linked to the game when it is a different folder or when the old copy has a missing file, without making everything again.
- **Picked RPCS3's update/DLC folder (dev_hdd0\game\BCUS98232):** the generator now finds the disc by itself through RPCS3's game list (or its folders) and carries on.
### es
- **Pantalla negra y el juego se cierra solo justo después de abrir:** en algunos PC faltaba un archivo del disco, o estaba cortado, después de haber generado el juego (carpeta cambiada, antivirus, OneDrive que dejó el archivo solo en la nube). Ahora el juego revisa la copia del disco cada vez que abre y, si falta algo, muestra qué archivos tienen problema en lugar de la pantalla negra.
- **"Elegir el disco de nuevo" con el juego ya generado:** ahora el disco elegido siempre se vincula al juego cuando es otra carpeta o cuando la copia antigua tiene un archivo que falta, sin generar todo de nuevo.
- **Elegiste la carpeta de update/DLC de RPCS3 (dev_hdd0\game\BCUS98232):** el generador ahora encuentra solo el disco por la lista de juegos de RPCS3 (o por sus carpetas) y sigue.

## 29/10/2026 · 03e894e
### pt
- **A geração parava com "Visual Studio Build Tools com Clang não encontrado" ou "vcvars64.bat não é reconhecido":** acontecia em PCs com um Visual Studio sem as ferramentas de C++ completas (por exemplo, o 2026 Enterprise). Agora o gerador instala sozinho o Visual Studio 2022 Build Tools (o Windows pede a permissão de administrador) e continua. Gere de novo pelo launcher.
### en
- **Generation stopped with "Visual Studio Build Tools com Clang não encontrado" or "vcvars64.bat is not recognized":** it happened on PCs with a Visual Studio without the full C++ tools (for example, 2026 Enterprise). The generator now installs Visual Studio 2022 Build Tools by itself (Windows asks for administrator permission) and carries on. Generate again from the launcher.
### es
- **La generación se detenía con "Visual Studio Build Tools com Clang não encontrado" o "vcvars64.bat no se reconoce":** pasaba en PC con un Visual Studio sin las herramientas de C++ completas (por ejemplo, el 2026 Enterprise). Ahora el generador instala solo el Visual Studio 2022 Build Tools (Windows pide permiso de administrador) y sigue. Genera de nuevo desde el launcher.

## 29/10/2026 · d163758
### pt
- **Áudio com engasgos ou cortes:** encontramos a causa. O som do jogo é mixado em rajadas por uma tarefa dos núcleos SPU do PS3, e num PC sem folga no processador essas rajadas atrasam (medimos até 40 ms entre elas, até em PCs rápidos), mais do que o jogo deixa guardado. Agora o port espera pelo jogo enquanto toca o que já tem guardado (o buffer de áudio do launcher, 100 ms por padrão). Nos nossos testes com o processador limitado a 2 núcleos, o tempo de som cortado caiu de 4 a 5 segundos para menos de 1 segundo. Parte da mixagem do som também rodava num modo lento (interpretada) e agora roda traduzida.
- **Os cortes que sobrarem ficam mais suaves** (sem o estalo seco) e a mixagem final tem prioridade de áudio no Windows.
- **Mande um report se o som falhar ou sumir:** o log agora registra a saúde do som a cada 10 segundos e avisa se o jogo parar de mandar som.
### en
- **Stuttering or cutting audio:** we found the cause. The game mixes its sound in bursts on a task of the PS3's SPU cores, and on a PC with no processor headroom those bursts run late (we measured up to 40 ms between them, even on fast PCs), longer than the game keeps buffered. The port now waits for the game while it plays what is already buffered (the launcher's audio buffer, 100 ms by default). In our tests with the processor limited to 2 cores, the time with the sound cut fell from 4-5 seconds to under 1 second. Part of the sound mixing also ran in a slow (interpreted) mode and now runs translated.
- **Any drops that remain are softer** (no hard click) and the final mix gets audio priority in Windows.
- **Send a report if the sound drops or disappears:** the log now records the sound's health every 10 seconds and says if the game stops sending sound.
### es
- **Audio con tirones o cortes:** encontramos la causa. El juego mezcla su sonido a ráfagas en una tarea de los núcleos SPU de la PS3, y en un PC sin margen en el procesador esas ráfagas se retrasan (medimos hasta 40 ms entre ellas, incluso en PC rápidos), más de lo que el juego deja guardado. Ahora el port espera al juego mientras reproduce lo que ya tiene guardado (el búfer de audio del launcher, 100 ms por defecto). En nuestras pruebas con el procesador limitado a 2 núcleos, el tiempo con el sonido cortado bajó de 4-5 segundos a menos de 1 segundo. Parte de la mezcla del sonido también corría en un modo lento (interpretada) y ahora corre traducida.
- **Los cortes que queden son más suaves** (sin el chasquido seco) y la mezcla final tiene prioridad de audio en Windows.
- **Envía un report si el sonido falla o desaparece:** el log ahora registra la salud del sonido cada 10 segundos y avisa si el juego deja de enviar sonido.

## 29/10/2026 · 06c722e
### pt
- **A geração parava no 6_compilar com "vcvars64.bat não é reconhecido" ou "'ninja' failed":** acontecia em PCs com o Visual Studio 2026 (ou outro sem as ferramentas de C++) instalado ao lado do 2022. O gerador agora escolhe uma instalação que tem o compilador e usa o ninja do próprio Visual Studio. Gere de novo pelo launcher.
- **"WinError 4551" ou "Controle Inteligente de Aplicativos" ao decriptar o jogo ou o update:** o Windows 11 bloqueava um componente de criptografia do Python. Agora, quando isso acontece, a geração usa a criptografia do próprio Windows. Também deixou de ser obrigatório ter o pip no Python.
- **Disco em outro HD sem espaço para a cópia:** quando o port não consegue apontar para o disco em outro drive, ele agora lê o jogo direto de lá, sem copiar os 34 GB. Não mova nem desligue esse HD. Se ele estiver desligado, o jogo avisa ao abrir.
### en
- **Generation stopped at 6_compilar with "vcvars64.bat is not recognized" or "'ninja' failed":** it happened on PCs with Visual Studio 2026 (or another one without the C++ tools) installed next to 2022. The generator now picks an installation that has the compiler and uses Visual Studio's own ninja. Generate again from the launcher.
- **"WinError 4551" or "Smart App Control" while decrypting the game or the update:** Windows 11 blocked a Python encryption component. Now, when that happens, generation uses Windows' own encryption. Having pip in Python is no longer required either.
- **Disc on another drive with no room for the copy:** when the port can't point to the disc on another drive, it now reads the game straight from there, without copying the 34 GB. Don't move that drive or turn it off. If it is off, the game says so when it opens.
### es
- **La generación se detenía en 6_compilar con "vcvars64.bat no se reconoce" o "'ninja' failed":** pasaba en PC con Visual Studio 2026 (u otro sin las herramientas de C++) instalado junto al 2022. El generador ahora elige una instalación que tiene el compilador y usa el ninja del propio Visual Studio. Genera de nuevo desde el launcher.
- **"WinError 4551" o "Control inteligente de aplicaciones" al descifrar el juego o el update:** Windows 11 bloqueaba un componente de cifrado de Python. Ahora, cuando pasa eso, la generación usa el cifrado del propio Windows. Tampoco hace falta ya tener pip en Python.
- **Disco en otro disco duro sin espacio para la copia:** cuando el port no puede apuntar al disco en otra unidad, ahora lee el juego directamente de ahí, sin copiar los 34 GB. No muevas ni apagues esa unidad. Si está apagada, el juego lo avisa al abrir.

## 29/10/2026 · 67b94bc
### pt
- **FFmpeg pelo servidor do launcher:** a geração agora baixa o FFmpeg (usado nos vídeos do jogo) primeiro do nosso servidor, e o GitHub fica de reserva. Isso resolve quem tinha a conexão cortada no meio do download.
- **Crashes ("O jogo fechou com erro"):** o log agora diz em qual função e em qual componente (o jogo, o driver de vídeo...) o erro aconteceu. Se o jogo fechar sozinho, mande um report logo depois.
### en
- **FFmpeg from the launcher's server:** generation now downloads FFmpeg (used for the game's videos) from our server first, with GitHub as a fallback. This fixes downloads that were cut off midway.
- **Crashes ("The game closed with an error"):** the log now says in which function and which component (the game, the video driver...) the error happened. If the game closes by itself, send a report right after.
### es
- **FFmpeg desde el servidor del launcher:** la generación ahora descarga el FFmpeg (usado en los videos del juego) primero de nuestro servidor, y GitHub queda de respaldo. Esto resuelve las descargas que se cortaban a la mitad.
- **Cierres ("El juego se cerró con un error"):** el log ahora dice en qué función y en qué componente (el juego, el driver de video...) ocurrió el error. Si el juego se cierra solo, envía un report justo después.

## 29/10/2026 · 4401d58
### pt
- **A geração parava no 6_compilar com "...was unexpected at this time" (ou "No se esperaba...")**: acontecia quando o PATH do Windows tinha uma pasta entre aspas, como a do ZeroTier ou a da NVIDIA. Corrigido: gere de novo pelo launcher.
- **"A pasta out\spu_workloads ficou sem permissão"**: agora o gerador tenta liberar a pasta sozinho e, se não conseguir, gera numa pasta nova ao lado, sem precisar apagar nada pelo Explorer.
- **Tela preta logo depois da Santa Monica:** os logs novos mostraram que, na primeira vez, o jogo pode ficar escuro enquanto compila os shaders, principalmente em placas mais antigas e em PCs com poucos núcleos. Espere de 1 a 2 minutos antes de fechar. Se continuar preto, mande um report: o diagnóstico agora também funciona com a geração de quadros ligada.
### en
- **Generation stopped at 6_compilar with "...was unexpected at this time"**: it happened when the Windows PATH had a folder in quotes, such as ZeroTier's or NVIDIA's. Fixed: generate again from the launcher.
- **"The out\spu_workloads folder has no permission"**: the generator now tries to unlock the folder by itself and, if it can't, generates into a new folder next to it, with no need to delete anything in Explorer.
- **Black screen right after Santa Monica:** the new logs showed that, on the first run, the game can stay dark while it compiles the shaders, especially on older graphics cards and PCs with few cores. Wait 1 to 2 minutes before closing. If it stays black, send a report: the diagnosis now also works with frame generation on.
### es
- **La generación se detenía en 6_compilar con "No se esperaba... en este momento"**: pasaba cuando el PATH de Windows tenía una carpeta entre comillas, como la de ZeroTier o la de NVIDIA. Corregido: genera de nuevo desde el launcher.
- **"La carpeta out\spu_workloads quedó sin permiso"**: ahora el generador intenta liberar la carpeta solo y, si no puede, genera en una carpeta nueva al lado, sin tener que borrar nada en el Explorador.
- **Pantalla negra justo después de Santa Monica:** los logs nuevos mostraron que, la primera vez, el juego puede quedar oscuro mientras compila los shaders, sobre todo en tarjetas gráficas más antiguas y en PC con pocos núcleos. Espera de 1 a 2 minutos antes de cerrar. Si sigue negro, envía un report: el diagnóstico ahora también funciona con la generación de fotogramas activada.

## 28/10/2026 · 4c7c7e0
### pt
- **Tela preta com som depois do logo da Santa Monica:** ainda não conseguimos reproduzir esse defeito nos nossos PCs. Esta versão registra no log, a cada 2 segundos, o brilho da imagem que o jogo desenhou e o da imagem enviada para a janela, além do monitor, do HDR e do modo de vídeo. Se acontecer com você, mande um report pelo launcher logo depois: o log vai mostrar em que etapa a imagem fica preta.
### en
- **Black screen with sound after the Santa Monica logo:** we still cannot reproduce it on our PCs. This version logs, every 2 seconds, the brightness of the picture the game drew and of the picture sent to the window, plus the monitor, HDR and video mode. If it happens to you, send a report from the launcher right after: the log will show at which step the picture goes black.
### es
- **Pantalla negra con sonido después del logo de Santa Monica:** todavía no logramos reproducirlo en nuestros PC. Esta versión registra en el log, cada 2 segundos, el brillo de la imagen que dibujó el juego y el de la imagen enviada a la ventana, además del monitor, el HDR y el modo de video. Si te pasa, envía un report desde el launcher justo después: el log mostrará en qué paso la imagen se vuelve negra.

## 28/10/2026 · ada0f9c
### pt
- **Cópia tirada do PS3 para pendrive:** o arquivo maior do jogo (9,3 GB) vem dividido em partes (.66600, .66601...) e o gerador dizia que ele faltava. Agora ele junta as partes sozinho.
- **Windows em árabe, vietnamita e outros idiomas:** a geração parava logo depois de decriptar o jogo, por causa de um acento numa mensagem. Corrigido.
### en
- **Copy taken from the PS3 to a USB drive:** the game's largest file (9.3 GB) comes split into parts (.66600, .66601...) and the generator said it was missing. It now joins the parts by itself.
- **Windows in Arabic, Vietnamese and other languages:** generation stopped right after decrypting the game, because of an accent in a message. Fixed.
### es
- **Copia sacada de la PS3 a un pendrive:** el archivo más grande del juego (9,3 GB) viene dividido en partes (.66600, .66601...) y el generador decía que faltaba. Ahora junta las partes solo.
- **Windows en árabe, vietnamita y otros idiomas:** la generación se detenía justo después de descifrar el juego, por un acento en un mensaje. Corregido.

## 28/10/2026 · 46fcafd
### pt
- **Controle de PS5 e de PS4 por Bluetooth:** só funcionavam pelo cabo ou com o DSX. O port procurava o controle por um nome que o Windows só usa no USB; agora acha também pelo Bluetooth, sem programa extra.
### en
- **PS5 and PS4 controllers over Bluetooth:** they only worked by cable or through DSX. The port looked for the controller under a name Windows uses only for USB; it now finds it over Bluetooth too, with no extra program.
### es
- **Control de PS5 y de PS4 por Bluetooth:** solo funcionaban por cable o con DSX. El port buscaba el control con un nombre que Windows solo usa en USB; ahora también lo encuentra por Bluetooth, sin programas extra.

## 28/10/2026 · 7df5048
### pt
- **Apagar perfil:** a opção "Apagar perfil" do jogo não fazia nada (o port não tinha essa função do PS3). Agora apaga.
### en
- **Delete profile:** the game's "Delete Profile" option did nothing (the port lacked that PS3 function). It now deletes.
### es
- **Borrar perfil:** la opción "Borrar perfil" del juego no hacía nada (al port le faltaba esa función de la PS3). Ahora borra.

## 28/10/2026 · 2802ddc
### pt
- **Texturas pela metade e cutscene em preto depois de uns minutos de jogo:** a cada área o jogo reserva e devolve memória para a placa de vídeo, e o port nunca reaproveitava o espaço devolvido. Depois de uns 8 minutos o espaço acabava, e o jogo deixava texturas incompletas e algumas cutscenes pretas (como a da Fúria depois do primeiro chefe). Corrigido.
- **Menos texturas recarregadas em placas de 8 GB ou mais:** essas placas agora guardam o dobro de texturas antes de descartar as antigas, o que diminui os engasgos ao andar pelo jogo.
- **Jogo que abre e fecha sem mostrar nada:** acontecia com a .iso criptografada (as da Redump). O gerador agora avisa e explica que precisa de uma cópia decriptada.
### en
- **Half-drawn textures and a black cutscene after a few minutes of play:** at every area the game reserves and returns memory for the graphics card, and the port never reused what was returned. After about 8 minutes it ran out, leaving textures incomplete and some cutscenes black (such as the Fury's after the first boss). Fixed.
- **Fewer reloaded textures on cards with 8 GB or more:** these cards now keep twice as many textures before dropping old ones, which reduces stutters while playing.
- **Game opening and closing without showing anything:** it happened with encrypted .iso files (the Redump ones). The generator now warns and explains that a decrypted copy is needed.
### es
- **Texturas a medias y cinemática en negro tras unos minutos de juego:** en cada zona el juego reserva y devuelve memoria para la tarjeta de video, y el port nunca reutilizaba lo devuelto. Tras unos 8 minutos se agotaba, dejando texturas incompletas y algunas cinemáticas en negro (como la de la Furia después del primer jefe). Corregido.
- **Menos texturas recargadas en tarjetas de 8 GB o más:** ahora guardan el doble de texturas antes de descartar las viejas, lo que reduce los tirones al jugar.
- **Juego que abre y se cierra sin mostrar nada:** pasaba con la .iso cifrada (las de Redump). El generador ahora avisa y explica que hace falta una copia descifrada.

## 27/10/2026 · a75ee9a
### pt
- **Firmware escolhido errado:** quem escolhia no campo "Firmware" a pasta do jogo, a PKGDIR ou um arquivo de outra versão via a geração parar. Agora o gerador avisa que aquilo não serve e baixa sozinho o firmware oficial do PS3, como já fazia com o campo vazio.
- **Pasta do update no lugar do disco:** quem escolhia a pasta do update 1.12 (o arquivo .pkg) como disco agora tem o disco achado sozinho na pasta ao lado, quando ele está lá; senão, o aviso explica que aquilo é o update.
- **Jogo dentro do OneDrive:** quando o Windows não deixa ligar os arquivos do disco (como no OneDrive), o gerador copia o disco para a pasta do port em vez de parar.
### en
- **Wrong firmware picked:** picking the game folder, PKGDIR or a file from another version in the "Firmware" field stopped the generation. The generator now says it does not fit and downloads the official PS3 firmware by itself, as it already did with the field empty.
- **Update folder instead of the disc:** picking the 1.12 update folder (the .pkg file) as the disc now finds the disc by itself in the folder next to it, when it is there; otherwise the message explains that it is the update.
- **Game inside OneDrive:** when Windows does not allow linking the disc files (as in OneDrive), the generator copies the disc into the port folder instead of stopping.
### es
- **Firmware equivocado:** elegir en el campo "Firmware" la carpeta del juego, PKGDIR o un archivo de otra versión detenía la generación. Ahora el generador avisa que eso no sirve y descarga solo el firmware oficial de PS3, como ya hacía con el campo vacío.
- **Carpeta del update en lugar del disco:** elegir la carpeta del update 1.12 (el archivo .pkg) como disco ahora encuentra el disco solo en la carpeta de al lado, cuando está ahí; si no, el aviso explica que eso es el update.
- **Juego dentro de OneDrive:** cuando Windows no deja enlazar los archivos del disco (como en OneDrive), el generador copia el disco a la carpeta del port en vez de detenerse.

## 27/10/2026 · ef9c8db
### pt
- **Idioma do jogo:** o port dizia ao jogo que o console estava sempre em inglês. Agora ele segue a opção "Idioma" do launcher (página Jogo): Automático usa o idioma do Windows quando o disco tem (inglês, francês, espanhol ou português do Brasil); as outras opções fixam um deles. O jogo guardava o idioma no save na primeira vez e não olhava mais; agora a escolha vale também para quem já tem save.
### en
- **Game language:** the port always told the game the console was in English. It now follows the launcher's "Language" option (Game page): Automatic uses the Windows language when the disc has it (English, French, Spanish or Brazilian Portuguese); the other choices pick one of them. The game stored the language in the save the first time and never looked again; the choice now also applies to existing saves.
### es
- **Idioma del juego:** el port le decía al juego que la consola siempre estaba en inglés. Ahora sigue la opción "Idioma" del launcher (página Juego): Automático usa el idioma de Windows cuando el disco lo tiene (inglés, francés, español o portugués de Brasil); las demás opciones fijan uno de ellos. El juego guardaba el idioma en la partida la primera vez y no volvía a mirarlo; ahora la elección vale también para quien ya tiene partida guardada.

## 27/10/2026 · 127ed67
### pt
- **Disco na pasta do RPCS3:** quem escolhia a pasta do RPCS3 como disco recebia "não achei o jogo". Agora o gerador procura o jogo dentro dela (games e dev_hdd0\disc). E quem escolhia a pasta do código do port, baixada do GitHub, recebe um aviso claro de que ali não está o jogo.
### en
- **Disc in the RPCS3 folder:** picking the RPCS3 folder as the disc gave "game not found". The generator now looks for the game inside it (games and dev_hdd0\disc). Picking the port's code folder, downloaded from GitHub, now says clearly that the game is not there.
### es
- **Disco en la carpeta de RPCS3:** elegir la carpeta de RPCS3 como disco daba "no encontré el juego". Ahora el generador busca el juego dentro (games y dev_hdd0\disc). Y elegir la carpeta del código del port, descargada de GitHub, avisa con claridad que el juego no está ahí.

## 27/10/2026 · 8b3fefd
### pt
- **Vídeos pela metade e botões esticados (placas AMD mais antigas, como a RX 580):** nessas placas, o último ponto de alguns desenhos era lido como zero, o que cortava os vídeos de abertura pela metade e esticava ícones. Corrigido.
### en
- **Half-screen videos and stretched buttons (older AMD cards, such as the RX 580):** on these cards, the last point of some drawings was read as zero, which cut the intro videos in half and stretched icons. Fixed.
### es
- **Videos a media pantalla y botones estirados (tarjetas AMD más antiguas, como la RX 580):** en estas tarjetas, el último punto de algunos dibujos se leía como cero, lo que cortaba los videos de inicio por la mitad y estiraba los íconos. Corregido.

## 27/10/2026 · 0348189
### pt
- **Mãos e armas amarelas, brilho estourado:** o PS3 faz algumas contas de cor com precisão reduzida, e nelas um valor inválido vira zero na hora. O port fazia essas contas com precisão total, e o valor inválido chegava até a tela, apagando uma das cores (o que sobrava ficava amarelo) ou estourando o brilho. Agora as contas seguem a precisão do PS3. A cor do céu em algumas áreas também ficou mais fiel.
### en
- **Yellow hands and weapons, blown-out brightness:** the PS3 does some color math at reduced precision, where an invalid value turns into zero on the spot. The port did that math at full precision, and the invalid value reached the screen, wiping out one of the colors (what was left looked yellow) or blowing out the brightness. The math now follows the PS3's precision. The sky color in some areas is also more faithful.
### es
- **Manos y armas amarillas, brillo excesivo:** la PS3 hace algunas cuentas de color con precisión reducida, y en ellas un valor inválido se vuelve cero al instante. El port hacía esas cuentas con precisión total, y el valor inválido llegaba a la pantalla, borrando uno de los colores (lo que quedaba se veía amarillo) o reventando el brillo. Ahora las cuentas siguen la precisión de la PS3. El color del cielo en algunas zonas también es más fiel.

## 27/10/2026 · launcher 1.0.49
### pt
- **VRR desligado por padrão de novo:** com o VRR ligado, alguns monitores de 120 Hz ou mais ficavam com a tela preta e só o som no vídeo de abertura. A opção volta a vir desligada (o jogo espera a tela); quem tem monitor com G-SYNC ou FreeSync pode ligá-la em Opções avançadas > Vídeo. Vale assim que o launcher atualizar para a 1.0.49.
### en
- **VRR off by default again:** with VRR on, some 120 Hz or faster monitors showed a black screen with only the sound in the opening video. The option is off by default again (the game waits for the screen); with a G-SYNC or FreeSync monitor you can turn it on in Advanced options > Video. It takes effect once the launcher updates to 1.0.49.
### es
- **VRR desactivado por defecto otra vez:** con el VRR activado, algunos monitores de 120 Hz o más mostraban la pantalla negra y solo el sonido en el vídeo de apertura. La opción vuelve a venir desactivada (el juego espera a la pantalla); con un monitor G-SYNC o FreeSync puedes activarla en Opciones avanzadas > Vídeo. Se aplica en cuanto el launcher se actualice a la 1.0.49.

## 27/10/2026 · 236282e
### pt
- **Sem congelar ao entrar em áreas novas (ao abrir uma porta, por exemplo):** quando o jogo precisava de efeitos visuais (shaders) que ainda não estavam prontos, ele parava até montar todos, por até alguns segundos. Agora eles são montados em segundo plano, sem parar o jogo; algum objeto pode aparecer um instante depois, só na primeira vez.
### en
- **No freezing when entering new areas (when opening a door, for example):** when the game needed visual effects (shaders) that were not ready yet, it stopped until all of them were built, for up to a few seconds. They are now built in the background without stopping the game; an object may show up a moment later, only the first time.
### es
- **Sin congelarse al entrar en zonas nuevas (al abrir una puerta, por ejemplo):** cuando el juego necesitaba efectos visuales (shaders) que aún no estaban listos, se detenía hasta prepararlos todos, por hasta algunos segundos. Ahora se preparan en segundo plano sin detener el juego; algún objeto puede aparecer un instante después, solo la primera vez.

## 27/10/2026 · 21186cc
### pt
- **Menos engasgos ao entrar em áreas novas:** o jogo travava por um instante na primeira vez em cada área, enquanto montava os efeitos visuais (shaders) para a sua placa. Agora o launcher já traz esses shaders prontos, reunidos das partidas de teste, e os coloca no jogo ao gerar. Os que ainda não estiverem no pacote continuam sendo montados na primeira vez, e o pacote cresce a cada atualização. Vale depois que o launcher gerar o jogo com esta versão.
### en
- **Fewer stutters when entering new areas:** the game froze for a moment the first time in each area while it built the visual effects (shaders) for your graphics card. The launcher now brings those shaders ready, gathered from test playthroughs, and puts them in the game when generating it. Any not yet in the pack are still built the first time, and the pack grows with each update. It takes effect once the launcher generates the game with this version.
### es
- **Menos tirones al entrar en zonas nuevas:** el juego se trababa un instante la primera vez en cada zona mientras preparaba los efectos visuales (shaders) para tu tarjeta. Ahora el launcher ya trae esos shaders listos, reunidos de las partidas de prueba, y los pone en el juego al generarlo. Los que aún no estén en el paquete se siguen preparando la primera vez, y el paquete crece con cada actualización. Se aplica después de que el launcher genere el juego con esta versión.

## 27/10/2026 · 15043d0
### pt
- **Geração parando no "pip install":** em PCs com outro programa que traz o próprio Python (como o Inkscape), o gerador podia usar esse Python, que não tem o pip, e a geração parava logo no começo. Agora ele escolhe sozinho um Python 3.10 ou mais novo que funcione e, se só houver um sem pip, instala o pip nele.
### en
- **Generation stopping at "pip install":** on PCs with another program that brings its own Python (such as Inkscape), the generator could use that Python, which has no pip, and generation stopped right at the start. It now picks a working Python 3.10 or newer by itself and, if the only one has no pip, installs pip in it.
### es
- **La generación se detenía en "pip install":** en PC con otro programa que trae su propio Python (como Inkscape), el generador podía usar ese Python, que no tiene pip, y la generación se detenía al principio. Ahora elige solo un Python 3.10 o más nuevo que funcione y, si el único no tiene pip, le instala el pip.

## 27/10/2026 · a5e1053
### pt
- **Pele manchada em placas AMD:** nas placas Radeon, Kratos, a Fúria e outros personagens apareciam com manchas claras e escuras na pele (às vezes com cara de vidro trincado). A causa era o jeito como os dados dos modelos chegavam à placa de vídeo. Agora a pele sai limpa. Vale depois que o launcher gerar o jogo com esta versão.
### en
- **Blotchy skin on AMD cards:** on Radeon cards, Kratos, the Fury and other characters showed light and dark patches on their skin (sometimes looking like cracked glass). The cause was the way the models' data reached the graphics card. The skin now comes out clean. It takes effect once the launcher generates the game with this version.
### es
- **Piel manchada en tarjetas AMD:** en las tarjetas Radeon, Kratos, la Furia y otros personajes aparecían con manchas claras y oscuras en la piel (a veces con aspecto de vidrio roto). La causa era la forma en que los datos de los modelos llegaban a la tarjeta gráfica. Ahora la piel sale limpia. Se aplica después de que el launcher genere el juego con esta versión.

## 27/10/2026 · 1b60ed6
### pt
- **VRR (G-SYNC / FreeSync):** nova opção no Vídeo, no lugar do V-sync, ligada por padrão. Cada quadro vai para a tela assim que fica pronto: num monitor com G-SYNC ou FreeSync, uma queda de fps passa suave, sem engasgo. Num monitor sem VRR a imagem pode rasgar em movimento; aí desligue, e o jogo espera a tela (vsync). Vale depois que o launcher gerar o jogo com esta versão.
### en
- **VRR (G-SYNC / FreeSync):** a new option under Video, in place of V-sync, on by default. Each frame goes to the screen as soon as it is ready: on a G-SYNC or FreeSync monitor, a drop in fps stays smooth, without stutter. On a monitor without VRR the picture may tear in motion; then turn it off, and the game waits for the screen (vsync). It takes effect once the launcher generates the game with this version.

## 27/10/2026 · 6f42a1f
### pt
- **Controles de PlayStation:** o jogo agora reconhece direto o DualSense (PS5), o DualSense Edge e o DualShock 4 (PS4), por cabo ou Bluetooth, sem precisar de DS4Windows ou do DSX em modo Xbox. Antes, só os controles no padrão Xbox funcionavam.
### en
- **PlayStation controllers:** the game now recognizes the DualSense (PS5), the DualSense Edge and the DualShock 4 (PS4) directly, by cable or Bluetooth, with no need for DS4Windows or DSX in Xbox mode. Before, only Xbox-style controllers worked.

## 27/10/2026 · 12574d5
### pt
- **"Não achei o jogo" com uma .iso:** quando o arquivo escolhido não abria (movido, renomeado, download por terminar), o gerador só dizia que não achou o jogo. Agora, se houver um único arquivo do jogo na mesma pasta, ele é usado; se não houver, o aviso diz por que o arquivo não abriu. Numa pasta sem o jogo, o aviso mostra o que há dentro dela.
### en
- **"Game not found" with an .iso:** when the picked file did not open (moved, renamed, an unfinished download), the generator only said it did not find the game. Now, if there is a single game file in the same folder, it is used; if not, the message says why the file did not open. For a folder without the game, the message shows what is inside it.

## 27/10/2026 · 11e8205
### pt
- **Pele amarela ou personagens cinza:** em algumas placas (principalmente RTX 30xx), um valor inválido nas contas de cor pintava mãos, rostos e corpos de amarelo ou cinza. Agora esses valores são barrados antes de chegar na tela. Se acontecia com você, conte pelo "Reportar problema" se melhorou.
- **Ponteiro do mouse:** na tela cheia, o ponteiro não fica mais parado sobre o jogo.
- **Cache de shaders:** agora fica na pasta "cache" do port. Na primeira abertura depois desta atualização, o jogo monta o cache de novo (alguns travadinhos no começo), e depois ele fica guardado mesmo se você mover o jogo de pasta pelo launcher.
### en
- **Yellow skin or gray characters:** on some cards (mostly RTX 30xx), an invalid value in the color math painted hands, faces and bodies yellow or gray. Now those values are stopped before they reach the screen. If it happened to you, tell us through "Report a problem" whether it got better.
- **Mouse pointer:** in fullscreen, the pointer no longer sits over the game.
- **Shader cache:** it now lives in the port's "cache" folder. The first time you open the game after this update it builds the cache again (a few hitches at the start), and then it stays even if you move the game to another folder through the launcher.

## 27/10/2026 · f4d25ff
### pt
- **O jogo abre e fecha:** quando falta um arquivo do jogo na sua cópia (extração ou cópia interrompida, falta de espaço, antivírus, ou um pendrive/HD em FAT32, que corta arquivos acima de 4 GB), o jogo abria e fechava sem dizer nada. Agora o gerador confere a cópia inteira e diz quais arquivos faltam ou vieram cortados. Se o seu jogo abre e fecha, gere de novo para ver o aviso.
### en
- **The game opens and closes:** when a file of the game is missing from your copy (an interrupted unpack or copy, no free space, the antivirus, or a FAT32 drive, which cuts files over 4 GB), the game opened and closed without a word. Now the generator checks the whole copy and says which files are missing or cut short. If your game opens and closes, make it again to see the message.

## 26/10/2026 · 2f0ff9b
### pt
- **Download do FFmpeg:** quando o FFmpeg chegava incompleto, a geração parava com "End of Central Directory record could not be found". Agora o gerador baixa de novo sozinho e, se falhar outra vez, avisa para conferir a internet e o antivírus.
### en
- **FFmpeg download:** when FFmpeg arrived incomplete, the generation stopped with "End of Central Directory record could not be found". Now the generator downloads it again by itself and, if it fails once more, says to check the internet and the antivirus.

## 26/10/2026 · 9d6936c
### pt
- **Ícones do teclado:** jogando no teclado e mouse, o jogo agora mostra as teclas na tela (Espaço, E, Q, F, setas, botões do mouse…) no lugar dos botões do controle do PS3. Ao voltar para o controle, os ícones do PS3 voltam sozinhos.
- **L3 e R3 no teclado:** L3 agora é Z e R3 é V (a legenda do F1 mostra o mapa completo).
- **Download do jogo:** em alguns Windows o download parava com o erro 35 do curl (checagem de revogação do certificado). Agora o gerador tenta de novo sem essa checagem.
### en
- **Keyboard icons:** when playing with keyboard and mouse, the game now shows the keys on screen (Space, E, Q, F, arrows, mouse buttons…) instead of the PS3 controller buttons. Going back to the controller brings the PS3 icons back by itself.
- **L3 and R3 on the keyboard:** L3 is now Z and R3 is V (the F1 legend shows the full map).
- **Game download:** on some Windows machines the download stopped with curl error 35 (certificate revocation check). Now the generator tries again without that check.

## 26/10/2026 · 3ef42ed
### pt
- **Processador sem AVX2:** o port precisa de AVX2 (Intel Core de 4ª geração em diante, AMD Ryzen). Antes, num processador sem ele, o jogo era gerado e fechava ao abrir. Agora o gerador avisa logo no começo.
### en
- **Processor without AVX2:** the port needs AVX2 (Intel Core 4th generation onwards, AMD Ryzen). Before, on a processor without it, the game was made and closed at start. Now the generator says so right at the beginning.

## 26/10/2026 · dc1616d
### pt
- **Disco em outro drive:** quando o Windows nega o atalho e os links para o disco, o gerador agora copia o jogo para a pasta do port em qualquer caso. Antes, um "acesso negado" parava a geração.
- **Pasta errada:** se a pasta escolhida como disco for a pasta onde o launcher instala os ports, o aviso agora diz isso.
### en
- **Disc on another drive:** when Windows refuses both the link and the file links to the disc, the generator now copies the game into the port's folder in every case. Before, an "access denied" stopped the generation.
- **Wrong folder:** if the folder picked as the disc is the one where the launcher installs the ports, the message now says so.

## 26/10/2026 · ab9c15b
### pt
- **Disco de outra região:** o port só funciona com o disco americano (BCUS98232). Com o europeu (BCES01741) ou de outra região, o jogo abria e fechava sem dizer nada. Agora o gerador e o jogo avisam qual é a versão do disco e como trocar. Escolher o disco americano em "Escolher o disco de novo" já basta, sem gerar de novo.
### en
- **Disc of another region:** the port only works with the US disc (BCUS98232). With the European one (BCES01741) or another region's, the game opened and closed without a word. Now the generator and the game say which version the disc is and how to swap it. Picking the US disc in "Pick the disc again" is enough, with no need to make the game again.

## 26/10/2026 · 9d43781
### pt
- **Update escolhido errado:** se a pasta escolhida como update não for o update 1.04 (por exemplo, a mesma pasta do disco), o gerador avisa e baixa o update oficial, em vez de parar. Ele também acha o update dentro da pasta do RPCS3.
### en
- **Wrong update picked:** if the folder picked as the update is not the 1.04 update (for example, the disc folder itself), the generator says so and downloads the official update instead of stopping. It also finds the update inside the RPCS3 folder.

## 26/10/2026 · dbe1a90
### pt
- **Disco em outro drive:** quando o Windows não deixa ligar o disco por atalho e ele está em outro drive (como uma unidade de rede), o gerador copia o jogo para a pasta do port. Antes, ele confere se há espaço.
### en
- **Disc on another drive:** when Windows does not let the disc be linked and it is on another drive (such as a network drive), the generator copies the game into the port's folder, checking the free space first.

## 26/10/2026 · c22797c
### pt
- **Travamento raro corrigido:** às vezes, depois de alguns minutos, o jogo parava de desenhar e ficava congelado. Isso acontecia quando o leitor de comandos da placa se perdia num trecho que o jogo ainda estava escrevendo. Agora ele espera o trecho ficar pronto.
### en
- **Rare freeze fixed:** after some minutes the game could stop drawing and freeze. This happened when the graphics command reader got lost in a stretch the game was still writing. It now waits for that stretch to be ready.

## 26/10/2026 · 327e069
### pt
- **Objetos e efeitos sumindo depois de um tempo corrigido:** depois de uns 16 minutos de jogo, partes da imagem paravam de aparecer até reiniciar. Os "shaders que param de carregar" eram isso.
- **Desempenho:** ganhos pequenos em processadores sem AVX-512 e em PCs com menos de 12 threads.
### en
- **Objects and effects disappearing after a while fixed:** after about 16 minutes of play, parts of the picture stopped appearing until a restart (the "shaders stop loading").
- **Performance:** small gains on processors without AVX-512 and on PCs with fewer than 12 threads.

## 26/10/2026 · c9eb4de
### pt
- **Esquiva (analógico direito):** passa a responder também na diagonal. Toques e puxadas rápidas não se perdem mais quando o fps está baixo.
### en
- **Dodge (right stick):** it now also responds on diagonals, and quick taps and flicks are no longer lost when the frame rate is low.

## 25/10/2026 · 5e9bda1
### pt
- **Disco:** nos PCs em que o atalho para o disco não abre, o gerador liga os arquivos um a um, sem copiar e sem ocupar espaço a mais.
### en
- **Disc:** on PCs where the link to the disc does not open, the generator links the files one by one, without copying or using more space.

## 25/10/2026 · a2ffff6
### pt
- **Disco:** escolher a pasta PS3_GAME (ou a USRDIR) direto passou a funcionar.
- O atalho do disco é conferido e refeito quando o Windows o deixa sem abrir.
### en
- **Disc:** picking the PS3_GAME folder (or its USRDIR) directly now works.
- The link to the disc is checked and made again when Windows leaves it unopenable.

## 25/10/2026 · b4bee03
### pt
- **Sem o disco:** o jogo agora avisa o que falta, em vez de abrir e fechar sem dizer nada.
### en
- **Without the disc:** the game now says what is missing instead of opening and closing silently.

## 25/10/2026 · 3c144cf
### pt
- **Teclado:** F1 mostra e esconde a legenda das teclas. Ela também aparece por alguns segundos na primeira tecla.
### en
- **Keyboard:** F1 shows and hides the keys' legend, which also appears for a few seconds at the first key.

## 25/10/2026 · 487a6cb
### pt
- **Versão:** a tela de título mostra 01.04 com o update instalado.
### en
- **Version:** the title screen shows 01.04 with the update installed.

## 25/10/2026 · 3168665
### pt
- **Teclado e mouse:** WASD move, Espaço pula e o mouse ataca, junto com o controle.
- **Áudio:** buffer maior, com menos cortes quando o jogo fica lento.
- **Tela cheia ao abrir** e correções de texturas.
### en
- **Keyboard and mouse:** WASD moves, Space jumps and the mouse attacks, together with the controller.
- **Audio:** a bigger buffer, so fewer cuts when the game slows down.
- **Fullscreen at start** and texture fixes.

## 25/10/2026 · e4db658
### pt
- **Fechar a janela encerra o jogo de vez.** Antes, ele continuava rodando escondido e deixava o PC lento.
### en
- **Closing the window ends the game for good.** Before, it kept running hidden and slowed the PC down.
