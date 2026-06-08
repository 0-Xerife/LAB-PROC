# Controle de Periféricos com PWM e ESP32-C3

[cite_start]Este repositório contém o projeto desenvolvido para o **Laboratório 5** da disciplina **PCS3732**[cite: 1, 15]. [cite_start]O objetivo do experimento é implementar o controle integrado e não bloqueante de periféricos (um LED externo e um servomotor) a partir de uma interface web responsiva, utilizando a arquitetura RISC-V do microcontrolador ESP32-C3[cite: 2, 6, 151, 152].

## 🏗️ Arquitetura do Sistema

[cite_start]O sistema é estruturado em três camadas fundamentais[cite: 17]:

1. [cite_start]**Camada Web (Software de Alto Nível):** Dashboard em HTML5, CSS3 e JavaScript executado no navegador do cliente[cite: 18, 19]. Controla os periféricos de forma assíncrona usando a API `fetch()` do JavaScript para evitar o recarregamento de página.
2. [cite_start]**Camada Lógica (ESP32 / RISC-V):** Servidor HTTP assíncrono local que intercepta as requisições de rede[cite: 20, 34]. [cite_start]Em nível de silício, a arquitetura RISC-V emprega **E/S Mapeada em Memória (Memory-Mapped I/O)** [cite: 59][cite_start], acessando os registradores dos periféricos através de instruções nativas de carga e armazenamento (`load`/`store`)[cite: 67].
3. [cite_start]**Camada Física (Hardware):** Composta por circuitos de atuação em bancada, contendo um LED de alta frequência com resistor de proteção e um servomotor eletromecânico[cite: 37, 38, 39, 103, 145, 150].

## 📝 Respostas Teóricas (Pré-Aula)

Com base no material didático e nas diretrizes fornecidas, seguem as respostas aos questionamentos do roteiro (em formato de documentação para o repositório):

### 1. Acesso a Periféricos no ESP32-C3 e na Arquitetura RISC-V

- [cite_start]**Mapeamento de Periféricos:** Na placa ESP32-C3, o mapeamento é feito estritamente em **memória (Memory-Mapped I/O)**[cite: 59, 69]. [cite_start]Leituras e escritas nos registradores dos controladores utilizam as mesmas instruções tradicionais de carga e armazenamento (`load`/`store`) da arquitetura de memória convencional[cite: 67].
- [cite_start]**Transição de Privilégio e Syscalls:** Em arquiteturas RISC-V, o acesso direto ao hardware é protegido[cite: 51, 52]. [cite_start]As aplicações em espaço de usuário solicitam serviços ao ambiente de execução através de **chamadas de sistema (_syscalls_)**[cite: 53, 54]. [cite_start]Utiliza-se a instrução `ecall` para alterar o nível de privilégio e interagir com segurança com os periféricos[cite: 47, 48, 54].

### 2. Implementação do PWM no ESP32-C3

- [cite_start]Ao contrário de técnicas de software como _bit-banging_, o ESP32-C3 utiliza o periférico de hardware dedicado **LEDC (LED Control)**[cite: 99].
- [cite_start]O controle do PWM ocorre diretamente em hardware nativo, o que dispensa completamente a CPU de manter os ciclos de clock e os tempos de transição de estado[cite: 99].

### 3. O Gargalo do _Busy Waiting_ e suas Alternativas

- [cite_start]**Desvantagens:** A técnica de espera ocupada (_Busy Waiting_) baseia-se em laços de checagem contínuos (`while`)[cite: 73, 76]. [cite_start]Isso gera um **desperdício crítico de ciclos da CPU**, impedindo que o sistema processe outras tarefas em paralelo, como responder a requisições de rede simultâneas[cite: 71, 72].
- [cite_start]**Alternativas:** As soluções ideais envolvem a delegação do trabalho para periféricos de hardware dedicados (como o LEDC) ou a implementação de E/S direcionada por interrupções (**Interrupt-driven I/O**)[cite: 80, 87].

## ⚙️ Configuração das Frequências e Canais PWM

[cite_start]O sistema coordena simultaneamente dois canais do periférico LEDC com exigências físicas completamente distintas para garantir uma operação não bloqueante[cite: 140, 151, 152]:

| Periférico      | Canal LEDC                      | Frequência                                         | Resolução          | Comportamento Físico                                                                                                                       |
| :-------------- | :------------------------------ | :------------------------------------------------- | :----------------- | :----------------------------------------------------------------------------------------------------------------------------------------- |
| **LED Externo** | [cite_start]Canal 0 [cite: 141] | [cite_start]Alta (~1 kHz a 5 kHz) [cite: 128, 145] | 8 bits (0 - 255)   | [cite_start]Modulação linear do brilho sem oscilações bruscas ou cintilação (_flicker_)[cite: 128, 212].                                   |
| **Servomotor**  | [cite_start]Canal 1 [cite: 148] | [cite_start]Baixa Estável (50 Hz) [cite: 150]      | 12 bits (0 - 4095) | [cite_start]Controle angular preciso calibrado por largura de pulso: 1.0ms para 0°, 1.5ms para 90° e 2.0ms para 180°[cite: 131, 134, 136]. |

> [cite_start]⚠️ **Atenção:** O ESP32 fornece sinal lógico de 3.3V, mas o servomotor deve ser alimentado adequadamente através do pino V-IN ou por uma fonte externa de 5V para suprir as demandas de corrente[cite: 5, 7, 137, 138].

## 📋 Requisitos e Engenharia de Software (ISO 25010)

[cite_start]O desenvolvimento do sistema foi guiado pela especificação rigorosa de requisitos e seus respectivos testes[cite: 205, 215]:

- [cite_start]**RF1 - Interface Acessível:** Conectar à rede Wi-Fi via IP do ESP32 pelo celular[cite: 207].
  - [cite_start]_Teste:_ Validar o carregamento completo do HTML sem falhas de requisição GET[cite: 207].
- [cite_start]**RF2 - Controle Lumínico:** Alterar o slider do LED na página web de 0 a 100%[cite: 210].
  - [cite_start]_Teste:_ Verificar a variação linear do brilho físico do LED sem oscilações bruscas[cite: 212].
- [cite_start]**RF3 - Atuação Posicional:** Solicitar ângulos limites de 0° e 180° através da interface web[cite: 211].
  - [cite_start]_Teste:_ Confirmar se o servo atinge o ângulo requerido sem engasgar ou gerar picos de corrente[cite: 213].
- [cite_start]**RNF1 - Eficiência de Desempenho (ISO 25010):** O sistema de PWM deve operar exclusivamente em canais de hardware dedicados[cite: 215, 216].
  - [cite_start]_Teste:_ Monitorar a latência da página web sob carga, provando a ausência de gargalos na CPU[cite: 217].
- [cite_start]**RNF2 - Usabilidade (ISO 25010):** Interface responsiva projetada com elementos dimensionados para telas móveis[cite: 221, 222, 223].
  - [cite_start]_Teste:_ Redimensionar a janela do navegador em diferentes resoluções e validar a quebra de layout via DevTools[cite: 180, 227, 229].
- [cite_start]**RNF3 - Confiabilidade (ISO 25010):** O ESP32 deve tratar exceções de conexão sem travar a execução principal[cite: 224].
  - [cite_start]_Teste:_ Alterar as credenciais do roteador propositalmente e verificar a emissão do log de erro não-fatal no Serial Monitor[cite: 228, 230].

## 🛠️ Método Experimental e Pipeline de Depuração

[cite_start]As atividades práticas em laboratório seguiram um fluxo incremental e modular focado no isolamento prévio de falhas elétricas e lógicas[cite: 154, 161]:
