/*
 * otavix_legenda: a legenda das teclas do God of War: Ascension, num painel por cima do jogo (ver otavix_legenda.c).
 */
#ifndef OTAVIX_LEGENDA_H
#define OTAVIX_LEGENDA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Começa a thread do painel (uma vez; chamar de novo não faz nada). */
void otx_legenda_iniciar(void);

/* Mostra (1) ou esconde (0) o painel. Pode ser chamada de qualquer thread. */
void otx_legenda_mostrar(int sim);

#ifdef __cplusplus
}
#endif

#endif
