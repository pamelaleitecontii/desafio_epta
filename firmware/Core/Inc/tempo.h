/**
 * @file    tempo.h
 * @brief   Base de tempo em microssegundos (TIM2), estendida para 64 bits.
 */
#ifndef TEMPO_H
#define TEMPO_H

#include <stdint.h>

/** Liga o TIM2 (1 tick = 1 µs, configurado no CubeMX). */
void tempo_iniciar(void);

/** Microssegundos desde tempo_iniciar(). Pode ser chamada em interrupções. */
uint64_t tempo_us(void);

/** Espera ativa curta, em microssegundos. */
void tempo_esperar_us(uint32_t us);

#endif /* TEMPO_H */
