.data
GPIO_OUT: .word 0x10000000  # Endereço simulado do hardware

.text
.global _start

_start:
    lui t0, 0x10000         # Carrega a base do endereço (0x10000000)
    
loop:
    # --- ESTADO VERDE ---
    li t1, 0x1              # 0x1 = Bit 0 ligado (Verde)
    sw t1, 0(t0)            # Escreve no hardware
    jal delay               # Chama rotina de delay

    # --- ESTADO VERMELHO ---
    li t1, 0x2              # 0x2 = Bit 1 ligado (Vermelho)
    sw t1, 0(t0)            # Escreve no hardware
    jal delay

    # --- ESTADO AMARELO ---
    li t1, 0x4              # 0x4 = Bit 2 ligado (Amarelo)
    sw t1, 0(t0)            # Escreve no hardware
    jal delay
    
    j loop                  # Repete o ciclo

delay:                      # Simples loop de espera
    li t2, 100000
d_loop:
    addi t2, t2, -1
    bnez t2, d_loop
    ret