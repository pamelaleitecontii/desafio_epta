/**
 * @file    datalogger.h
 * @brief   Aplicação do datalogger: aquisição, gravação e tratamento de falhas.
 */
#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <stdint.h>

#define FIRMWARE_VERSAO           "1.0.0"

/* Parâmetros ajustáveis */
#define CFG_PERIODO_BME_MS        40u      /* 25 leituras/s do BME280                     */
#define CFG_PERIODO_SYNC_MS       1000u    /* f_sync: perda máxima de ~1 s se cair energia */
#define CFG_RETENTATIVA_MS        1000u    /* nova tentativa de sensor ou cartão           */
#define CFG_VERIFICACAO_MS        1000u    /* checagem periódica do WHO_AM_I do IMU        */
#define CFG_IMU_SILENCIO_MS       50u      /* sem amostra do IMU por esse tempo = falha    */
#define CFG_BME_FALHAS_MAX        3u       /* leituras seguidas com erro = sensor perdido  */
#define CFG_TAMANHO_MAX_ARQUIVO   (1024u * 1024u * 1024u) /* troca de arquivo a cada 1 GB */
#define CFG_FILA_IMU              2048u    /* amostras (~4,9 s a 416 Hz); potência de 2   */
#define CFG_BUFFER_TEXTO          8192u    /* bytes de CSV acumulados antes do f_write    */
#define CFG_LIMIAR_ESCRITA        4096u
#define CFG_WATCHDOG              1        /* 0 desliga o IWDG (útil para depurar)        */
#define CFG_DEBUG_UART            1        /* mensagens no USART2 (PA2), só na Black Pill */

void datalogger_iniciar(void);
void datalogger_executar(void);

/** Chamada pela interrupção do pino INT1 do IMU (PB0). */
void datalogger_imu_interrupcao(void);

#endif /* DATALOGGER_H */
