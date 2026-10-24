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

**Atualização:** como o PA0 (botão) e o PA2 e PA3 (USART2) não foram ligados na PCB, o botão BTN_USER e o USART2 foram desativados no CubeMX. O firmware não usa mais esses pinos, nem na Black Pill.

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
- SDIO em 4 bits, com fator de divisão de clock 2. A frequência no cartão é o clock do SDIO dividido por (CLKDIV + 2), ou seja, 48 / 4 = **12 MHz**. Antes estava escrito 16 MHz, o que não bate com a fórmula do manual de referência (RM0383, registrador SDIO_CLKCR). É uma velocidade conservadora, que dá uns 6 MB/s teóricos, muito acima dos 40 kB/s gravados. A identificação do cartão é feita pela própria HAL a 400 kHz.
- DMA habilitado para SDIO_RX e SDIO_TX, com interrupções habilitadas.
- FatFs em modo SD Card, com detecção de cartão no PB1, nomes longos de arquivo (LFN) e setor de 512 bytes.

---

## 15. Validação com hardware limitado

**Situação:** para a validação, estavam disponíveis apenas uma Black Pill e um ST-Link, sem os módulos de sensores e de microSD.

**Decisão:** validar em hardware tudo o que não depende dos módulos (gravação, clock, LED, botão, RTC, USB) e usar a ausência dos módulos para testar de fato o tratamento de erros exigido pelo desafio ("falha de sensor" e "cartão ausente"). A lógica independente de hardware (compensação do BME280, conversão das leituras do IMU, formatação do CSV) é testada no computador com valores de referência do datasheet.


## 16. Correções na configuração do CubeMX

Ao escrever o firmware, encontrei alguns problemas na configuração e corrigi tanto no `firmware.ioc` quanto no código gerado.

| Problema | Correção |
|---|---|
| O rótulo `IMU_CS` estava no PA4 e também no PA2, que é o TX do USART2. Com isso o CubeMX gerou `IMU_CS_Pin` apontando para o PA2 e criou um `IMU_CSA4_Pin` para o PA4, ou seja, o nome `IMU_CS` apontava para o pino errado | Tirei o rótulo do PA2. Agora `IMU_CS_Pin` é o PA4, como na PCB |
| O CS do IMU começava em nível baixo, deixando o sensor selecionado durante o boot | Nível inicial alto |
| O SPI1 estava com divisor 2, o que dá 48 MHz, quase cinco vezes o máximo de 10 MHz do LSM6DSO | Divisor 16, o que dá 6 MHz |
| O I2C1 estava em 100 kHz | 400 kHz, que o BME280 e os pull-ups de 4,7 kΩ suportam |
| Todas as interrupções estavam com prioridade 0 | SDIO com 1, DMA com 2 e a interrupção do IMU com 5 (ver a decisão 18) |

## 17. Leitura do IMU dentro da interrupção, com fila circular

**Decisão:** a interrupção de dado pronto do IMU lê a amostra pelo SPI e a guarda numa fila de 2048 posições, junto com o tempo em microssegundos. O laço principal só formata e grava.

**Justificativa:** uma escrita no microSD pode travar por centenas de milissegundos, e a especificação SD permite até 250 ms por bloco nos cartões SDHC. Se o IMU fosse lido no laço principal, cada pausa do cartão viraria um buraco nos dados. A leitura dentro da interrupção leva uns 30 µs a 6 MHz, o que ocupa cerca de 1,2 % da CPU a 416 Hz. A fila aguenta quase 5 s de atraso do cartão, e se mesmo assim encher, as amostras perdidas são contadas e gravadas na coluna `perdidas`, em vez de sumirem sem ninguém perceber.

**Detalhe:** o pino de dado pronto fica em nível alto até a amostra ser lida. Se alguma leitura falhar, não aparece uma nova borda de subida e a aquisição pararia de vez. Por isso o firmware trata como falha qualquer intervalo de 50 ms sem amostras e configura o sensor de novo.

## 18. Escrita no cartão por DMA

**Decisão:** trocar as funções `BSP_SD_ReadBlocks` e `BSP_SD_WriteBlocks`, que no código gerado funcionam por polling, por versões com DMA, no arquivo `sd_dma.c`.

**Justificativa:** no modo polling, a CPU precisa abastecer a FIFO de 32 palavras do SDIO a cada 20 µs, mais ou menos. A interrupção do IMU, que dura uns 30 µs, causaria erros de falta e de excesso de dados na FIFO. Com DMA, a transferência não depende da CPU. As funções originais são `__weak`, então a troca continua valendo mesmo depois de regenerar o código no CubeMX. O DMA foi configurado para acessar palavras de 32 bits, o que exige buffers alinhados em 4 bytes; quando o FatFs entrega um buffer desalinhado, os dados passam por um buffer intermediário.

As interrupções do SDIO e do DMA ficaram com prioridade maior que a do IMU, para que o fim de uma transferência nunca precise esperar a leitura do sensor.

## 19. Faixas e taxa do IMU

**Decisão:** acelerômetro em ±16 g e giroscópio em ±2000 graus por segundo, os dois a 416 Hz.

**Justificativa:** são as faixas máximas do LSM6DSO. Motores de foguetes amadores produzem acelerações de 5 a 15 g, com picos maiores na abertura do paraquedas, e o veículo pode girar rápido em torno do próprio eixo. A resolução que se perde, de 0,488 mg, é desprezível perto dessas grandezas, enquanto um sinal saturado não tem como ser recuperado depois. A taxa de 416 Hz descreve bem a dinâmica do voo e gera uns 40 kB/s de CSV, ou 144 MB por hora, um volume tranquilo para o cartão.

## 20. Configuração do BME280

**Decisão:** oversampling de 8 vezes na pressão, 2 vezes na temperatura e 1 vez na umidade, filtro IIR com coeficiente 4, modo normal e leitura a cada 40 ms.

**Justificativa:** a pressão é a grandeza que dá a altitude, então ela recebeu o maior oversampling. O filtro com coeficiente 4 reduz o ruído sem atrasar demais a resposta durante a subida; coeficientes altos, como 16, acabam suavizando o apogeu. Com essa configuração, cada medição leva uns 28 ms (apêndice B do datasheet do BME280).

## 21. CSV em ponto fixo, com uma linha por amostra do IMU

**Decisão:** cada linha é uma amostra do IMU, acompanhada da última leitura do BME280 e da marcação `bme_novo`. Os valores são convertidos em texto a partir de inteiros, sem printf e sem ponto flutuante.

**Justificativa:** um arquivo só, com uma base de tempo só, é o formato mais fácil de analisar, seja numa planilha ou no pandas. O printf da newlib nano não formata ponto flutuante por padrão, e a conversão a partir de inteiros é várias vezes mais rápida, o que faz diferença com 416 linhas por segundo.

## 22. Sincronização do arquivo a cada segundo

**Decisão:** chamar o f_sync a cada segundo e, entre uma sincronização e outra, juntar os dados em blocos de 4 kB.

**Justificativa:** sem o f_sync, os dados ficam no buffer do FatFs e a tabela FAT não é atualizada. Uma queda de energia, como um cabo que solta ou o impacto no pouso, pode deixar o arquivo com tamanho zero. Sincronizando a cada segundo, a perda máxima fica em torno de 1 s de dados, com pouco custo de desempenho. Gravar em blocos de 4 kB, que são múltiplos de 512 bytes, diminui a quantidade de acessos ao cartão.

## 23. Watchdog independente (IWDG)

**Decisão:** IWDG de uns 8 s, configurado direto nos registradores e parado durante a depuração.

**Justificativa:** se o firmware travar em voo, por um cartão que não responde ou qualquer falha inesperada, a placa reinicia sozinha e abre um arquivo novo, com "causa do reset: watchdog" no cabeçalho. Os 8 s ficam acima do pior caso de montagem de um cartão grande. O módulo do IWDG na HAL não estava habilitado no CubeMX, e configurar pelos registradores evitou mexer na configuração gerada.

## 24. Base de tempo de 64 bits e acerto do relógio por arquivo

**Decisão:** toda linha usa o `t_us`, que é o tempo em microssegundos desde que a placa ligou, contado pelo TIM2 e estendido para 64 bits no software. A data real vem do RTC e só aparece no cabeçalho, quando foi acertada pelo arquivo `RTC.TXT`.

**Justificativa:** o TIM2 tem 32 bits e zera a cada 71 minutos, mais ou menos, e a placa pode ficar ligada na rampa mais tempo que isso antes do lançamento. Com a extensão para 64 bits, esse problema deixa de existir. O RTC não serve como base de tempo do voo porque o VBAT está ligado ao 3,3 V, e a hora se perde sempre que a placa desliga. Como a placa não tem botão nem serial para acertar a hora, um arquivo no cartão foi o jeito mais simples.

## 25. Drivers independentes da HAL e testes no computador

**Decisão:** os drivers do BME280 e do LSM6DSO, a formatação do CSV e a leitura do `RTC.TXT` não incluem nenhum arquivo da HAL. O acesso ao barramento é feito por ponteiros de função.

**Justificativa:** assim essa parte do código compila e pode ser testada no computador ([`tests/`](../tests/)), sem precisar da placa. Os testes conferem a compensação do BME280 com o exemplo numérico da Bosch e com as fórmulas em ponto flutuante do datasheet, os registradores configurados, os casos de sensor ausente e de ID errado (com um barramento simulado), as conversões e a formatação. Eles rodam no GitHub Actions a cada push.

**Fonte do exemplo numérico:** o datasheet do BME280 não traz um exemplo com números. As fórmulas de temperatura e pressão são as mesmas do BMP280, e o datasheet dele (BST-BMP280-DS001, seção 3.12) traz os valores de referência: 25,08 °C e 100653,27 Pa.
