# Novidades do port

O Otavix Launcher mostra este arquivo na página "Novidades" do jogo. Cada atualização tem um título
"## <data> · <versão>" e o texto em português (### pt), inglês (### en) e, quando houver, espanhol (### es).
As mais novas ficam em cima.

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
