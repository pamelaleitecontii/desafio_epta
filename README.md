# Datalogger Ambiental e Inercial com STM32F411

Projeto desenvolvido para o desafio do processo seletivo da Payload, da EPTA (Equipe de Propulsão e Tecnologia Aeroespacial da UFU).

Trata-se de uma placa embarcada no payload de um foguete, responsável por registrar o voo inteiro: temperatura, umidade e pressão do ambiente, além da aceleração e da velocidade angular do veículo. Os dados são gravados em um cartão microSD, em arquivos CSV, para análise posterior.

## Status do projeto

Situação do projeto na entrega do desafio:

- [x] Configuração do microcontrolador no STM32CubeMX 
- [x] Projeto base compilando no STM32CubeIDE, sem erros e sem warnings
- [x] Esquemático no KiCad
- [x] Layout da PCB no KiCad
- [x] Firmware escrito: drivers do BME280 e do LSM6DSO, gravação do CSV e tratamento de erros
- [x] Testes da lógica do firmware no computador
- [x] Compilação do firmware completo no STM32CubeIDE

## Componentes principais

| Bloco | Componente | Interface |
|---|---|---|
| Microcontrolador | STM32F411CEU6 (UFQFPN48) | Não se aplica |
| Temperatura, umidade e pressão | BME280 | I²C |
| IMU (acelerômetro e giroscópio) | LSM6DSO | SPI, com pino de interrupção |
| Armazenamento | Slot microSD | SDIO de 4 bits |
| Alimentação | USB-C com regulador de 3,3 V | Não se aplica |
| Gravação e depuração | Conector SWD | SWDIO e SWCLK |
| Interface com o usuário | LED de status, botão de reset e botão de BOOT0 | GPIO e pinos do MCU |

## Pinagem

A pinagem foi definida no STM32CubeMX, e a configuração completa está no arquivo [`firmware/firmware.ioc`](firmware/firmware.ioc).

Os pinos do LED, dos cristais, do USB e do SWD seguem a mesma disposição da placa WeAct Black Pill, que usa o mesmo microcontrolador. Com isso, o mesmo firmware pode ser validado futuramente em uma Black Pill e rodar na placa projetada sem nenhuma alteração.

### Cartão microSD (SDIO de 4 bits)

| Sinal | Pino | Observação |
|---|---|---|
| CK | PB15 | Clock do cartão |
| CMD | PA6 | Comandos |
| D0 | PB7 | Dados |
| D1 | PA8 | Dados |
| D2 | PA9 | Dados |
| D3 | PB5 | Dados |
| SD_DETECT | PB1 | Detecção de cartão, entrada com pull-up |

### IMU LSM6DSO (SPI1)

| Sinal | Pino | Observação |
|---|---|---|
| SCK | PA5 | Clock do SPI |
| MOSI | PA7 | Dados do microcontrolador para o sensor |
| MISO | PB4 | Dados do sensor para o microcontrolador |
| IMU_CS | PA4 | Seleção do sensor, controlada por software |
| IMU_INT1 | PB0 | Interrupção de dado pronto, borda de subida |

### Sensor BME280 (I2C1)

| Sinal | Pino |
|---|---|
| SCL | PB8 |
| SDA | PB9 |

### USB

| Sinal | Pino |
|---|---|
| D- | PA11 |
| D+ | PA12 |

### Gravação e depuração

| Interface | Sinal | Pino |
|---|---|---|
| SWD | SWDIO | PA13 |
| SWD | SWCLK | PA14 |

### Cristais

| Cristal | Uso | Pinos |
|---|---|---|
| 25 MHz | Clock principal (HSE) | PH0 e PH1 |
| 32,768 kHz | Relógio de tempo real (LSE) | PC14 e PC15 |

### LED

| Função | Pino | Observação |
|---|---|---|
| LED_STATUS | PC13 | Mesmo pino do LED da Black Pill |

## Placa (PCB)

O projeto do KiCad está em [`hardware/datalogger/`](hardware/datalogger/).

| Item | Valor |
|---|---|
| Camadas | 2 (F.Cu e B.Cu) |
| Dimensões | 51,2 × 31,2 mm |
| Plano de terra | GND preenchido nas duas camadas, costurado com vias |
| Trilhas de sinal | 0,2 mm |
| Trilhas de alimentação (VBUS, +3V3, VDDA, GND) | 0,4 mm (classe de rede `Power`) |
| Isolamento mínimo | 0,2 mm |
| Vias | 0,6 mm de diâmetro, furo de 0,3 mm |
| Componentes passivos | 0603 |

**Alimentação:** USB-C (com resistores de 5,1 kΩ no CC1 e no CC2 e blindagem ligada ao GND por 1 MΩ em paralelo com 4,7 nF), proteção ESD com USBLC6-2SC6 nas linhas de dados e regulador AP2112K-3.3 com capacitores de 1 µF na entrada e na saída.

**Microcontrolador:** um capacitor de 100 nF junto de cada pino VDD e do VBAT, capacitor de volume de 4,7 µF, 4,7 µF no VCAP1 e filtro no VDDA (ferrite de 600 Ω a 100 MHz com 1 µF e 100 nF).

**Botões:** reset (NRST) e BOOT0 (com pull-down de 10 kΩ; pressionado durante o reset, coloca o chip no bootloader para gravação pelo USB).

**Pontos de teste:**

| Ponto | Rede |
|---|---|
| TP2 | VBUS |
| TP3 | +3V3 |
| TP10 | VDDA |
| TP5 | GND |
| TP6 | I2C1_SCL |
| TP7 | I2C1_SDA |

## Clock

O clock do sistema vem do cristal externo de 25 MHz, passando pelo PLL.

| Parâmetro | Valor |
|---|---|
| Fonte do PLL | HSE, cristal de 25 MHz |
| Divisores do PLL | M = 25, N = 192, P = 2, Q = 4 |
| Clock do sistema (HCLK) | 96 MHz |
| Barramento APB1 | 48 MHz |
| Barramento APB2 | 96 MHz |
| Clock do USB e do SDIO | 48 MHz |
| RTC | LSE, cristal de 32,768 kHz |

O sistema opera em 96 MHz, e não no máximo de 100 MHz, porque o USB e o SDIO precisam de exatamente 48 MHz. Com um cristal de 25 MHz, 96 MHz é a frequência que permite obter esses 48 MHz exatos a partir do mesmo PLL.

Outras configurações:

- **TIM2** conta de 1 em 1 µs (prescaler de 95). Por ser de 32 bits, só reinicia a contagem depois de cerca de 71 minutos, tempo muito maior que a duração de um voo. É usado nos timestamps de alta resolução.
- **RTC** alimentado pelo cristal de 32,768 kHz, para registrar data e hora reais em cada leitura.
- **FatFs** sobre o SDIO com DMA, com suporte a nomes longos de arquivo.

## Organização do repositório

| Pasta | Conteúdo |
|---|---|
| `hardware/` | Projeto do KiCad (esquemático e PCB) e arquivos de fabricação |
| `firmware/` | Projeto do STM32CubeIDE, gerado pelo STM32CubeMX |
| `docs/` | Decisões de projeto, diagrama de blocos, testes e datasheets |
| `scripts/` | Scripts em Python para análise dos CSVs |
| `tests/` | Testes da lógica do firmware que rodam no computador |

## Firmware

### Organização do código

O código próprio fica em arquivos separados dos gerados pelo CubeMX, e a parte que não depende de hardware fica separada dos drivers. Assim, regenerar o projeto não apaga nada e a lógica pode ser testada no computador.

| Arquivo (`firmware/Core/`) | Função | Depende da HAL? |
|---|---|---|
| `app.c` / `app.h` | Laço principal: leitura dos sensores, linhas do CSV, erros e LED | Sim |
| `bme280.c` / `bme280.h` | Driver do BME280 pelo I2C1 | Sim |
| `bme280_comp.c` / `bme280_comp.h` | Compensação de temperatura, pressão e umidade (fórmulas da Bosch) | Não |
| `lsm6dso.c` / `lsm6dso.h` | Driver do LSM6DSO pelo SPI1 e conversão para mg e mdps | Só o driver |
| `csv_log.c` / `csv_log.h` | Formatação das linhas do CSV | Não |
| `logger.c` / `logger.h` | Gravação no microSD com FatFs | Sim |

### Funcionamento

- **IMU:** 104 Hz, ±16 g e ±2000 dps. A cada amostra nova o sensor gera uma interrupção no INT1 (PB0); a interrupção só sinaliza, e a leitura pelo SPI acontece no laço principal. Se a interrupção parar de chegar por 50 ms, o IMU é lido por polling.
- **BME280:** modo normal, lido a cada 100 ms. A última leitura entra em todas as linhas do CSV.
- **Gravação:** cada amostra do IMU gera uma linha. As linhas se acumulam num buffer de 4 KB e são gravadas em setores inteiros de 512 bytes; `f_sync` é chamado a cada 1 s, então uma queda de energia perde no máximo cerca de 1 s de dados.
- **Arquivos:** cada gravação cria o próximo nome livre (`LOG000.CSV`, `LOG001.CSV`, ...).
- **Início:** a gravação começa sozinha ao ligar a placa, assim que houver cartão.

### Tratamento de erros

| Situação | Como é detectada | Comportamento |
|---|---|---|
| Cartão ausente | Pino SD_DETECT em nível alto | Não grava; tenta de novo a cada 1 s e começa sozinho quando o cartão é inserido |
| Cartão removido durante a gravação | SD_DETECT muda durante a gravação | Descarta o estado do arquivo e volta a esperar o cartão |
| Cartão cheio | `f_write` grava menos bytes que o pedido | Fecha o arquivo com o que já foi gravado e para |
| Falha do IMU | WHO_AM_I diferente de 0x6C ou erro no SPI | Continua gravando o BME280 a 10 Hz, com flag de erro; tenta de novo a cada 1 s |
| Falha do BME280 | ID diferente de 0x60 ou erro no I2C | Continua gravando o IMU, com flag de erro; tenta de novo a cada 1 s |

### LED de status (PC13)

| Padrão | Significado |
|---|---|
| 1 piscada curta por segundo | Gravando, sensores OK |
| 2 piscadas curtas por segundo | Gravando, algum sensor com falha |
| Pisca rápido (100 ms) | Cartão ausente |
| Pisca médio (250 ms) | Erro ao montar ou gravar no cartão |
| Aceso direto | Cartão cheio |

### Formato do CSV

Os valores são gravados como inteiros em unidades pequenas, para não depender de `printf` com ponto flutuante (desligado por padrão na biblioteca C do STM32). O script `scripts/plot_csv.py` converte para as unidades usuais.

| Coluna | Unidade | Exemplo | Valor |
|---|---|---|---|
| `t_s` | segundos desde a inicialização (resolução de 1 µs, TIM2) | `12.345678` | 12,345678 s |
| `rtc` | hora do RTC | `13:07:05` | |
| `ax_mg`, `ay_mg`, `az_mg` | mg | `999` | 0,999 g |
| `gx_mdps`, `gy_mdps`, `gz_mdps` | mdps (milésimos de grau por segundo) | `-70` | -0,07 °/s |
| `temp_cdegC` | centésimos de °C | `2508` | 25,08 °C |
| `press_Pa` | Pa | `100653` | 1006,53 hPa |
| `hum_cpct` | centésimos de %RH | `4523` | 45,23 %RH |
| `flags` | bit 0: erro no IMU; bit 1: erro no BME280 | `2` | BME280 com falha |

### Análise dos dados

```
pip install pandas matplotlib
python scripts/plot_csv.py LOG000.CSV            # abre os gráficos
python scripts/plot_csv.py LOG000.CSV grafico.png # salva em arquivo
```

O script mostra um resumo (duração, taxa de amostragem, linhas com erro, aceleração máxima e altitude barométrica relativa máxima) e plota aceleração, velocidade angular, temperatura, pressão com altitude relativa e umidade.

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
- **Testes no computador** da parte do firmware que não depende de hardware, em `tests/`.

| Teste | Método | Resultado |
|---|---|---|
| Compilação do projeto base (CubeMX) | STM32CubeIDE | OK, sem erros e sem warnings |
| Compilação do firmware completo | STM32CubeIDE | Pendente |
| Compensação do BME280: exemplo numérico da Bosch | `tests/test_host.c` | OK: 25,08 °C e 100653,25 Pa (esperado: 25,08 °C e 100653,27 Pa) |
| Compensação do BME280: ponto fixo x ponto flutuante | `tests/test_host.c`, varredura de 7 749 combinações | OK: diferença máxima de 0,005 °C, 0,07 Pa e 0,007 %RH |
| Leitura dos coeficientes de calibração (incluindo H4 e H5 negativos) | `tests/test_host.c` | OK |
| Conversão das leituras do IMU | `tests/test_host.c` | OK: 2049 LSB = 999 mg; fundo de escala = 15,99 g e 2293,7 °/s |
| Formatação do CSV | `tests/test_host.c` | OK, inclusive com tempo acima da volta de 32 bits do TIM2 |
| Gravação em setores alinhados de 512 bytes | `tests/test_logger_mock.c`, FatFs simulada | OK: 0 escritas desalinhadas |
| Detecção de cartão cheio e de cartão ausente | `tests/test_logger_mock.c`, FatFs simulada | OK |
| Script de análise | `scripts/plot_csv.py` com CSV sintético | OK |
| Testes em hardware | Não realizado | Sem hardware disponível |

Para rodar os testes (na raiz do repositório, com GCC):

```
gcc -std=c99 -Wall -Wextra -I firmware/Core/Inc tests/test_host.c firmware/Core/Src/bme280_comp.c firmware/Core/Src/csv_log.c -lm -o test_host
./test_host

gcc -std=c11 -Wall -Wextra -I tests/mock -I firmware/Core/Inc tests/test_logger_mock.c firmware/Core/Src/logger.c firmware/Core/Src/csv_log.c -o test_logger
./test_logger
```

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
- Bosch Sensortec. BME280 Datasheet (fórmulas de compensação, seção 4.2.3 e apêndice 8).
- Bosch Sensortec. BMP280 Datasheet (exemplo numérico de compensação de temperatura e pressão, usado nos testes).
- ChaN. FatFs: Generic FAT Filesystem Module. http://elm-chan.org/fsw/ff/
- WeAct Studio. MiniF4 (Black Pill), STM32F4x1. https://github.com/WeActTC/MiniF4-STM32F4x1
- EPTA, UFU. Desafio de Processo Seletivo Payload, 11 de outubro de 2026.
