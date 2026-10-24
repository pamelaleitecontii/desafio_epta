/**
 * @file    datahora.c
 * @brief   Conversão de data e hora em texto.
 */
#include "datahora.h"

static int ler_numero(const char *txt, int digitos, int *valor)
{
  *valor = 0;
  for (int i = 0; i < digitos; i++)
  {
    if (txt[i] < '0' || txt[i] > '9')
    {
      return -1;
    }
    *valor = *valor * 10 + (txt[i] - '0');
  }
  return 0;
}

static int bissexto(int ano)
{
  return (ano % 4 == 0 && ano % 100 != 0) || (ano % 400 == 0);
}

static int dias_no_mes(int ano, int mes)
{
  static const uint8_t dias[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (mes == 2 && bissexto(ano)) ? 29 : dias[mes - 1];
}

int datahora_analisar(const char *txt, datahora_t *dh)
{
  int a, me, d, h, mi, s;

  if (ler_numero(&txt[0], 4, &a) || txt[4] != '-' ||
      ler_numero(&txt[5], 2, &me) || txt[7] != '-' ||
      ler_numero(&txt[8], 2, &d) || (txt[10] != ' ' && txt[10] != 'T') ||
      ler_numero(&txt[11], 2, &h) || txt[13] != ':' ||
      ler_numero(&txt[14], 2, &mi) || txt[16] != ':' ||
      ler_numero(&txt[17], 2, &s))
  {
    return -1;
  }
  if (a < 2000 || a > 2099 || me < 1 || me > 12 || d < 1 || d > dias_no_mes(a, me) ||
      h > 23 || mi > 59 || s > 59)
  {
    return -1;
  }
  dh->ano = (uint16_t)a;
  dh->mes = (uint8_t)me;
  dh->dia = (uint8_t)d;
  dh->hora = (uint8_t)h;
  dh->min = (uint8_t)mi;
  dh->seg = (uint8_t)s;
  return 0;
}

static void dois_digitos(char *buf, unsigned v)
{
  buf[0] = (char)('0' + (v / 10u) % 10u);
  buf[1] = (char)('0' + v % 10u);
}

size_t datahora_formatar(char *buf, const datahora_t *dh)
{
  dois_digitos(&buf[0], dh->ano / 100u);
  dois_digitos(&buf[2], dh->ano % 100u);
  buf[4] = '-';
  dois_digitos(&buf[5], dh->mes);
  buf[7] = '-';
  dois_digitos(&buf[8], dh->dia);
  buf[10] = ' ';
  dois_digitos(&buf[11], dh->hora);
  buf[13] = ':';
  dois_digitos(&buf[14], dh->min);
  buf[16] = ':';
  dois_digitos(&buf[17], dh->seg);
  return 19;
}

uint8_t datahora_dia_semana(uint16_t ano, uint8_t mes, uint8_t dia)
{
  /* Algoritmo de Sakamoto: 0 = domingo ... 6 = sábado */
  static const uint8_t t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int a = ano - (mes < 3);
  int ds = (a + a / 4 - a / 100 + a / 400 + t[mes - 1] + dia) % 7;
  return (uint8_t)(ds == 0 ? 7 : ds);
}
