/**
 * @file    tempo.c
 * @brief   Base de tempo em microssegundos.
 *
 * O TIM2 tem 32 bits e reinicia a contagem a cada 2^32 µs (cerca de 71 min).
 * O voo dura poucos minutos, mas a placa pode ficar ligada na rampa por
 * horas antes do lançamento. Por isso o contador é estendido para 64 bits
 * em software: cada leitura compara com a anterior e conta as voltas. Basta
 * que tempo_us() seja chamada pelo menos uma vez a cada 71 minutos, o que o
 * laço principal e a interrupção do IMU fazem centenas de vezes por segundo.
 */
#include "tempo.h"
#include "tim.h"

static uint32_t ultima_leitura;
static uint32_t voltas;

void tempo_iniciar(void)
{
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  HAL_TIM_Base_Start(&htim2);
}

uint64_t tempo_us(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  uint32_t agora = __HAL_TIM_GET_COUNTER(&htim2);
  if (agora < ultima_leitura)
  {
    voltas++;
  }
  ultima_leitura = agora;
  uint64_t t = ((uint64_t)voltas << 32) | agora;

  __set_PRIMASK(primask);
  return t;
}

void tempo_esperar_us(uint32_t us)
{
  uint32_t inicio = __HAL_TIM_GET_COUNTER(&htim2);
  while ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - inicio) < us)
  {
  }
}
