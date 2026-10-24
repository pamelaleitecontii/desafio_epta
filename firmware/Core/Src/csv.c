/**
 * @file    csv.c
 * @brief   Formatação das linhas do CSV.
 *
 * O printf do newlib-nano não formata float por padrão, e mesmo com float
 * seria lento demais para 416 linhas por segundo. Por isso os valores são
 * mantidos em inteiros de ponto fixo e convertidos para texto aqui.
 */
#include "csv.h"
#include "lsm6dso.h"
#include <string.h>

static const char CABECALHO[] =
    "t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_C,press_Pa,umid_pct,bme_novo,perdidas\r\n";

size_t csv_u64(char *buf, uint64_t valor)
{
  char tmp[20];
  size_t n = 0;

  do
  {
    tmp[n++] = (char)('0' + (valor % 10u));
    valor /= 10u;
  } while (valor != 0u);

  for (size_t i = 0; i < n; i++)
  {
    buf[i] = tmp[n - 1 - i];
  }
  return n;
}

size_t csv_fixo(char *buf, int64_t valor, unsigned casas)
{
  uint64_t abs_valor;
  uint64_t divisor = 1;
  size_t n = 0;

  for (unsigned i = 0; i < casas; i++)
  {
    divisor *= 10u;
  }
  if (valor < 0)
  {
    buf[n++] = '-';
    abs_valor = (uint64_t)(-(valor + 1)) + 1u; /* evita overflow em INT64_MIN */
  }
  else
  {
    abs_valor = (uint64_t)valor;
  }

  n += csv_u64(&buf[n], abs_valor / divisor);
  if (casas > 0)
  {
    uint64_t frac = abs_valor % divisor;
    buf[n++] = '.';
    for (unsigned i = casas; i > 0; i--)
    {
      uint64_t peso = 1;
      for (unsigned j = 1; j < i; j++)
      {
        peso *= 10u;
      }
      buf[n++] = (char)('0' + (frac / peso) % 10u);
    }
  }
  return n;
}

size_t csv_cabecalho(char *buf, size_t tam)
{
  size_t n = sizeof(CABECALHO) - 1;

  if (tam < n)
  {
    return 0;
  }
  memcpy(buf, CABECALHO, n);
  return n;
}

size_t csv_formatar(char *buf, size_t tam, const csv_linha_t *l)
{
  char linha[CSV_LINHA_MAX];
  size_t n = 0;

  n += csv_u64(&linha[n], l->t_us);

  for (int i = 0; i < 3; i++)
  {
    linha[n++] = ',';
    if (l->tem_imu)
    {
      n += csv_fixo(&linha[n], lsm6dso_acel_g_x10000(l->acel[i]), 4);
    }
  }
  for (int i = 0; i < 3; i++)
  {
    linha[n++] = ',';
    if (l->tem_imu)
    {
      n += csv_fixo(&linha[n], lsm6dso_giro_mdps(l->giro[i]), 3);
    }
  }

  linha[n++] = ',';
  if (l->tem_bme)
  {
    n += csv_fixo(&linha[n], l->temp_c_x100, 2);
  }
  linha[n++] = ',';
  if (l->tem_bme)
  {
    /* Converte de Q24.8 para Pa com 2 casas, arredondando */
    int64_t pa_x100 = ((int64_t)l->press_pa_q24_8 * 100 + 128) / 256;
    n += csv_fixo(&linha[n], pa_x100, 2);
  }
  linha[n++] = ',';
  if (l->tem_bme)
  {
    /* Converte de Q22.10 para % com 2 casas, arredondando */
    int64_t pct_x100 = ((int64_t)l->umid_q22_10 * 100 + 512) / 1024;
    n += csv_fixo(&linha[n], pct_x100, 2);
  }

  linha[n++] = ',';
  linha[n++] = l->bme_novo ? '1' : '0';
  linha[n++] = ',';
  n += csv_u64(&linha[n], l->perdidas);
  linha[n++] = '\r';
  linha[n++] = '\n';

  if (n > tam)
  {
    return 0;
  }
  memcpy(buf, linha, n);
  return n;
}
