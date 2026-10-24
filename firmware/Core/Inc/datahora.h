/**
 * @file    datahora.h
 * @brief   Data e hora em texto ("AAAA-MM-DD HH:MM:SS"), sem dependência de hardware.
 */
#ifndef DATAHORA_H
#define DATAHORA_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
  uint16_t ano;   /* 2000..2099 (limite do RTC do STM32) */
  uint8_t  mes;   /* 1..12 */
  uint8_t  dia;   /* 1..31 */
  uint8_t  hora;  /* 0..23 */
  uint8_t  min;   /* 0..59 */
  uint8_t  seg;   /* 0..59 */
} datahora_t;

/** Lê "AAAA-MM-DD HH:MM:SS" (aceita 'T' no lugar do espaço). Retorna 0 se válida. */
int datahora_analisar(const char *txt, datahora_t *dh);

/** Escreve "AAAA-MM-DD HH:MM:SS" (19 caracteres, sem terminador). */
size_t datahora_formatar(char *buf, const datahora_t *dh);

/** Dia da semana no formato do RTC: 1 = segunda ... 7 = domingo. */
uint8_t datahora_dia_semana(uint16_t ano, uint8_t mes, uint8_t dia);

#endif /* DATAHORA_H */
