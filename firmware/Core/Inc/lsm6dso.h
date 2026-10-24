/**
 * @file    lsm6dso.h
 * @brief   Driver do IMU LSM6DSO (acelerômetro + giroscópio) via SPI.
 *
 * O esquemático usa o símbolo do LSM6DSL com o valor "LSM6DSO". Os dois
 * chips têm a mesma pinagem LGA-14 e o mesmo mapa para os registradores
 * usados aqui; por isso o driver aceita os dois WHO_AM_I (0x6C e 0x6A).
 *
 * Assim como o driver do BME280, não depende da HAL.
 */
#ifndef LSM6DSO_H
#define LSM6DSO_H

#include <stdint.h>

#define LSM6DSO_WHO_AM_I_VALOR  0x6Cu
#define LSM6DSL_WHO_AM_I_VALOR  0x6Au

#define LSM6DSO_REG_INT1_CTRL   0x0Du
#define LSM6DSO_REG_WHO_AM_I    0x0Fu
#define LSM6DSO_REG_CTRL1_XL    0x10u
#define LSM6DSO_REG_CTRL2_G     0x11u
#define LSM6DSO_REG_CTRL3_C     0x12u
#define LSM6DSO_REG_CTRL9_XL    0x18u
#define LSM6DSO_REG_OUTX_L_G    0x22u  /* 0x22..0x27 giro, 0x28..0x2D acel */

#define LSM6DSO_BYTES_AMOSTRA   12u

/* Escalas configuradas: ±16 g e ±2000 dps (voo de foguete) */
#define LSM6DSO_SENS_ACEL_UG    488    /* 0,488 mg/LSB em ±16 g  (datasheet, tabela 2) */
#define LSM6DSO_SENS_GIRO_MDPS  70     /* 70 mdps/LSB em ±2000 dps                     */
#define LSM6DSO_ODR_HZ          416

typedef int (*lsm6dso_ler_fn)(void *ctx, uint8_t reg, uint8_t *dados, uint16_t n);
typedef int (*lsm6dso_escrever_fn)(void *ctx, uint8_t reg, uint8_t valor);
typedef void (*lsm6dso_atraso_fn)(uint32_t ms);

typedef struct
{
  lsm6dso_ler_fn      ler;
  lsm6dso_escrever_fn escrever;
  lsm6dso_atraso_fn   atraso_ms;
  void               *ctx;
  uint8_t             who_am_i;  /* valor lido na inicialização */
} lsm6dso_t;

typedef struct
{
  int16_t giro[3];  /* x, y, z: valores brutos */
  int16_t acel[3];
} lsm6dso_bruto_t;

typedef enum
{
  LSM6DSO_OK = 0,
  LSM6DSO_ERRO_BARRAMENTO,
  LSM6DSO_ERRO_ID,
  LSM6DSO_ERRO_TIMEOUT,
} lsm6dso_status_t;

/** Reset por software, conferência do WHO_AM_I e configuração (416 Hz, ±16 g, ±2000 dps). */
lsm6dso_status_t lsm6dso_iniciar(lsm6dso_t *dev);

/** Confere apenas o WHO_AM_I (usado na supervisão periódica). */
lsm6dso_status_t lsm6dso_verificar(lsm6dso_t *dev);

/** Lê giroscópio e acelerômetro em uma única rajada (12 bytes). Limpa o DRDY. */
lsm6dso_status_t lsm6dso_ler(lsm6dso_t *dev, lsm6dso_bruto_t *saida);

/* Funções puras, sem acesso ao barramento */
void    lsm6dso_decodificar(const uint8_t d[LSM6DSO_BYTES_AMOSTRA], lsm6dso_bruto_t *saida);
/** Aceleração em 10^-4 g (ex.: 10000 = 1 g), com arredondamento. */
int32_t lsm6dso_acel_g_x10000(int16_t bruto);
/** Velocidade angular em mdps (10^-3 graus por segundo). */
int32_t lsm6dso_giro_mdps(int16_t bruto);

#endif /* LSM6DSO_H */
