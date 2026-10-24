/**
 * @file    datalogger.c
 * @brief   Aplicação do datalogger.
 *
 * Como funciona: o IMU gera uma interrupção no PB0 (pino INT1) a cada
 * amostra, 416 vezes por segundo. A própria interrupção lê a amostra pelo
 * SPI, o que leva uns 30 µs, e guarda o dado numa fila circular junto com o
 * tempo em µs. Assim nenhuma amostra se perde enquanto o laço principal
 * está ocupado gravando no cartão, já que uma escrita no microSD pode levar
 * centenas de milissegundos.
 *
 * O laço principal lê o BME280 a cada 40 ms, tira as amostras da fila,
 * monta as linhas do CSV num buffer e grava em blocos de 4 kB. Uma máquina
 * de estados cuida do cartão (ausente, removido, cheio ou com erro), e os
 * sensores são vigiados e reiniciados quando falham, sem parar a gravação
 * dos que continuam funcionando.
 */
#include "datalogger.h"

#include <stdio.h>
#include <string.h>

#include "bme280.h"
#include "bsp_driver_sd.h"
#include "csv.h"
#include "datahora.h"
#include "i2c.h"
#include "lsm6dso.h"
#include "main.h"
#include "registro.h"
#include "rtc.h"
#include "spi.h"
#include "tempo.h"

#if CFG_DEBUG_UART
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

#if (CFG_FILA_IMU & (CFG_FILA_IMU - 1)) != 0
#error "CFG_FILA_IMU precisa ser potência de 2"
#endif
#if CFG_LIMIAR_ESCRITA + CSV_LINHA_MAX > CFG_BUFFER_TEXTO
#error "CFG_BUFFER_TEXTO pequeno demais para o limiar de escrita"
#endif

typedef enum
{
  EST_SEM_CARTAO = 0,  /* aguardando cartão (ou tentando montar)  */
  EST_GRAVANDO,
  EST_CARTAO_CHEIO,
  EST_PARADO,          /* gravação encerrada pelo botão            */
} estado_t;

typedef struct
{
  uint64_t        t_us;
  lsm6dso_bruto_t dado;
} amostra_imu_t;

/* Fila do IMU: quem escreve é a interrupção, quem lê é o laço principal */
static amostra_imu_t     fila[CFG_FILA_IMU];
static volatile uint32_t fila_entrada;     /* escrito só pela interrupção  */
static volatile uint32_t fila_saida;       /* escrito só pelo laço         */
static volatile uint32_t perdidas_pendentes;
static volatile uint32_t perdidas_total;
static volatile uint32_t erros_spi_interrupcao;
static volatile uint32_t ultimo_dado_imu_ms;

/* Sensores */
static lsm6dso_t         imu;
static bme280_t          bme;
static volatile uint8_t  imu_ok;
static uint8_t           bme_ok;
static uint8_t           bme_valida;
static uint8_t           bme_novo;
static uint32_t          bme_falhas;
static bme280_leitura_t  bme_ultima;
static uint32_t          recuperacoes_imu;

/* Gravação */
static estado_t estado;
static char     texto[CFG_BUFFER_TEXTO] __attribute__((aligned(4)));
static uint32_t texto_len;
static const char *causa_reset = "desconhecida";

/* Temporizações (HAL_GetTick, em ms) */
static uint32_t t_bme;
static uint32_t t_sync;
static uint32_t t_ultima_linha;
static uint32_t t_tentativa_imu;
static uint32_t t_tentativa_bme;
static uint32_t t_verificacao_imu;
static uint32_t t_tentativa_cartao;

/* ========================================================================= */
/* Barramentos                                                               */
/* ========================================================================= */

static int imu_spi_ler(void *ctx, uint8_t reg, uint8_t *dados, uint16_t n)
{
  uint8_t tx[1 + LSM6DSO_BYTES_AMOSTRA] = {0};
  uint8_t rx[1 + LSM6DSO_BYTES_AMOSTRA];
  HAL_StatusTypeDef st;
  (void)ctx;

  if (n > LSM6DSO_BYTES_AMOSTRA)
  {
    return -1;
  }
  tx[0] = reg | 0x80u; /* bit 7 = leitura */
  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
  st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, (uint16_t)(n + 1), 5);
  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
  memcpy(dados, &rx[1], n);
  return (st == HAL_OK) ? 0 : -1;
}

static int imu_spi_escrever(void *ctx, uint8_t reg, uint8_t valor)
{
  uint8_t tx[2] = {(uint8_t)(reg & 0x7Fu), valor};
  uint8_t rx[2];
  HAL_StatusTypeDef st;
  (void)ctx;

  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
  st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 5);
  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
  return (st == HAL_OK) ? 0 : -1;
}

static int bme_i2c_ler(void *ctx, uint8_t reg, uint8_t *dados, uint16_t n)
{
  (void)ctx;
  return (HAL_I2C_Mem_Read(&hi2c1, BME280_ENDERECO_I2C << 1, reg, I2C_MEMADD_SIZE_8BIT,
                           dados, n, 10) == HAL_OK) ? 0 : -1;
}

static int bme_i2c_escrever(void *ctx, uint8_t reg, uint8_t valor)
{
  (void)ctx;
  return (HAL_I2C_Mem_Write(&hi2c1, BME280_ENDERECO_I2C << 1, reg, I2C_MEMADD_SIZE_8BIT,
                            &valor, 1, 10) == HAL_OK) ? 0 : -1;
}

static void atraso_ms(uint32_t ms)
{
  HAL_Delay(ms);
}

/**
 * Libera o barramento I²C quando um escravo ficou segurando o SDA em nível
 * baixo (por exemplo, após um reset no meio de uma leitura): gera 9 pulsos
 * de clock e uma condição de STOP, e então reinicia o periférico. Também
 * resolve o travamento do bit BUSY do I2C do STM32F4 (errata ES0287).
 */
static void i2c_recuperar(void)
{
  GPIO_InitTypeDef g = {0};

  HAL_I2C_DeInit(&hi2c1);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_SET);
  g.Pin = GPIO_PIN_8 | GPIO_PIN_9; /* PB8 = SCL, PB9 = SDA */
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);

  for (int i = 0; i < 9; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    tempo_esperar_us(5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    tempo_esperar_us(5);
  }
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
  tempo_esperar_us(5);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET); /* STOP: SDA sobe com SCL alto */
  tempo_esperar_us(5);

  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();
  MX_I2C1_Init();
}

/* ========================================================================= */
/* IMU                                                                       */
/* ========================================================================= */

void datalogger_imu_interrupcao(void)
{
  amostra_imu_t a;
  uint32_t entrada;

  if (!imu_ok)
  {
    return; /* driver ainda não configurado */
  }
  a.t_us = tempo_us();
  if (lsm6dso_ler(&imu, &a.dado) != LSM6DSO_OK)
  {
    erros_spi_interrupcao++;
    return;
  }
  ultimo_dado_imu_ms = HAL_GetTick();

  entrada = fila_entrada;
  if (entrada - fila_saida >= CFG_FILA_IMU)
  {
    /* Fila cheia: o cartão travou por mais de ~5 s. Descarta a amostra e
     * registra a perda na coluna "perdidas" da próxima linha gravada. */
    perdidas_pendentes++;
    perdidas_total++;
    return;
  }
  fila[entrada & (CFG_FILA_IMU - 1u)] = a;
  __DMB();
  fila_entrada = entrada + 1u;
}

static int fila_retirar(amostra_imu_t *a)
{
  uint32_t saida = fila_saida;

  if (saida == fila_entrada)
  {
    return 0;
  }
  *a = fila[saida & (CFG_FILA_IMU - 1u)];
  __DMB();
  fila_saida = saida + 1u;
  return 1;
}

static void fila_descartar(void)
{
  fila_saida = fila_entrada;
  __disable_irq();
  perdidas_pendentes = 0;
  __enable_irq();
}

/* Todo acesso ao SPI fora da interrupção é feito com a EXTI do IMU desligada */
static void imu_interrupcao_desligar(void)
{
  HAL_NVIC_DisableIRQ(IMU_INT1_EXTI_IRQn);
  __DSB();
  __ISB();
}

static void imu_interrupcao_religar(void)
{
  lsm6dso_bruto_t descarte;

  /* O DRDY fica em nível alto até a amostra ser lida. Se ficou alto enquanto
   * a interrupção estava desligada, não haverá nova borda de subida e a
   * aquisição pararia. Limpa a pendência e lê uma amostra para baixar o
   * sinal; se outra chegar depois disso, a borda fica pendente e dispara. */
  __HAL_GPIO_EXTI_CLEAR_IT(IMU_INT1_Pin);
  lsm6dso_ler(&imu, &descarte);
  ultimo_dado_imu_ms = HAL_GetTick();
  HAL_NVIC_EnableIRQ(IMU_INT1_EXTI_IRQn);
}

static void imu_tentar_iniciar(void)
{
  imu_interrupcao_desligar();
  imu_ok = 0;
  lsm6dso_status_t st = lsm6dso_iniciar(&imu);
  if (st == LSM6DSO_OK)
  {
    imu_ok = 1;
    imu_interrupcao_religar();
    LOG("IMU ok (WHO_AM_I = 0x%02X)\r\n", imu.who_am_i);
  }
  else
  {
    LOG("IMU: falha na inicializacao (%d)\r\n", (int)st);
  }
}

static void imu_supervisionar(uint32_t agora)
{
  if (!imu_ok)
  {
    if (agora - t_tentativa_imu >= CFG_RETENTATIVA_MS)
    {
      t_tentativa_imu = agora;
      imu_tentar_iniciar();
    }
    return;
  }

  /* Comparação com sinal: a interrupção pode atualizar ultimo_dado_imu_ms
   * depois que "agora" foi lido, deixando-o um pouco no futuro. */
  if ((int32_t)(agora - ultimo_dado_imu_ms) > (int32_t)CFG_IMU_SILENCIO_MS)
  {
    /* Sem amostras: o sensor pode ter reiniciado (perdendo a configuração)
     * ou o DRDY ficou preso. Reconfigura do zero. */
    recuperacoes_imu++;
    LOG("IMU: sem dados, reiniciando\r\n");
    t_tentativa_imu = agora;
    imu_tentar_iniciar();
    return;
  }

  if (agora - t_verificacao_imu >= CFG_VERIFICACAO_MS)
  {
    t_verificacao_imu = agora;
    imu_interrupcao_desligar();
    if (lsm6dso_verificar(&imu) == LSM6DSO_OK)
    {
      imu_interrupcao_religar();
    }
    else
    {
      imu_ok = 0;
      t_tentativa_imu = agora;
      LOG("IMU: WHO_AM_I invalido, sensor perdido\r\n");
    }
  }
}

/* ========================================================================= */
/* BME280                                                                    */
/* ========================================================================= */

static void bme_tentar_iniciar(void)
{
  bme280_status_t st = bme280_iniciar(&bme);

  bme_ok = (st == BME280_OK);
  bme_valida = 0;
  bme_falhas = 0;
  if (bme_ok)
  {
    LOG("BME280 ok\r\n");
  }
  else
  {
    LOG("BME280: falha na inicializacao (%d)\r\n", (int)st);
  }
}

static void bme_supervisionar(uint32_t agora)
{
  bme280_leitura_t l;

  if (!bme_ok)
  {
    if (agora - t_tentativa_bme >= CFG_RETENTATIVA_MS)
    {
      t_tentativa_bme = agora;
      i2c_recuperar();
      bme_tentar_iniciar();
    }
    return;
  }

  if (agora - t_bme < CFG_PERIODO_BME_MS)
  {
    return;
  }
  t_bme = agora;

  if (bme280_ler(&bme, &l) == BME280_OK)
  {
    bme_ultima = l;
    bme_valida = 1;
    bme_novo = 1;
    bme_falhas = 0;
  }
  else if (++bme_falhas >= CFG_BME_FALHAS_MAX)
  {
    bme_ok = 0;
    bme_valida = 0;
    t_tentativa_bme = agora;
    LOG("BME280: leituras falhando, sensor perdido\r\n");
  }
}

/* ========================================================================= */
/* CSV e cartão                                                              */
/* ========================================================================= */

static void gerar_linha(uint64_t t_us, const lsm6dso_bruto_t *dado)
{
  csv_linha_t l;

  memset(&l, 0, sizeof(l));
  l.t_us = t_us;
  if (dado != NULL)
  {
    l.tem_imu = 1;
    memcpy(l.acel, dado->acel, sizeof(l.acel));
    memcpy(l.giro, dado->giro, sizeof(l.giro));
  }
  if (bme_ok && bme_valida)
  {
    l.tem_bme = 1;
    l.temp_c_x100 = bme_ultima.temp_c_x100;
    l.press_pa_q24_8 = bme_ultima.press_pa_q24_8;
    l.umid_q22_10 = bme_ultima.umid_q22_10;
    l.bme_novo = bme_novo;
  }
  bme_novo = 0;

  __disable_irq();
  l.perdidas = perdidas_pendentes;
  perdidas_pendentes = 0;
  __enable_irq();

  texto_len += (uint32_t)csv_formatar(&texto[texto_len], sizeof(texto) - texto_len, &l);
  t_ultima_linha = HAL_GetTick();
}

static reg_status_t descarregar_texto(void)
{
  reg_status_t r = REG_OK;

  if (texto_len > 0)
  {
    r = registro_escrever(texto, texto_len);
    texto_len = 0;
  }
  return r;
}

static void ler_rtc(datahora_t *dh)
{
  RTC_TimeTypeDef h;
  RTC_DateTypeDef d;

  /* A data precisa ser lida depois da hora para destravar os registradores */
  HAL_RTC_GetTime(&hrtc, &h, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
  dh->ano = (uint16_t)(2000u + d.Year);
  dh->mes = d.Month;
  dh->dia = d.Date;
  dh->hora = h.Hours;
  dh->min = h.Minutes;
  dh->seg = h.Seconds;
}

static void montar_cabecalho(char *buf, size_t tam)
{
  datahora_t dh;
  char data[20];
  char t_inicio[21];
  char imu_txt[40];
  int n;

  ler_rtc(&dh);
  data[datahora_formatar(data, &dh)] = '\0';
  t_inicio[csv_u64(t_inicio, tempo_us())] = '\0';

  if (imu_ok)
  {
    snprintf(imu_txt, sizeof(imu_txt), "ok, WHO_AM_I=0x%02X", imu.who_am_i);
  }
  else
  {
    snprintf(imu_txt, sizeof(imu_txt), "FALHA");
  }

  n = snprintf(buf, tam,
               "# datalogger-epta | firmware " FIRMWARE_VERSAO "\r\n"
               "# inicio (RTC): %s | relogio ajustado: %s | causa do reset: %s\r\n"
               "# t_us = tempo desde a energizacao em microssegundos (arquivo iniciado em t_us=%s)\r\n"
               "# IMU LSM6DSO: %s | faixa do acelerometro 16 g, faixa do giroscopio 2000 dps, 416 amostras por segundo\r\n"
               "# BME280: %s | T x2, P x8, H x1, filtro IIR 4, lido a cada %u ms\r\n"
               "# contadores desde a energizacao: amostras perdidas=%lu, reinicios do IMU=%lu, erros SPI=%lu\r\n"
               "# campo vazio = sensor indisponivel; bme_novo=1 na linha seguinte a cada leitura do BME280\r\n",
               data,
               (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == RTC_BKP_AJUSTADO) ? "sim" : "nao (crie RTC.TXT)",
               causa_reset, t_inicio, imu_txt, bme_ok ? "ok" : "FALHA",
               (unsigned)CFG_PERIODO_BME_MS, (unsigned long)perdidas_total,
               (unsigned long)recuperacoes_imu, (unsigned long)erros_spi_interrupcao);
  if (n > 0 && (size_t)n < tam)
  {
    csv_cabecalho(&buf[n], tam - (size_t)n);
    buf[tam - 1] = '\0';
  }
}

static void iniciar_arquivo(void)
{
  static char cabecalho[1024];
  reg_status_t r;

  memset(cabecalho, 0, sizeof(cabecalho));
  montar_cabecalho(cabecalho, sizeof(cabecalho));
  r = registro_abrir(cabecalho);

  if (r == REG_OK)
  {
    fila_descartar();
    texto_len = 0;
    t_sync = HAL_GetTick();
    estado = EST_GRAVANDO;
    LOG("Gravando em %s\r\n", registro_nome_arquivo());
  }
  else if (r == REG_CHEIO)
  {
    registro_fechar();
    estado = EST_CARTAO_CHEIO;
    LOG("Cartao cheio\r\n");
  }
  else
  {
    registro_desmontar();
    estado = EST_SEM_CARTAO;
    LOG("Erro ao criar arquivo\r\n");
  }
}

static void tratar_resultado(reg_status_t r)
{
  if (r == REG_OK)
  {
    return;
  }
  texto_len = 0;
  if (r == REG_CHEIO)
  {
    registro_fechar();
    estado = EST_CARTAO_CHEIO;
    LOG("Cartao cheio: gravacao encerrada\r\n");
  }
  else
  {
    registro_desmontar();
    estado = EST_SEM_CARTAO;
    t_tentativa_cartao = HAL_GetTick();
    LOG("Erro de escrita no cartao: remontando\r\n");
  }
}

static void cartao_tentar(uint32_t agora)
{
  if (agora - t_tentativa_cartao < CFG_RETENTATIVA_MS)
  {
    return;
  }
  t_tentativa_cartao = agora;

  if (BSP_SD_IsDetected() != SD_PRESENT)
  {
    return;
  }
  if (registro_montar() != REG_OK)
  {
    LOG("Cartao presente, mas nao montou\r\n");
    registro_desmontar();
    return;
  }
  if (registro_acertar_rtc_por_arquivo())
  {
    LOG("RTC acertado por RTC.TXT\r\n");
  }
  iniciar_arquivo();
}

static reg_status_t processar_gravacao(uint32_t agora)
{
  amostra_imu_t a;
  reg_status_t r = REG_OK;

  /* No máximo 64 amostras por volta, para não atrasar a supervisão */
  for (int i = 0; i < 64 && fila_retirar(&a); i++)
  {
    gerar_linha(a.t_us, &a.dado);
    if (texto_len >= CFG_LIMIAR_ESCRITA)
    {
      r = descarregar_texto();
      if (r != REG_OK)
      {
        return r;
      }
    }
  }

  if (!imu_ok)
  {
    /* Sem IMU, as linhas passam a ser geradas pelas leituras do BME280;
     * sem nenhum sensor, uma linha por segundo mostra que o sistema vive. */
    if ((bme_novo && bme_ok) || (agora - t_ultima_linha >= 1000u))
    {
      gerar_linha(tempo_us(), NULL);
    }
  }

  if (agora - t_sync >= CFG_PERIODO_SYNC_MS)
  {
    t_sync = agora;
    r = descarregar_texto();
    if (r == REG_OK)
    {
      r = registro_sincronizar();
    }
    if (r == REG_OK && registro_tamanho() >= CFG_TAMANHO_MAX_ARQUIVO)
    {
      registro_fechar();
      iniciar_arquivo();
    }
  }
  return r;
}

/* ========================================================================= */
/* LED, botão e watchdog                                                     */
/* ========================================================================= */

static void led_atualizar(uint32_t agora)
{
  uint32_t fase;
  int aceso;

  switch (estado)
  {
    case EST_GRAVANDO:
      fase = agora % 1000u;
      if (imu_ok && bme_ok)
      {
        aceso = fase < 50u;                                  /* 1 piscada/s       */
      }
      else
      {
        aceso = fase < 50u || (fase >= 200u && fase < 250u); /* 2 piscadas/s      */
      }
      break;
    case EST_SEM_CARTAO:
      aceso = (agora % 200u) < 100u;                         /* rápido, 5 Hz      */
      break;
    case EST_CARTAO_CHEIO:
      aceso = 1;                                             /* aceso direto      */
      break;
    case EST_PARADO:
    default:
      aceso = (agora % 2000u) < 1000u;                       /* lento, 0,5 Hz     */
      break;
  }
  /* LED ligado entre 3V3 (via R12) e PC13: acende com nível baixo */
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, aceso ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static int botao_pressionado(uint32_t agora)
{
  static uint8_t estavel = 1;
  static uint8_t anterior = 1;
  static uint32_t t_mudanca;
  uint8_t lido = (uint8_t)HAL_GPIO_ReadPin(BTN_USER_GPIO_Port, BTN_USER_Pin);

  if (lido != anterior)
  {
    anterior = lido;
    t_mudanca = agora;
  }
  else if (lido != estavel && agora - t_mudanca >= 30u)
  {
    estavel = lido;
    return estavel == 0; /* ativo em nível baixo (pull-up interno) */
  }
  return 0;
}

static void watchdog_iniciar(void)
{
#if CFG_WATCHDOG
  /* IWDG por registradores (o módulo HAL do IWDG não foi habilitado no
   * CubeMX). LSI ~32 kHz / 128 = 250 Hz; com recarga de 2000, dá cerca de 8 s. */
  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP; /* para durante a depuração */
  IWDG->KR = 0xCCCCu;
  IWDG->KR = 0x5555u;
  IWDG->PR = 5u;
  IWDG->RLR = 2000u;
  while (IWDG->SR != 0u)
  {
  }
  IWDG->KR = 0xAAAAu;
#endif
}

static void watchdog_alimentar(void)
{
#if CFG_WATCHDOG
  IWDG->KR = 0xAAAAu;
#endif
}

static void identificar_causa_reset(void)
{
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
  {
    causa_reset = "watchdog";
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))
  {
    causa_reset = "queda de tensao (BOR)";
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
  {
    causa_reset = "energizacao";
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
  {
    causa_reset = "software";
  }
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
  {
    causa_reset = "botao de reset";
  }
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

/* ========================================================================= */
/* API                                                                       */
/* ========================================================================= */

void datalogger_iniciar(void)
{
  uint32_t agora;

  identificar_causa_reset();
  tempo_iniciar();

  imu.ler = imu_spi_ler;
  imu.escrever = imu_spi_escrever;
  imu.atraso_ms = atraso_ms;
  imu.ctx = NULL;

  bme.ler = bme_i2c_ler;
  bme.escrever = bme_i2c_escrever;
  bme.atraso_ms = atraso_ms;
  bme.ctx = NULL;

  LOG("\r\ndatalogger-epta firmware " FIRMWARE_VERSAO " (reset: %s)\r\n", causa_reset);

  imu_tentar_iniciar();
  bme_tentar_iniciar();

  agora = HAL_GetTick();
  t_bme = t_sync = t_ultima_linha = agora;
  t_tentativa_imu = t_tentativa_bme = t_verificacao_imu = agora;
  t_tentativa_cartao = agora - CFG_RETENTATIVA_MS; /* tenta o cartão já na 1ª volta */
  estado = EST_SEM_CARTAO;

  watchdog_iniciar();
}

void datalogger_executar(void)
{
  uint32_t agora = HAL_GetTick();
  int botao;
  int cartao_presente;

  watchdog_alimentar();
  (void)tempo_us(); /* mantém a extensão de 64 bits do TIM2 em dia */

  imu_supervisionar(agora);
  bme_supervisionar(agora);
  botao = botao_pressionado(agora);
  cartao_presente = (BSP_SD_IsDetected() == SD_PRESENT);

  switch (estado)
  {
    case EST_SEM_CARTAO:
      fila_descartar();
      cartao_tentar(agora);
      break;

    case EST_GRAVANDO:
      if (!cartao_presente)
      {
        LOG("Cartao removido\r\n");
        registro_desmontar();
        texto_len = 0;
        estado = EST_SEM_CARTAO;
      }
      else if (botao)
      {
        reg_status_t r = descarregar_texto();
        if (r == REG_OK)
        {
          r = registro_sincronizar();
        }
        registro_fechar();
        estado = EST_PARADO;
        LOG("Gravacao encerrada pelo botao (%d)\r\n", (int)r);
      }
      else
      {
        tratar_resultado(processar_gravacao(agora));
      }
      break;

    case EST_CARTAO_CHEIO:
    case EST_PARADO:
      fila_descartar();
      if (!cartao_presente)
      {
        registro_desmontar();
        estado = EST_SEM_CARTAO;
      }
      else if (estado == EST_PARADO && botao)
      {
        iniciar_arquivo();
      }
      break;
  }

  led_atualizar(agora);
}
