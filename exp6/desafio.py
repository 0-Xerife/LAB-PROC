#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PCS3732 - EXP6
Desafio standalone - Calculadora binária no Raspberry Pi 3

Versão baseada nas bibliotecas do kit Freenove:
- Keypad.py
- LCD1602.py

Hardware:
- Teclado matricial 4x4
- LCD1602 I2C

Representação:
- Complemento de 2
- Largura padrão: 4 bits

Mapeamento do teclado:
    0 e 1 -> entrada dos bits
    A     -> soma
    B     -> subtração
    C     -> multiplicação
    D     -> divisão
    *     -> fatorial
    #     -> executa
    2     -> limpa tudo
    3     -> apaga último bit

Exemplos:
    0111 A 0001 #  => 7 + 1 = -8 em 4 bits, resultado 1000
    0111 D 0000 #  => erro: divisão por zero
    0111 * #       => 7!
"""

from __future__ import annotations

from time import sleep
import argparse

import Keypad
from LCD1602 import CharLCD1602


# ============================================================
# Configuração Freenove - Matrix Keypad
# ============================================================

ROWS = 4
COLS = 4

KEYS = [
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'
]

# Pinos BCM usados pela documentação do Freenove.
ROWS_PINS = [16, 20, 21, 26]
COLS_PINS = [19, 13, 6, 5]


# ============================================================
# Complemento de 2
# ============================================================

def signed_range(width: int) -> tuple[int, int]:
    if width < 2:
        raise ValueError("Complemento de 2 exige pelo menos 2 bits.")
    return -(1 << (width - 1)), (1 << (width - 1)) - 1


def parse_twos(binary_string: str, width: int) -> int:
    s = binary_string.strip()

    if not s:
        raise ValueError("entrada vazia")

    if any(c not in "01" for c in s):
        raise ValueError("use apenas 0 e 1")

    if len(s) > width:
        raise ValueError(f"max {width} bits")

    s = s.zfill(width)

    unsigned_value = int(s, 2)
    sign_bit = 1 << (width - 1)

    if unsigned_value & sign_bit:
        return unsigned_value - (1 << width)

    return unsigned_value


def to_twos(value: int, width: int) -> str:
    mask = (1 << width) - 1
    return format(value & mask, f"0{width}b")


def wrap_twos(value: int, width: int) -> int:
    mask = (1 << width) - 1
    value &= mask

    sign_bit = 1 << (width - 1)

    if value & sign_bit:
        return value - (1 << width)

    return value


# ============================================================
# Operações
# ============================================================

def calc_add(a: int, b: int, width: int) -> int:
    return wrap_twos(a + b, width)


def calc_sub(a: int, b: int, width: int) -> int:
    return wrap_twos(a - b, width)


def calc_mul(a: int, b: int, width: int) -> int:
    return wrap_twos(a * b, width)


def calc_div(a: int, b: int, width: int) -> int:
    if b == 0:
        raise ZeroDivisionError("div zero")

    # Trunca em direção a zero, como C/C++.
    return wrap_twos(int(a / b), width)


def calc_factorial(a: int, width: int) -> int:
    if a < 0:
        raise ValueError("fat negativo")

    result = 1

    for i in range(2, a + 1):
        result = wrap_twos(result * i, width)

    return wrap_twos(result, width)


# ============================================================
# Interface LCD
# ============================================================

class LCD:
    def __init__(self, address: int | None = None):
        self.lcd = CharLCD1602()
        self.address = address

    def init(self) -> None:
        if self.address is None:
            self.lcd.init_lcd()
        else:
            self.lcd.init_lcd(addr=self.address)

    def clear(self) -> None:
        self.lcd.clear()

    def write(self, line1: str = "", line2: str = "") -> None:
        self.lcd.clear()
        self.lcd.write(0, 0, line1[:16].ljust(16))
        self.lcd.write(0, 1, line2[:16].ljust(16))


# ============================================================
# Calculadora standalone
# ============================================================

class StandaloneCalculator:
    def __init__(self, width: int, lcd: LCD, keypad):
        self.width = width
        self.lcd = lcd
        self.keypad = keypad

        self.a_bin = ""
        self.b_bin = ""
        self.op = None

    def reset(self) -> None:
        self.a_bin = ""
        self.b_bin = ""
        self.op = None
        self.redraw()

    def backspace(self) -> None:
        if self.op is None or self.op == "!":
            self.a_bin = self.a_bin[:-1]
        else:
            self.b_bin = self.b_bin[:-1]

        self.redraw()

    def expression_text(self) -> str:
        if self.op is None:
            return "A:" + self.a_bin

        if self.op == "!":
            return "A:" + self.a_bin + "!"

        return self.a_bin + self.op + self.b_bin

    def redraw(self) -> None:
        expr = self.expression_text()

        if self.op is None:
            help_line = "A+B-C*D/ *=fat"
        else:
            help_line = "#OK 2CLR 3DEL"

        self.lcd.write(expr, help_line)

    def input_bit(self, bit: str) -> None:
        if self.op is None or self.op == "!":
            if len(self.a_bin) < self.width:
                self.a_bin += bit
            else:
                self.lcd.write("Limite", f"{self.width} bits")
                sleep(0.7)
        else:
            if len(self.b_bin) < self.width:
                self.b_bin += bit
            else:
                self.lcd.write("Limite", f"{self.width} bits")
                sleep(0.7)

        self.redraw()

    def set_operator(self, op: str) -> None:
        if not self.a_bin:
            self.lcd.write("Erro", "Digite A")
            sleep(1.0)
            self.redraw()
            return

        self.op = op
        self.redraw()

    def execute(self) -> None:
        try:
            if not self.a_bin:
                self.lcd.write("Erro", "A vazio")
                sleep(1.2)
                self.redraw()
                return

            a = parse_twos(self.a_bin, self.width)

            if self.op is None:
                result = a
                expr = f"A={a}"

            elif self.op == "!":
                result = calc_factorial(a, self.width)
                expr = f"{a}!"

            else:
                if not self.b_bin:
                    self.lcd.write("Erro", "B vazio")
                    sleep(1.2)
                    self.redraw()
                    return

                b = parse_twos(self.b_bin, self.width)

                if self.op == "+":
                    result = calc_add(a, b, self.width)
                elif self.op == "-":
                    result = calc_sub(a, b, self.width)
                elif self.op == "*":
                    result = calc_mul(a, b, self.width)
                elif self.op == "/":
                    result = calc_div(a, b, self.width)
                else:
                    raise ValueError("op invalida")

                expr = f"{a}{self.op}{b}"

            result_bin = to_twos(result, self.width)

            self.lcd.write(f"{expr}={result}", f"BIN:{result_bin}")
            sleep(3)
            self.reset()

        except ZeroDivisionError:
            self.lcd.write("ERRO", "Divisao por 0")
            sleep(2)
            self.reset()

        except Exception as exc:
            self.lcd.write("ERRO", str(exc)[:16])
            sleep(2)
            self.reset()

    def handle_key(self, key: str) -> None:
        if key == '0' or key == '1':
            self.input_bit(key)

        elif key == 'A':
            self.set_operator("+")

        elif key == 'B':
            self.set_operator("-")

        elif key == 'C':
            self.set_operator("*")

        elif key == 'D':
            self.set_operator("/")

        elif key == '*':
            self.set_operator("!")

        elif key == '#':
            self.execute()

        elif key == '2':
            self.reset()

        elif key == '3':
            self.backspace()

        # Demais teclas 4,5,6,7,8,9 são ignoradas.

    def run(self) -> None:
        min_value, max_value = signed_range(self.width)

        self.lcd.write("Calc Binaria", f"C2 {min_value}..{max_value}")
        sleep(2)
        self.redraw()

        while True:
            key = self.keypad.getKey()

            if key != self.keypad.NULL:
                self.handle_key(key)

            sleep(0.01)


# ============================================================
# Main
# ============================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Calculadora standalone com Freenove Keypad + LCD1602"
    )

    parser.add_argument(
        "--width",
        type=int,
        default=4,
        help="largura em bits para complemento de 2",
    )

    parser.add_argument(
        "--lcd-address",
        default=None,
        help="endereço I2C do LCD, exemplo: 0x27 ou 0x3F. Se omitido, usa padrão da biblioteca.",
    )

    args = parser.parse_args()

    if args.width < 2:
        raise ValueError("width deve ser pelo menos 2")

    lcd_address = None
    if args.lcd_address is not None:
        lcd_address = int(args.lcd_address, 16)

    lcd = LCD(address=lcd_address)
    lcd.init()

    keypad = Keypad.Keypad(KEYS, ROWS_PINS, COLS_PINS, ROWS, COLS)
    keypad.setDebounceTime(50)

    app = StandaloneCalculator(
        width=args.width,
        lcd=lcd,
        keypad=keypad,
    )

    try:
        app.run()

    except KeyboardInterrupt:
        lcd.write("Encerrando", "")
        sleep(1)
        lcd.clear()


if __name__ == "__main__":
    main()


