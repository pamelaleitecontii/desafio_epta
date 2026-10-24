/**
 * @file    csv.h
 * @brief   Formatação das linhas do CSV, sem printf e sem ponto flutuante.
 */
#ifndef CSV_H
#define CSV_H

#include <stddef.h>
#include <stdint.h>

/* Maior linha possível, com folga (a linha típica tem cerca de 90 bytes) */
#define CSV_LINHA_MAX  192u

typedef struct
{
  uint64_t t_us;            /* tempo desde a energização, em µs           */
  uint8_t  tem_imu;         /* 0: campos do IMU ficam vazios              */
  int16_t  acel[3];         /* brutos, ±16 g                              */
  int16_t  giro[3];         /* brutos, ±2000 dps                          */
  uint8_t  tem_bme;         /* 0: campos do BME280 ficam vazios           */
  int32_t  temp_c_x100;
  uint32_t press_pa_q24_8;
  uint32_t umid_q22_10;
  uint8_t  bme_novo;        /* 1: leitura do BME280 feita desde a última linha */
  uint32_t perdidas;        /* amostras do IMU descartadas desde a última linha */
} csv_linha_t;

/** Escreve a linha de cabeçalho (nomes das colunas). Retorna o tamanho. */
size_t csv_cabecalho(char *buf, size_t tam);

/** Formata uma linha terminada em "\r\n". Retorna o tamanho, ou 0 se não couber. */
size_t csv_formatar(char *buf, size_t tam, const csv_linha_t *l);

/** Escreve um inteiro com casas decimais fixas. Exemplo: (12345, 2) vira "123.45". */
size_t csv_fixo(char *buf, int64_t valor, unsigned casas);

/** Escreve um inteiro sem sinal de 64 bits em decimal. */
size_t csv_u64(char *buf, uint64_t valor);

#endif /* CSV_H */
