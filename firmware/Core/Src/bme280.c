/**
 * @file    bme280.c
 * @brief   Driver do BME280. Compensação conforme o datasheet da Bosch
 *          (BST-BME280-DS002, seção 4.2.3), em aritmética inteira.
 */
#include "bme280.h"

/*
 * Configuração escolhida para o voo: umidade x1, temperatura x2 e pressão x8,
 * porque a pressão é a grandeza mais importante (é dela que sai a altitude).
 * O filtro IIR com coeficiente 4 reduz o ruído sem atrasar demais a resposta
 * durante a subida, e o sensor fica em modo normal com t_standby de 0,5 ms.
 * Tempo de medição (datasheet, apêndice B): 1,25 + 2,3*2 + (2,3*8 + 0,575)
 * + (2,3*1 + 0,575) = 27,7 ms, ou seja, cerca de 35 medições por segundo.
 */
#define CTRL_HUM_VALOR   0x01u                                 /* osrs_h = x1           */
#define CONFIG_VALOR     ((0u << 5) | (2u << 2))               /* t_sb 0,5 ms, IIR 4    */
#define CTRL_MEAS_VALOR  ((2u << 5) | (4u << 2) | 3u)          /* x2, x8, modo normal   */

void bme280_decodificar_calib(const uint8_t c1[26], const uint8_t c2[7], bme280_calib_t *cal)
{
  cal->T1 = (uint16_t)(c1[1] << 8 | c1[0]);
  cal->T2 = (int16_t)(c1[3] << 8 | c1[2]);
  cal->T3 = (int16_t)(c1[5] << 8 | c1[4]);
  cal->P1 = (uint16_t)(c1[7] << 8 | c1[6]);
  cal->P2 = (int16_t)(c1[9] << 8 | c1[8]);
  cal->P3 = (int16_t)(c1[11] << 8 | c1[10]);
  cal->P4 = (int16_t)(c1[13] << 8 | c1[12]);
  cal->P5 = (int16_t)(c1[15] << 8 | c1[14]);
  cal->P6 = (int16_t)(c1[17] << 8 | c1[16]);
  cal->P7 = (int16_t)(c1[19] << 8 | c1[18]);
  cal->P8 = (int16_t)(c1[21] << 8 | c1[20]);
  cal->P9 = (int16_t)(c1[23] << 8 | c1[22]);
  /* c1[24] (0xA0) não é usado */
  cal->H1 = c1[25];

  cal->H2 = (int16_t)(c2[1] << 8 | c2[0]);
  cal->H3 = c2[2];
  /* H4 e H5 são de 12 bits, com sinal, e compartilham o registrador 0xE5 */
  cal->H4 = (int16_t)(((int16_t)(int8_t)c2[3] * 16) | (c2[4] & 0x0F));
  cal->H5 = (int16_t)(((int16_t)(int8_t)c2[5] * 16) | (c2[4] >> 4));
  cal->H6 = (int8_t)c2[6];
}

void bme280_decodificar_dados(const uint8_t d[8], int32_t *adc_p, int32_t *adc_t, int32_t *adc_h)
{
  *adc_p = (int32_t)(((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4));
  *adc_t = (int32_t)(((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4));
  *adc_h = (int32_t)(((uint32_t)d[6] << 8) | d[7]);
}

int32_t bme280_compensar_temp(bme280_t *dev, int32_t adc_t)
{
  const bme280_calib_t *c = &dev->calib;
  int32_t var1, var2;

  var1 = ((((adc_t >> 3) - ((int32_t)c->T1 << 1))) * ((int32_t)c->T2)) >> 11;
  var2 = (((((adc_t >> 4) - ((int32_t)c->T1)) * ((adc_t >> 4) - ((int32_t)c->T1))) >> 12)
          * ((int32_t)c->T3)) >> 14;
  dev->t_fine = var1 + var2;
  return (dev->t_fine * 5 + 128) >> 8;
}

uint32_t bme280_compensar_press(const bme280_t *dev, int32_t adc_p)
{
  const bme280_calib_t *c = &dev->calib;
  int64_t var1, var2, p;

  var1 = ((int64_t)dev->t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)c->P6;
  var2 = var2 + ((var1 * (int64_t)c->P5) * 131072);          /* << 17 */
  var2 = var2 + (((int64_t)c->P4) * 34359738368LL);          /* << 35 */
  var1 = ((var1 * var1 * (int64_t)c->P3) >> 8) + ((var1 * (int64_t)c->P2) * 4096);
  var1 = ((((int64_t)1) << 47) + var1) * ((int64_t)c->P1) >> 33;
  if (var1 == 0)
  {
    return 0; /* evita divisão por zero */
  }
  p = 1048576 - adc_p;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)c->P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)c->P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)c->P7) * 16);
  return (uint32_t)p;
}

uint32_t bme280_compensar_umid(const bme280_t *dev, int32_t adc_h)
{
  const bme280_calib_t *c = &dev->calib;
  int32_t v;

  v = dev->t_fine - ((int32_t)76800);
  v = (((((adc_h << 14) - (((int32_t)c->H4) * 1048576) - (((int32_t)c->H5) * v)) + ((int32_t)16384)) >> 15)
       * (((((((v * ((int32_t)c->H6)) >> 10) * (((v * ((int32_t)c->H3)) >> 11) + ((int32_t)32768))) >> 10)
            + ((int32_t)2097152)) * ((int32_t)c->H2) + 8192) >> 14));
  v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)c->H1)) >> 4));
  v = (v < 0) ? 0 : v;
  v = (v > 419430400) ? 419430400 : v;
  return (uint32_t)(v >> 12);
}

bme280_status_t bme280_iniciar(bme280_t *dev)
{
  uint8_t id = 0;
  uint8_t c1[26];
  uint8_t c2[7];
  uint8_t status = 0;

  if (dev->ler(dev->ctx, BME280_REG_CHIP_ID, &id, 1) != 0)
  {
    return BME280_ERRO_BARRAMENTO;
  }
  if (id != BME280_CHIP_ID)
  {
    return BME280_ERRO_ID;
  }

  if (dev->escrever(dev->ctx, BME280_REG_RESET, BME280_CMD_RESET) != 0)
  {
    return BME280_ERRO_BARRAMENTO;
  }
  dev->atraso_ms(3); /* tempo de partida: 2 ms (datasheet, tabela 1) */

  /* Aguarda o fim da cópia da NVM para os registradores (bit im_update) */
  for (int tentativas = 0; tentativas < 10; tentativas++)
  {
    if (dev->ler(dev->ctx, BME280_REG_STATUS, &status, 1) != 0)
    {
      return BME280_ERRO_BARRAMENTO;
    }
    if ((status & 0x01u) == 0)
    {
      break;
    }
    dev->atraso_ms(1);
  }

  if (dev->ler(dev->ctx, BME280_REG_CALIB00, c1, sizeof(c1)) != 0 ||
      dev->ler(dev->ctx, BME280_REG_CALIB26, c2, sizeof(c2)) != 0)
  {
    return BME280_ERRO_BARRAMENTO;
  }
  bme280_decodificar_calib(c1, c2, &dev->calib);

  /* ctrl_hum só passa a valer depois de uma escrita em ctrl_meas */
  if (dev->escrever(dev->ctx, BME280_REG_CTRL_HUM, CTRL_HUM_VALOR) != 0 ||
      dev->escrever(dev->ctx, BME280_REG_CONFIG, CONFIG_VALOR) != 0 ||
      dev->escrever(dev->ctx, BME280_REG_CTRL_MEAS, CTRL_MEAS_VALOR) != 0)
  {
    return BME280_ERRO_BARRAMENTO;
  }
  return BME280_OK;
}

bme280_status_t bme280_ler(bme280_t *dev, bme280_leitura_t *saida)
{
  uint8_t d[8];
  int32_t adc_p, adc_t, adc_h;

  /* Leitura em rajada: garante que P, T e H são da mesma medição */
  if (dev->ler(dev->ctx, BME280_REG_DADOS, d, sizeof(d)) != 0)
  {
    return BME280_ERRO_BARRAMENTO;
  }
  bme280_decodificar_dados(d, &adc_p, &adc_t, &adc_h);
  if (adc_t == BME280_ADC_INVALIDO_20B || adc_p == BME280_ADC_INVALIDO_20B ||
      adc_h == BME280_ADC_INVALIDO_16B)
  {
    /* Valores de reset: o sensor reiniciou ou ainda não mediu */
    return BME280_ERRO_DADO_INVALIDO;
  }

  saida->temp_c_x100    = bme280_compensar_temp(dev, adc_t); /* calcula t_fine */
  saida->press_pa_q24_8 = bme280_compensar_press(dev, adc_p);
  saida->umid_q22_10    = bme280_compensar_umid(dev, adc_h);
  return BME280_OK;
}
