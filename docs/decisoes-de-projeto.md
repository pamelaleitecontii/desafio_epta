# Decisões de Projeto

Registro das decisões tomadas ao longo do projeto, com a justificativa e a fonte de cada uma.

> Campos marcados com `p. __` devem ser preenchidos com a página exata do documento citado.

---

## 1. Microcontrolador: STM32F411CEU6

**Decisão:** STM32F411CEU6, encapsulamento UFQFPN48.

**Justificativa:**
- Sugerido pelo desafio por ter SDIO, USB e 512 KB de flash, o que atende a todos os periféricos exigidos.
- É o mesmo chip da placa WeAct Black Pill, o que permite validar o firmware nela sem alterações.
- Ampla disponibilidade de documentação, exemplos e suporte no STM32CubeMX clássico (HAL1).

**Observação:** o encapsulamento UFQFPN48 (QFN, sem terminais, com pad térmico) é de difícil soldagem manual. Como a placa não será montada fisicamente neste desafio, isso não foi um impeditivo; para montagem real, o recomendado seria montagem pelo fabricante (PCBA) ou soldagem com ar quente.

---

## 2. IMU: LSM6DSO

**Decisão:** LSM6DSO, via SPI.

**Justificativa:**
- Atende ao requisito de acelerômetro + giroscópio com interface SPI e pino de interrupção.
- Foi escolhido principalmente pela grande disponibilidade de informação e de código, incluindo o driver oficial da ST, independente de plataforma (`STMicroelectronics/lsm6dso-pid`).
- Alternativas consideradas: ICM-42688-P e ICM-42686-P.

**Interface:** SPI (e não I²C) por permitir taxas de amostragem maiores, o que é relevante para registrar o movimento durante o voo.

---

## 3. Ferramenta de configuração: STM32CubeMX (e não CubeMX2)

**Decisão:** STM32CubeMX 6.x.

**Justificativa:** o STM32CubeMX2 suporta apenas microcontroladores da camada HAL2 (atualmente a série STM32C5). O STM32F411 é suportado apenas pelo STM32CubeMX clássico.

**Fonte:** página oficial do STM32CubeMX (st.com/stm32cubemx).

---

## 4. Ordem de alocação dos periféricos

**Decisão:** os periféricos foram configurados no CubeMX na ordem SYS → RCC → SDIO → USB → I²C → SPI → GPIOs.

**Justificativa:** os periféricos com menos opções de pino (SDIO, USB) foram alocados primeiro, e os mais flexíveis (I²C, SPI, GPIOs) se encaixaram nos pinos restantes, evitando conflitos.

---

## 5. SDIO em pinos alternativos

**Decisão:** SDIO em 4 bits nos pinos CK = PB15, CMD = PA6, D0 = PB7, D1 = PA8, D2 = PA9, D3 = PB5.

**Justificativa:** os pinos "padrão" do SDIO (PC8 a PC12 e PD2) não existem no encapsulamento de 48 pinos. O CubeMX alocou o periférico nas funções alternativas disponíveis.

**Consequência para o layout:** os pinos do SDIO ficaram distribuídos em três lados do chip. O slot microSD deve ser posicionado próximo ao canto superior direito do microcontrolador, com atenção ao roteamento do CMD (PA6, no lado inferior), para manter as trilhas curtas e de comprimento semelhante, como pede o desafio.

---

## 6. Aviso de conflito parcial do SDIO com o I2C1

**Situação:** o CubeMX indica "SDIO partly disabled – conflict with I2C1".

**Análise:** no STM32F411, os pinos PB8 e PB9 (usados pelo I2C1) também são SDIO_D4 e SDIO_D5, usados apenas no modo de 8 bits. O projeto usa o modo de 4 bits (D0 a D3), que não é afetado.

**Decisão:** aviso ignorado conscientemente.

---

## 7. MISO do SPI1 no PB4

**Situação:** o MISO do SPI1 ficou no PB4, distante do SCK (PA5) e do MOSI (PA7).

**Análise:** o MISO do SPI1 só pode ficar no PA6 ou no PB4. O PA6 é a única opção de pino para o SDIO_CMD nesse encapsulamento (a outra opção, PD2, não existe nele). Os demais SPIs (SPI2 a SPI5) apresentavam conflito com pinos já ocupados pelo SDIO ou pelo USB.

**Decisão:** manter o MISO no PB4. Com o SPI operando a no máximo 10 MHz, uma trilha alguns centímetros mais longa não compromete o sinal. O IMU será posicionado próximo aos pinos PA4 a PA7, e apenas a trilha do MISO fará o contorno.

---

## 8. I²C no PB8 / PB9

**Decisão:** SCL movido do PB6 para o PB8, ficando SCL = PB8 e SDA = PB9.

**Justificativa:** os dois pinos ficam lado a lado, o que simplifica o roteamento do barramento I²C.

---

## 9. Compatibilidade de pinagem com a Black Pill

**Decisão:** LED de status no PC13, botão no PA0, cristal de 25 MHz no HSE, cristal de 32,768 kHz no LSE e USB no PA11/PA12, iguais aos da WeAct Black Pill.

**Justificativa:** o mesmo firmware roda na placa projetada e na Black Pill usada para validação, sem alterações.

**Fonte:** WeAct Studio, repositório MiniF4-STM32F4x1.

---

## 10. Clock do sistema em 96 MHz

**Decisão:** HCLK = 96 MHz (PLL: M = 25, N = 192, P = 2, Q = 4).

**Justificativa:** o USB e o SDIO precisam de um clock de exatamente 48 MHz, derivado do mesmo PLL. Com HSE de 25 MHz, 96 MHz é a frequência de sistema mais próxima do máximo (100 MHz) que permite obter 48 MHz exatos na saída Q.

---

## 11. RTC alimentado pelo LSE

**Decisão:** RTC com o cristal externo de 32,768 kHz (LSE), e não com o oscilador interno (LSI).

**Justificativa:** o LSI é um oscilador RC com tolerância de alguns por cento, o que faria os timestamps derivarem ao longo do voo. O LSE, a cristal, oferece precisão muito maior para timestamps absolutos.

---

## 12. TIM2 como base de tempo em microssegundos

**Decisão:** TIM2 com prescaler 95 e período 2³² − 1.

**Justificativa:** com o clock de timer do APB1 em 96 MHz, o prescaler 95 resulta em 1 MHz (1 tick = 1 µs). Por ser um timer de 32 bits, o contador só reinicia após cerca de 71 minutos, tempo muito maior que a duração de um voo.

---

## 13. Capacitor no pino VCAP1

**Decisão:** capacitor cerâmico X5R/X7R de 4,7 µF entre o VCAP1 e o GND.

**Justificativa:** o datasheet especifica, para encapsulamentos com apenas um pino VCAP, um único capacitor externo de 4,7 µF com ESR < 1 Ω para estabilizar o regulador interno. Capacitores cerâmicos atendem ao requisito de ESR com folga.

**Fonte:** STM32F411xC/E Datasheet (DS10314), p. __.

---

## 14. Configuração do SDIO e do FatFs

**Decisões:**
- SDIO em 4 bits, com fator de divisão de clock 2 (16 MHz no cartão), uma velocidade conservadora para a inicialização e a operação.
- DMA habilitado para SDIO_RX e SDIO_TX, com interrupções habilitadas.
- FatFs em modo SD Card, com detecção de cartão no PB1, nomes longos de arquivo (LFN) e setor de 512 bytes.

---

## 15. Validação com hardware limitado

**Situação:** para a validação, estavam disponíveis apenas uma Black Pill e um ST-Link, sem os módulos de sensores e de microSD.

**Decisão:** validar em hardware tudo o que não depende dos módulos (gravação, clock, LED, botão, RTC, USB) e usar a ausência dos módulos para testar de fato o tratamento de erros exigido pelo desafio ("falha de sensor" e "cartão ausente"). A lógica independente de hardware (compensação do BME280, conversão das leituras do IMU, formatação do CSV) é testada no computador com valores de referência do datasheet.
