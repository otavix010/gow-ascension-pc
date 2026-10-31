# Novidades do port

O Otavix Launcher mostra este arquivo na página "Novidades" do jogo. Cada atualização tem um título
"## <data> · <versão>" e o texto em português (### pt), inglês (### en) e, quando houver, espanhol (### es).
As mais novas ficam em cima.

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
