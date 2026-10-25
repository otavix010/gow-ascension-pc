/*
 * otavix_osd: o medidor padrão dos ports Otavix (Windows).
 *
 * Uma linha de texto nítida no canto de cima à esquerda da janela do jogo, desenhada numa
 * janela própria (em camadas, transparente ao mouse, presa por cima da do jogo) na resolução
 * real da tela, sem passar pelo renderizador do jogo. O tamanho segue a referência de 1080p:
 * fonte de 16 px quando a área do jogo tem 1080 linhas, proporcional nas outras alturas, igual
 * em todos os jogos qualquer que seja a resolução interna deles.
 *
 * Não aparece sobre tela cheia EXCLUSIVA (só janela e tela cheia sem bordas).
 */
#ifndef OTAVIX_OSD_H
#define OTAVIX_OSD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Começa a thread do OSD (uma vez; chamar de novo não faz nada). */
void otx_osd_iniciar(void);

/* Troca o texto (UTF-8, uma linha). NULL ou "" esconde. Pode ser chamada de qualquer thread. */
void otx_osd_texto(const char *linha);

#ifdef __cplusplus
}
#endif

#endif
