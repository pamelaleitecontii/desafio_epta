/**
 * @file    bme280.h
 * @brief   Driver do BME280 (temperatura, pressão e umidade) via I²C.
 *
 * O driver não depende da HAL: o acesso ao barramento é feito por funções
 * passadas em bme280_t. Isso permite testar a compensação no computador.
 *
 * Na placa, CSB está em 3V3 (modo I²C) e SDO em GND, logo o endereço é 0x76.
 */
#ifndef BME280_H
#define BME280_H

#include <stdint.h>

#define BME280_ENDERECO_I2C   0x76u
#define BME280_CHIP_ID        0x60u

#define BME280_REG_CALIB00    0x88u  /* 0x88..0xA1: T1..T3, P1..P9, H1      */
#define BME280_REG_CHIP_ID    0xD0u
#define BME280_REG_RESET      0xE0u
#define BME280_REG_CALIB26    0xE1u  /* 0xE1..0xE7: H2..H6                  */
#define BME280_REG_CTRL_HUM   0xF2u
#define BME280_REG_STATUS     0xF3u
#define BME280_REG_CTRL_MEAS  0xF4u
#define BME280_REG_CONFIG     0xF5u
#define BME280_REG_DADOS      0xF7u  /* 0xF7..0xFE: press, temp, umid       */

#define BME280_CMD_RESET      0xB6u

/* Valores brutos que o sensor devolve quando a medição foi desabilitada */
#define BME280_ADC_INVALIDO_20B  0x80000
#define BME280_ADC_INVALIDO_16B  0x8000

/* Funções de acesso ao barramento: retornam 0 em sucesso */
typedef int (*bme280_ler_fn)(void *ctx, uint8_t reg, uint8_t *dados, uint16_t n);
typedef int (*bme280_escrever_fn)(void *ctx, uint8_t reg, uint8_t valor);
typedef void (*bme280_atraso_fn)(uint32_t ms);

typedef struct
{
  uint16_t T1; int16_t T2; int16_t T3;
  uint16_t P1; int16_t P2; int16_t P3; int16_t P4; int16_t P5;
  int16_t  P6; int16_t P7; int16_t P8; int16_t P9;
  uint8_t  H1; int16_t H2; uint8_t H3; int16_t H4; int16_t H5; int8_t H6;
} bme280_calib_t;

typedef struct
{
  bme280_ler_fn      ler;
  bme280_escrever_fn escrever;
  bme280_atraso_fn   atraso_ms;
  void              *ctx;
  bme280_calib_t     calib;
  int32_t            t_fine;
} bme280_t;

/* Leitura compensada, em inteiros (sem ponto flutuante) */
typedef struct
{
  int32_t  temp_c_x100;     /* temperatura em 0,01 °C            */
  uint32_t press_pa_q24_8;  /* pressão em Pa, formato Q24.8      */
  uint32_t umid_q22_10;     /* umidade em %UR, formato Q22.10    */
} bme280_leitura_t;

typedef enum
{
  BME280_OK = 0,
  BME280_ERRO_BARRAMENTO,
  BME280_ERRO_ID,
  BME280_ERRO_DADO_INVALIDO,
} bme280_status_t;

/** Reset, conferência do chip ID, leitura da calibração e configuração. */
bme280_status_t bme280_iniciar(bme280_t *dev);

/** Lê os registradores de dados e devolve os valores compensados. */
bme280_status_t bme280_ler(bme280_t *dev, bme280_leitura_t *saida);

/* Funções puras (sem barramento), usadas também nos testes */

/** Converte os 26 + 7 bytes de calibração lidos do sensor. */
void bme280_decodificar_calib(const uint8_t c1[26], const uint8_t c2[7], bme280_calib_t *cal);

/** Separa os 8 bytes de dados (0xF7..0xFE) em leituras brutas. */
void bme280_decodificar_dados(const uint8_t d[8], int32_t *adc_p, int32_t *adc_t, int32_t *adc_h);

/** Fórmulas de compensação do datasheet (seção 4.2.3, inteiros). */
int32_t  bme280_compensar_temp(bme280_t *dev, int32_t adc_t);
uint32_t bme280_compensar_press(const bme280_t *dev, int32_t adc_p);
uint32_t bme280_compensar_umid(const bme280_t *dev, int32_t adc_h);

#endif /* BME280_H */
