# Novidades do port

O Otavix Launcher mostra este arquivo na página "Novidades" do jogo. Cada atualização tem um título
"## <data> · <versão>" e o texto em português (### pt), inglês (### en) e, quando houver, espanhol (### es).
As mais novas ficam em cima.

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
