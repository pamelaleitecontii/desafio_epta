/**
 * @file    registro.h
 * @brief   Gravação dos arquivos CSV no microSD (FatFs).
 */
#ifndef REGISTRO_H
#define REGISTRO_H

#include <stdint.h>

typedef enum
{
  REG_OK = 0,
  REG_ERRO,    /* cartão ausente, removido ou com falha de leitura/escrita */
  REG_CHEIO,   /* sem espaço livre */
} reg_status_t;

/** Inicializa o cartão e monta o sistema de arquivos. */
reg_status_t registro_montar(void);

/** Desmonta e força a reinicialização do cartão na próxima montagem. */
void registro_desmontar(void);

/** Se existir RTC.TXT ("AAAA-MM-DD HH:MM:SS"), acerta o RTC e apaga o arquivo. 1 se acertou. */
int registro_acertar_rtc_por_arquivo(void);

/** Cria o próximo LOGnnn.CSV livre e grava o cabeçalho (texto pronto). */
reg_status_t registro_abrir(const char *cabecalho);

/** Grava n bytes no arquivo aberto. */
reg_status_t registro_escrever(const void *dados, uint32_t n);

/** Força a gravação física (f_sync): limita a perda de dados se a energia cair. */
reg_status_t registro_sincronizar(void);

/** Fecha o arquivo aberto (ignora erros: o cartão pode já ter sido removido). */
void registro_fechar(void);

uint32_t    registro_tamanho(void);
const char *registro_nome_arquivo(void);

#endif /* REGISTRO_H */
