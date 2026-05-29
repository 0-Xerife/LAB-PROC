# Experimento 2: Calculadora Binária de 4 Bits com ESP32

Este diretório (`exp2`) contém o código-fonte e a documentação do segundo experimento prático da disciplina. O objetivo do laboratório foi implementar uma calculadora binária funcional utilizando o microcontrolador ESP32, integrando conceitos de sistemas embarcados, desenvolvimento web básico, comunicação serial e aritmética binária digital (Complemento de Dois e Complemento de Um).

## 🛠️ Estrutura do Projeto

O experimento foi dividido em duas abordagens de firmware principais:

1. **`calculadora_webserver.ino` (Fases 1, 2 e 3):**
   - Transforma o ESP32 em um _Access Point_ (Hotspot Wi-Fi dedicado).
   - Hospeda uma interface gráfica minimalista embutida (HTML e JavaScript) para inserção dos dados.
   - Realiza os cálculos aritméticos de Soma e Subtração estritamente em linguagem C, no lado do servidor (Back-End).
   - Implementa a lógica de **Complemento de Dois** para representação de sinal em 4 bits (intervalo de -8 a +7) e faz a detecção automática de _Overflow_.

2. **`desafio_serial.ino` (Fase 4 - Desafio):**
   - Desativa as interfaces de rede (Wi-Fi) e o servidor web.
   - Estabelece uma comunicação ponto a ponto através da interface **Serial (UART)** via cabo USB.
   - Implementa a aritmética em **Complemento de Um**, processando os dados recebidos via terminal e aplicando o mecanismo de **_End-Around Carry_** quando ocorre estouro no bit mais significativo (MSB).

## 🔌 Arquitetura de Hardware e Conexões

Para a exibição dos resultados das operações lógicas, o barramento de saída da ULA (Unidade Lógica Aritmética) foi mapeado diretamente para 4 pinos GPIO do ESP32, conectados a LEDs indicadores na protoboard:

- **GPIO 19** ➡️ LED 3 (Bit de Sinal / MSB - _Most Significant Bit_)
- **GPIO 18** ➡️ LED 2
- **GPIO 5** ➡️ LED 1
- **GPIO 17** ➡️ LED 0 (LSB - _Least Significant Bit_)

_Nota: Todos os LEDs foram montados em série com resistores limitadores de corrente de 220 Ω / 330 Ω conectados ao barramento de aterramento comum (GND)._

## 🚀 Como Executar os Códigos

### Requisitos Prévios

- Arduino IDE instalada e configurada com o core oficial para placas ESP32.
- Cabo micro-USB / Tipo-C para gravação e comunicação.

### Passo a Passo para a Aplicação Webserver:

1. Abra o arquivo `calculadora_webserver.ino` na Arduino IDE.
2. Compile e grave o código no seu ESP32.
3. No seu computador ou smartphone, conecte-se à rede Wi-Fi aberta criada pelo chip chamada **`Calculadora_ESP32`**.
4. Abra o navegador de internet e acesse o endereço IP padrão: `http://192.168.4.1/`
5. Insira os operandos binários de 4 bits nas caixas de texto, selecione a operação desejada (SOMA ou SUB) e observe o retorno na tela e o acendimento correspondente dos LEDs físicos.

### Passo a Passo para o Desafio Serial:

1. Abra o arquivo `desafio_serial.ino` na Arduino IDE.
2. Compile e grave o código no microcontrolador.
3. Com a placa conectada na porta USB, abra o **Serial Monitor** da Arduino IDE.
4. Certifique-se de configurar a velocidade de transmissão para **115200 baud**.
5. Digite os dois números binários de 4 bits separados por um único espaço diretamente na barra de envio (Exemplo: `0110 0010`) e pressione _Enter_.
6. O processamento aritmético em Complemento de Um será exibido no terminal e os LEDs atualizarão o estado físico instantaneamente.
