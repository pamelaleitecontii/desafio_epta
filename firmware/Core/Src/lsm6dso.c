/**
 * @file    lsm6dso.c
 * @brief   Driver do LSM6DSO. Registradores conforme o datasheet da ST
 *          (DS12140). Também funciona com o LSM6DSL (DS11939).
 */
#include "lsm6dso.h"

#define CTRL3_C_SW_RESET   0x01u
#define CTRL3_C_IF_INC     0x04u
#define CTRL3_C_BDU        0x40u

#define ODR_416HZ          (0x6u << 4)
#define FS_XL_16G          (0x1u << 2)
#define FS_G_2000DPS       (0x3u << 2)
#define INT1_DRDY_XL       0x01u
#define CTRL9_XL_PADRAO    0xE0u
#define CTRL9_XL_I3C_OFF   0x02u   /* só existe no LSM6DSO */

static int who_am_i_valido(uint8_t id)
{
  return id == LSM6DSO_WHO_AM_I_VALOR || id == LSM6DSL_WHO_AM_I_VALOR;
}

lsm6dso_status_t lsm6dso_verificar(lsm6dso_t *dev)
{
  uint8_t id = 0;

  if (dev->ler(dev->ctx, LSM6DSO_REG_WHO_AM_I, &id, 1) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  /* MISO solto lê 0x00 ou 0xFF: os dois são rejeitados aqui */
  return who_am_i_valido(id) ? LSM6DSO_OK : LSM6DSO_ERRO_ID;
}

lsm6dso_status_t lsm6dso_iniciar(lsm6dso_t *dev)
{
  uint8_t ctrl3 = 0;
  int pronto = 0;

  if (dev->ler(dev->ctx, LSM6DSO_REG_WHO_AM_I, &dev->who_am_i, 1) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  if (!who_am_i_valido(dev->who_am_i))
  {
    return LSM6DSO_ERRO_ID;
  }

  /* Reset por software: volta todos os registradores ao padrão */
  if (dev->escrever(dev->ctx, LSM6DSO_REG_CTRL3_C, CTRL3_C_SW_RESET) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  for (int i = 0; i < 20; i++)
  {
    dev->atraso_ms(1);
    if (dev->ler(dev->ctx, LSM6DSO_REG_CTRL3_C, &ctrl3, 1) != 0)
    {
      return LSM6DSO_ERRO_BARRAMENTO;
    }
    if ((ctrl3 & CTRL3_C_SW_RESET) == 0)
    {
      pronto = 1;
      break;
    }
  }
  if (!pronto)
  {
    return LSM6DSO_ERRO_TIMEOUT;
  }

  /* BDU: parte alta e baixa de cada eixo sempre da mesma amostra.
   * IF_INC: endereço incrementa sozinho na leitura em rajada. */
  if (dev->escrever(dev->ctx, LSM6DSO_REG_CTRL3_C, CTRL3_C_BDU | CTRL3_C_IF_INC) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  if (dev->who_am_i == LSM6DSO_WHO_AM_I_VALOR &&
      dev->escrever(dev->ctx, LSM6DSO_REG_CTRL9_XL, CTRL9_XL_PADRAO | CTRL9_XL_I3C_OFF) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  /* DRDY do acelerômetro no INT1 (PB0). Como acelerômetro e giroscópio usam
   * o mesmo ODR, um único sinal basta para os dois. O sinal fica em nível
   * alto até a amostra ser lida (modo "latched", o padrão). */
  if (dev->escrever(dev->ctx, LSM6DSO_REG_INT1_CTRL, INT1_DRDY_XL) != 0 ||
      dev->escrever(dev->ctx, LSM6DSO_REG_CTRL2_G, ODR_416HZ | FS_G_2000DPS) != 0 ||
      dev->escrever(dev->ctx, LSM6DSO_REG_CTRL1_XL, ODR_416HZ | FS_XL_16G) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  return lsm6dso_verificar(dev);
}

void lsm6dso_decodificar(const uint8_t d[LSM6DSO_BYTES_AMOSTRA], lsm6dso_bruto_t *saida)
{
  for (int i = 0; i < 3; i++)
  {
    saida->giro[i] = (int16_t)((uint16_t)d[2 * i + 1] << 8 | d[2 * i]);
    saida->acel[i] = (int16_t)((uint16_t)d[2 * i + 7] << 8 | d[2 * i + 6]);
  }
}

lsm6dso_status_t lsm6dso_ler(lsm6dso_t *dev, lsm6dso_bruto_t *saida)
{
  uint8_t d[LSM6DSO_BYTES_AMOSTRA];

  if (dev->ler(dev->ctx, LSM6DSO_REG_OUTX_L_G, d, sizeof(d)) != 0)
  {
    return LSM6DSO_ERRO_BARRAMENTO;
  }
  lsm6dso_decodificar(d, saida);
  return LSM6DSO_OK;
}

int32_t lsm6dso_acel_g_x10000(int16_t bruto)
{
  /* bruto * 488 está em µg; divide por 100 para 10^-4 g, arredondando */
  int32_t ug = (int32_t)bruto * LSM6DSO_SENS_ACEL_UG;
  return (ug >= 0) ? (ug + 50) / 100 : (ug - 50) / 100;
}

int32_t lsm6dso_giro_mdps(int16_t bruto)
{
  return (int32_t)bruto * LSM6DSO_SENS_GIRO_MDPS;
}
