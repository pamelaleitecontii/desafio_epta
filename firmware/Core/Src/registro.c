/**
 * @file    registro.c
 * @brief   Gravação dos arquivos CSV no microSD.
 */
#include "registro.h"

#include <string.h>

#include "datahora.h"
#include "fatfs.h"
#include "ff_gen_drv.h"
#include "rtc.h"
#include "sdio.h"

#define ESPACO_MINIMO_KB   (4u * 1024u)   /* abaixo disso, o cartão é tratado como cheio */
#define MAX_ARQUIVOS       1000u

extern Disk_drvTypeDef disk;  /* ff_gen_drv.c */

static FIL  arquivo;
static int  arquivo_aberto;
static int  montado;
static char nome[13];

reg_status_t registro_montar(void)
{
  /* disk_initialize() do diskio.c gerado só inicializa o cartão uma vez.
   * Depois de uma troca de cartão, é preciso zerar essa marca e reiniciar
   * o periférico SDIO para que o novo cartão seja identificado. */
  disk.is_initialized[0] = 0;
  HAL_SD_DeInit(&hsd);

  if (f_mount(&SDFatFS, SDPath, 1) != FR_OK)
  {
    montado = 0;
    return REG_ERRO;
  }
  montado = 1;
  return REG_OK;
}

void registro_desmontar(void)
{
  if (arquivo_aberto)
  {
    registro_fechar();
  }
  f_mount(NULL, SDPath, 0);
  montado = 0;
  disk.is_initialized[0] = 0;
}

int registro_acertar_rtc_por_arquivo(void)
{
  FIL f;
  char txt[32] = {0};
  UINT lidos = 0;
  datahora_t dh;

  if (!montado || f_open(&f, "RTC.TXT", FA_READ) != FR_OK)
  {
    return 0;
  }
  f_read(&f, txt, sizeof(txt) - 1, &lidos);
  f_close(&f);

  if (lidos < 19 || datahora_analisar(txt, &dh) != 0)
  {
    return 0;
  }

  RTC_TimeTypeDef hora = {0};
  RTC_DateTypeDef data = {0};
  hora.Hours = dh.hora;
  hora.Minutes = dh.min;
  hora.Seconds = dh.seg;
  hora.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  hora.StoreOperation = RTC_STOREOPERATION_RESET;
  data.Year = (uint8_t)(dh.ano - 2000u);
  data.Month = dh.mes;
  data.Date = dh.dia;
  data.WeekDay = datahora_dia_semana(dh.ano, dh.mes, dh.dia);

  if (HAL_RTC_SetTime(&hrtc, &hora, RTC_FORMAT_BIN) != HAL_OK ||
      HAL_RTC_SetDate(&hrtc, &data, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 0;
  }
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_AJUSTADO);

  /* Apaga o arquivo para não voltar o relógio a cada energização */
  f_unlink("RTC.TXT");
  return 1;
}

static uint32_t espaco_livre_kb(void)
{
  FATFS *fs;
  DWORD clusters_livres = 0;

  if (f_getfree(SDPath, &clusters_livres, &fs) != FR_OK)
  {
    return 0;
  }
  /* setores de 512 bytes: kB = clusters * setores_por_cluster / 2 */
  return (uint32_t)(clusters_livres * fs->csize / 2u);
}

reg_status_t registro_abrir(const char *cabecalho)
{
  FILINFO info;
  uint32_t i;

  if (!montado)
  {
    return REG_ERRO;
  }
  if (espaco_livre_kb() < ESPACO_MINIMO_KB)
  {
    return REG_CHEIO;
  }

  /* Procura o primeiro LOGnnn.CSV que ainda não existe */
  for (i = 0; i < MAX_ARQUIVOS; i++)
  {
    memcpy(nome, "LOG000.CSV", 11);
    nome[3] = (char)('0' + (i / 100u) % 10u);
    nome[4] = (char)('0' + (i / 10u) % 10u);
    nome[5] = (char)('0' + i % 10u);
    FRESULT r = f_stat(nome, &info);
    if (r == FR_NO_FILE)
    {
      break;
    }
    if (r != FR_OK)
    {
      return REG_ERRO;
    }
  }
  if (i == MAX_ARQUIVOS)
  {
    return REG_CHEIO;
  }

  if (f_open(&arquivo, nome, FA_CREATE_NEW | FA_WRITE) != FR_OK)
  {
    return REG_ERRO;
  }
  arquivo_aberto = 1;

  reg_status_t r = registro_escrever(cabecalho, (uint32_t)strlen(cabecalho));
  if (r == REG_OK)
  {
    r = registro_sincronizar();
  }
  return r;
}

reg_status_t registro_escrever(const void *dados, uint32_t n)
{
  UINT escritos = 0;

  if (!arquivo_aberto)
  {
    return REG_ERRO;
  }
  if (f_write(&arquivo, dados, n, &escritos) != FR_OK)
  {
    return REG_ERRO;
  }
  /* O FatFs devolve FR_OK com menos bytes escritos quando o disco enche */
  return (escritos < n) ? REG_CHEIO : REG_OK;
}

reg_status_t registro_sincronizar(void)
{
  if (!arquivo_aberto)
  {
    return REG_ERRO;
  }
  return (f_sync(&arquivo) == FR_OK) ? REG_OK : REG_ERRO;
}

void registro_fechar(void)
{
  if (arquivo_aberto)
  {
    f_close(&arquivo);
    arquivo_aberto = 0;
  }
}

uint32_t registro_tamanho(void)
{
  return arquivo_aberto ? (uint32_t)f_size(&arquivo) : 0u;
}

const char *registro_nome_arquivo(void)
{
  return nome;
}
