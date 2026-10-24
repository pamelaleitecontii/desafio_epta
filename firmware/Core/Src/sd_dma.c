/**
 * @file    sd_dma.c
 * @brief   Leitura e escrita de blocos do microSD por DMA.
 *
 * O sd_diskio.c gerado pelo CubeMX chama BSP_SD_ReadBlocks/WriteBlocks, que
 * no bsp_driver_sd.c gerado usam o modo polling. Em polling, a CPU precisa
 * alimentar a FIFO do SDIO sem interrupção: a interrupção do IMU (416 por
 * segundo) causaria erros de TX underrun / RX overrun. As funções abaixo
 * substituem as versões __weak e usam o DMA que já está configurado
 * (DMA2 Stream3 = RX, Stream6 = TX). Por substituírem funções __weak, não
 * são apagadas quando o código é regenerado no CubeMX.
 */
#include <string.h>

#include "bsp_driver_sd.h"
#include "main.h"
#include "sdio.h"

#define BLOCO                 512u
/* Uma escrita pode deixar o cartão ocupado por até 250 ms (SDHC) ou 500 ms
 * (SDXC), segundo a especificação SD. 1 s dá margem sem travar o sistema. */
#define SD_DMA_TIMEOUT_MS     1000u

static volatile uint8_t tx_fim;
static volatile uint8_t rx_fim;
static volatile uint8_t erro_dma;

/* O DMA está configurado com acesso de palavra (32 bits): o buffer precisa
 * estar alinhado em 4 bytes. Buffers do FatFs que não estiverem alinhados
 * passam por este buffer intermediário, um bloco por vez. */
static uint32_t intermediario[BLOCO / 4];

void BSP_SD_WriteCpltCallback(void) { tx_fim = 1; }
void BSP_SD_ReadCpltCallback(void)  { rx_fim = 1; }
void HAL_SD_ErrorCallback(SD_HandleTypeDef *h) { (void)h; erro_dma = 1; }

static uint8_t aguardar(volatile uint8_t *fim)
{
  uint32_t inicio = HAL_GetTick();

  while (!*fim && !erro_dma)
  {
    if (HAL_GetTick() - inicio > SD_DMA_TIMEOUT_MS)
    {
      HAL_SD_Abort(&hsd);
      return MSD_ERROR;
    }
  }
  if (erro_dma)
  {
    return MSD_ERROR;
  }
  /* Fim do DMA não é fim da gravação: o cartão continua programando a
   * flash internamente até voltar ao estado TRANSFER. */
  while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER)
  {
    if (HAL_GetTick() - inicio > SD_DMA_TIMEOUT_MS)
    {
      return MSD_ERROR;
    }
  }
  return MSD_OK;
}

static uint8_t ler_dma(uint32_t *dados, uint32_t bloco, uint32_t n)
{
  rx_fim = 0;
  erro_dma = 0;
  if (HAL_SD_ReadBlocks_DMA(&hsd, (uint8_t *)dados, bloco, n) != HAL_OK)
  {
    return MSD_ERROR;
  }
  return aguardar(&rx_fim);
}

static uint8_t escrever_dma(const uint32_t *dados, uint32_t bloco, uint32_t n)
{
  tx_fim = 0;
  erro_dma = 0;
  if (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t *)dados, bloco, n) != HAL_OK)
  {
    return MSD_ERROR;
  }
  return aguardar(&tx_fim);
}

uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
  (void)Timeout;

  if (((uintptr_t)pData & 3u) == 0)
  {
    return ler_dma(pData, ReadAddr, NumOfBlocks);
  }
  for (uint32_t i = 0; i < NumOfBlocks; i++)
  {
    if (ler_dma(intermediario, ReadAddr + i, 1) != MSD_OK)
    {
      return MSD_ERROR;
    }
    memcpy((uint8_t *)pData + i * BLOCO, intermediario, BLOCO);
  }
  return MSD_OK;
}

uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
  (void)Timeout;

  if (((uintptr_t)pData & 3u) == 0)
  {
    return escrever_dma(pData, WriteAddr, NumOfBlocks);
  }
  for (uint32_t i = 0; i < NumOfBlocks; i++)
  {
    memcpy(intermediario, (const uint8_t *)pData + i * BLOCO, BLOCO);
    if (escrever_dma(intermediario, WriteAddr + i, 1) != MSD_OK)
    {
      return MSD_ERROR;
    }
  }
  return MSD_OK;
}
