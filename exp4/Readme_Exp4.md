# Controle de Periféricos com PWM e ESP32

1. **Montagem Isolada (LED):** Montagem do LED e dimensionamento do resistor em protoboard, validando a polaridade antes de qualquer conexão de rede.
2. **Teste Blink:** Execução de um código simples de piscada para garantir a integridade do circuito elétrico.
3. **Desenvolvimento Incremental:** Codificação modular e rastreamento de erros lógicos através de mensagens enviadas em tempo real para o _Serial Monitor_.

## 🚀 Como Executar o Projeto

1. Abra o código contido na pasta `/src` utilizando a **Arduino IDE**.
2. Altere as variáveis de configuração de Wi-Fi (`ssid` e `password`) com as credenciais da sua bancada de teste.
3. Configure a taxa de transmissão do terminal em `115200 baud`.
4. Compile e grave o firmware na placa ESP32-C3.
5. Copie o endereço IP impresso no terminal, cole em seu navegador de preferência e gerencie os periféricos em tempo real.

## 🏗️ Arquitetura do Sistema

O sistema é estruturado em três camadas fundamentais:

**Camada Web (Software de Alto Nível):** Dashboard em HTML5, CSS3 e JavaScript executado no navegador do cliente. Controla os periféricos de forma assíncrona usando a API `fetch()` do JavaScript para evitar o recarregamento de página.
**Camada Lógica (ESP32 / RISC-V):** Servidor HTTP assíncrono local que intercepta as requisições de rede. Em nível de silício, a arquitetura RISC-V emprega **E/S Mapeada em Memória (Memory-Mapped I/O)**, acessando os registradores dos periféricos através de instruções nativas de carga e armazenamento (`load`/`store`). 3. **Camada Física (Hardware):** Composta por circuitos de atuação em bancada, contendo um LED de alta frequência com resistor de proteção e um servomotor eletromecânico

## 📝 Respostas Teóricas (Pré-Aula)

Com base no material didático e nas diretrizes fornecidas, seguem as respostas aos questionamentos do roteiro (em formato de documentação para o repositório):

### 1. Acesso a Periféricos no ESP32-C3 e na Arquitetura RISC-V

- **Mapeamento de Periféricos:** Na placa ESP32-C3, o mapeamento é feito estritamente em **memória (Memory-Mapped I/O)**. Leituras e escritas nos registradores dos controladores utilizam as mesmas instruções tradicionais de carga e armazenamento (`load`/`store`) da arquitetura de memória convencional.
- **Transição de Privilégio e Syscalls:** Em arquiteturas RISC-V, o acesso direto ao hardware é protegido. As aplicações em espaço de usuário solicitam serviços ao ambiente de execução através de **chamadas de sistema (_syscalls_)**. Utiliza-se a instrução `ecall` para alterar o nível de privilégio e interagir com segurança com os periféricos.

### 3. O Gargalo do _Busy Waiting_ e suas Alternativas

- **Desvantagens:** A técnica de espera ocupada (_Busy Waiting_) baseia-se em laços de checagem contínuos (`while`). Isso gera um **desperdício crítico de ciclos da CPU**, impedindo que o sistema processe outras tarefas em paralelo, como responder a requisições de rede simultâneas.
- **Alternativas:** As soluções ideais envolvem a delegação do trabalho para periféricos de hardware dedicados (como o LEDC) ou a implementação de E/S direcionada por interrupções (**Interrupt-driven I/O**).

## 📋 Requisitos e Engenharia de Software (ISO 25010)

O desenvolvimento do sistema foi guiado pela especificação rigorosa de requisitos e seus respectivos testes:

- **RF1 - Interface Acessível:** Conectar à rede Wi-Fi via IP do ESP32 pelo celular.
  - _Teste:_ Validar o carregamento completo do HTML sem falhas de requisição GET.
- **RF2 - Controle Lumínico:** Alterar o slider do LED na página web de 0 a 100%.
  - _Teste:_ Verificar a variação linear do brilho físico do LED sem oscilações bruscas.
- **RF3 - Atuação Posicional:** Solicitar ângulos limites de 0° e 180° através da interface web.
  - _Teste:_ Confirmar se o servo atinge o ângulo requerido sem engasgar ou gerar picos de corrente.
- **RNF1 - Eficiência de Desempenho (ISO 25010):** O sistema de PWM deve operar exclusivamente em canais de hardware dedicados.
  - _Teste:_ Monitorar a latência da página web sob carga, provando a ausência de gargalos na CPU.
- **RNF2 - Usabilidade (ISO 25010):** Interface responsiva projetada com elementos dimensionados para telas móveis.
  - _Teste:_ Redimensionar a janela do navegador em diferentes resoluções e validar a quebra de layout via DevTools.
- **RNF3 - Confiabilidade (ISO 25010):** O ESP32 deve tratar exceções de conexão sem travar a execução principal.
  - _Teste:_ Alterar as credenciais do roteador propositalmente e verificar a emissão do log de erro não-fatal no Serial Monitor.
