# Datalogger Ambiental e Inercial com STM32F411

Projeto desenvolvido para o desafio do processo seletivo da Payload, da EPTA (Equipe de Propulsão e Tecnologia Aeroespacial da UFU).

Trata-se de uma placa embarcada no payload de um foguete, responsável por registrar o voo inteiro: temperatura, umidade e pressão do ambiente, além da aceleração e da velocidade angular do veículo. Os dados são gravados em um cartão microSD, em arquivos CSV, para análise posterior.

## Status do projeto

O projeto está em andamento.

- [x] Configuração do microcontrolador no STM32CubeMX (pinagem, clock, periféricos, FatFs e RTC)
- [x] Projeto base compilando no STM32CubeIDE, sem erros e sem warnings
- [ ] Esquemático no KiCad
- [ ] Layout da PCB no KiCad
- [ ] Firmware (drivers dos sensores, gravação do CSV e tratamento de erros)
- [ ] Testes da lógica do firmware no computador

## Componentes principais

| Bloco | Componente | Interface |
| Microcontrolador | STM32F411CEU6 (UFQFPN48) | Não se aplica |
| Temperatura, umidade e pressão | BME280 | I²C |
| IMU (acelerômetro e giroscópio) | LSM6DSO | SPI, com pino de interrupção |
| Armazenamento | Slot microSD | SDIO de 4 bits |
| Alimentação | USB-C com regulador de 3,3 V | Não se aplica |
| Gravação e depuração | Conector SWD | SWDIO e SWCLK |
| Interface com o usuário | LED de status e botão | GPIO |

## Pinagem

A pinagem foi definida no STM32CubeMX, e a configuração completa está no arquivo [`firmware/firmware.ioc`](firmware/firmware.ioc).

Os pinos do LED, do botão, dos cristais, do USB e do SWD seguem a mesma disposição da placa WeAct Black Pill, que usa o mesmo microcontrolador. Com isso, o mesmo firmware pode ser validado futuramente em uma Black Pill e rodar na placa projetada sem nenhuma alteração.

### Cartão microSD (SDIO de 4 bits)

| Sinal | Pino | Observação |
| CK | PB15 | Clock do cartão |
| CMD | PA6 | Comandos |
| D0 | PB7 | Dados |
| D1 | PA8 | Dados |
| D2 | PA9 | Dados |
| D3 | PB5 | Dados |
| SD_DETECT | PB1 | Detecção de cartão, entrada com pull-up |

### IMU LSM6DSO (SPI1)

| Sinal | Pino | Observação |
| SCK | PA5 | Clock do SPI |
| MOSI | PA7 | Dados do microcontrolador para o sensor |
| MISO | PB4 | Dados do sensor para o microcontrolador |
| IMU_CS | PA4 | Seleção do sensor, controlada por software |
| IMU_INT1 | PB0 | Interrupção de dado pronto, borda de subida |

### Sensor BME280 (I2C1)

| Sinal | Pino |
| SCL | PB8 |
| SDA | PB9 |

### USB

| Sinal | Pino |
| D- | PA11 |
| D+ | PA12 |

### Gravação e depuração

| Interface | Sinal | Pino |
| SWD | SWDIO | PA13 |
| SWD | SWCLK | PA14 |
| USART2 | TX | PA2 |
| USART2 | RX | PA3 |

### Cristais

| Cristal | Uso | Pinos |
| 25 MHz | Clock principal (HSE) | PH0 e PH1 |
| 32,768 kHz | Relógio de tempo real (LSE) | PC14 e PC15 |

### LED e botão

| Função | Pino | Observação |
| LED_STATUS | PC13 | Mesmo pino do LED da Black Pill |
| BTN_USER | PA0 | Entrada com pull-up, mesmo pino do botão da Black Pill |

## Clock

O clock do sistema vem do cristal externo de 25 MHz, passando pelo PLL.

| Parâmetro | Valor |
| Fonte do PLL | HSE, cristal de 25 MHz |
| Divisores do PLL | M = 25, N = 192, P = 2, Q = 4 |
| Clock do sistema (HCLK) | 96 MHz |
| Barramento APB1 | 48 MHz |
| Barramento APB2 | 96 MHz |
| Clock do USB e do SDIO | 48 MHz |
| RTC | LSE, cristal de 32,768 kHz |

O sistema opera em 96 MHz, e não no máximo de 100 MHz, porque o USB e o SDIO precisam de exatamente 48 MHz. Com um cristal de 25 MHz, 96 MHz é a frequência que permite obter esses 48 MHz exatos a partir do mesmo PLL.

Outras configurações:

- **TIM2**: conta de 1 em 1 µs (prescaler de 95). Por ser de 32 bits, só reinicia a contagem depois de cerca de 71 minutos, tempo muito maior que a duração de um voo. É usado nos timestamps de alta resolução.
- **RTC**: alimentado pelo cristal de 32,768 kHz, para registrar data e hora reais em cada leitura.
- **FatFs**: sobre o SDIO com DMA, com suporte a nomes longos de arquivo.

## Organização do repositório

| Pasta | Conteúdo |
| `hardware/` | Projeto do KiCad, Gerbers, BOM, arquivo de posicionamento e PDF do esquemático |
| `firmware/` | Projeto do STM32CubeIDE, gerado pelo STM32CubeMX |
| `docs/` | Decisões de projeto, diagrama de blocos, testes e datasheets |
| `scripts/` | Scripts em Python para análise dos CSVs |

## Como compilar e gravar o firmware

São necessários o [STM32CubeIDE](https://www.st.com/stm32cubeide) e um gravador ST-Link V2. O [STM32CubeMX](https://www.st.com/stm32cubemx) só é necessário para alterar a configuração do microcontrolador.

Para compilar:

1. Clonar este repositório.
2. No STM32CubeIDE, abrir **File > Import > STM32 > STM32CubeMX/STM32CubeIDE Project**.
3. Selecionar a pasta `firmware` e clicar em **Finish**.
4. Selecionar o projeto e compilar com **Ctrl+B**.

Para gravar, basta ligar o ST-Link ao conector SWD (SWDIO, SWCLK, GND, 3V3 e NRST) e clicar em **Run** no STM32CubeIDE. Para depurar, usar **Debug**.

Para alterar algum pino ou periférico, abrir o `firmware/firmware.ioc` no STM32CubeMX, fazer a alteração e clicar em **GENERATE CODE**. O código escrito dentro dos blocos `USER CODE BEGIN` e `USER CODE END` é preservado.

## Testes

A placa não foi montada fisicamente, e não houve hardware disponível (placa de desenvolvimento ou módulos) para validar o firmware dentro do prazo do desafio.

Por isso, a validação foi feita em duas frentes:

- **Compilação** do firmware completo no STM32CubeIDE.
- **Testes no computador** da parte do firmware que não depende de hardware: a compensação do BME280, conferida com os valores de exemplo do datasheet da Bosch; a conversão das leituras brutas do IMU para g e graus por segundo; e a formatação das linhas do CSV.

| Teste | Método | Resultado |
| Compilação | STM32CubeIDE | OK, sem erros e sem warnings |
| Compensação do BME280 | Teste no computador | Pendente |
| Conversão das leituras do IMU | Teste no computador | Pendente |
| Formatação do CSV | Teste no computador | Pendente |
| Testes em hardware | Não realizado | Sem hardware disponível |

### Roteiro de validação em hardware

Roteiro previsto para quando houver hardware disponível, seja a placa montada ou uma Black Pill com módulos em protoboard, na ordem sugerida pelo desafio:

1. Medir a tensão de 3,3 V na saída do regulador.
2. Gravar o firmware pelo SWD e verificar o LED de status piscando.
3. Fazer uma varredura no barramento I²C e confirmar a resposta do BME280.
4. Ler o registrador WHO_AM_I do LSM6DSO pelo SPI.
5. Criar um arquivo no cartão microSD.
6. Testar os erros tratados pelo firmware: sensor desconectado, cartão ausente e cartão cheio.
7. Comparar as leituras do BME280 com um instrumento de referência.
8. Verificar o IMU em repouso, esperando aceleração próxima de 1 g no eixo vertical.

## Decisões de projeto

A justificativa de cada escolha, com as fontes consultadas, está no arquivo [`docs/decisoes-de-projeto.md`](docs/decisoes-de-projeto.md).

## Referências

- STMicroelectronics. STM32F411xC/E Datasheet (DS10314).
- STMicroelectronics. AN4488: Getting started with STM32F4xxxx MCU hardware development.
- STMicroelectronics. UM1718: STM32CubeMX for STM32 configuration and initialization C code generation.
- STMicroelectronics. LSM6DSO Datasheet.
- Bosch Sensortec. BME280 Datasheet.
- WeAct Studio. MiniF4 (Black Pill), STM32F4x1. https://github.com/WeActTC/MiniF4-STM32F4x1
- EPTA, UFU. Desafio de Processo Seletivo Payload, 11 de outubro de 2026.