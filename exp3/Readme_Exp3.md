# Calculadora Multi-bits ESP32 com Medição de Tempo

O projeto 2 foi atualizado e otimizado para atender aos requisitos laboratoriais de expansão de operações matemáticas, medição de tempos de execução na arquitetura do processador e implementação de algoritmos iterativos em nível de hardware (linguagem C/C++).

## 🚀 Funcionalidades Implementadas

1. **Soma (+) e Subtração (-):** Operações básicas herdadas da versão anterior, validadas e 100% retrocompatíveis.
2. **Multiplicação (\*):** Implementada nativamente no microcontrolador em C utilizando o algoritmo iterativo de deslocamento e adição (_Shift and Add_), eliminando processamentos no lado do JavaScript do navegador.
3. **Fatorial (!):** Calculado via laço iterativo sequencial em C no ESP32, processando o fatorial do operando A.
4. **Divisão (/) [Desafio]:** Algoritmo de divisão inteira por subtrações sucessivas com tratamento robusto para prevenção de divisão por zero.
5. **Suporte Multi-bits Avançado:** Remoção do limite estrito de 4 bits na interface web; o sistema agora calcula de forma dinâmica o peso do sinal (Complemento de 2) baseado na quantidade de caracteres binários digitados pelo usuário.
6. **Medição de Performance Real:** Captura precisa do tempo de processamento gasto pelo processador do ESP32 através da função `micros()`, enviando as métricas em microssegundos ($\mu s$) diretamente para o frontend.
7. **Saída Física via LEDs:** Exibição em tempo real dos 4 bits menos significativos (LSB) do resultado nos LEDs da protoboard, mantendo a integridade do circuito analógico original.

## 🛠️ Requisitos de Hardware e Montagem

### Componentes Necessários

- 1x Placa de Desenvolvimento ESP32 (ex: ESP32-WROOM-32)
- 1x Protoboard
- 4x LEDs (de preferência de cores limpas para facilitar a leitura dos bits)
- 4x Resistores de $220\Omega$ ou $330\Omega$
- Cabos Jumpers (Macho-Macho / Macho-Fêmea)

## 💻 Configuração e Upload do Código

1. Conecte o ESP32 ao computador via cabo USB, selecione a porta COM correta e o modelo correspondente da sua placa.
2. Compile e realize o upload do binário para o microcontrolador.
3. Abra o **Serial Monitor** configurado em **115200 baud** para acompanhar o status de inicialização do Access Point.
