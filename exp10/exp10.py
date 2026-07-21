#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PCS3732 - EXP10
Fechadura Eletrônica com Raspberry Pi 3

Componentes:
- Teclado matricial 4x4 Freenove
- Display LCD1602 I2C Freenove
- Buzzer
- Sensor digital de estado da fechadura
- Atuador da trava: relé, LED ou solenoide

Mapeamento do teclado:
    0-9 -> dígitos da senha
    *   -> apaga último dígito
    #   -> confirma senha
    A   -> limpa entrada
    B   -> mostra status do sensor
    C   -> trava manualmente
    D   -> destrava temporariamente

Bibliotecas esperadas na mesma pasta:
    Keypad.py
    LCD1602.py

Exemplo de execução:
    python3 fechadura_eletronica.py --mode run

Testes isolados:
    python3 fechadura_eletronica.py --mode lcd
    python3 fechadura_eletronica.py --mode keypad
    python3 fechadura_eletronica.py --mode buzzer
    python3 fechadura_eletronica.py --mode sensor
    python3 fechadura_eletronica.py --mode lock
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import time
from pathlib import Path
from typing import Optional

import RPi.GPIO as GPIO

import Keypad
from LCD1602 import CharLCD1602


# ============================================================
# Configuração padrão do hardware
# ============================================================

ROWS = 4
COLS = 4

KEYS = [
    "1", "2", "3", "A",
    "4", "5", "6", "B",
    "7", "8", "9", "C",
    "*", "0", "#", "D",
]

# Pinos BCM usados pela documentação Freenove para teclado matricial.
ROWS_PINS = [16, 20, 21, 26]
COLS_PINS = [19, 13, 6, 5]

# Pinos extras do projeto.
DEFAULT_BUZZER_PIN = 23
DEFAULT_LOCK_PIN = 24
DEFAULT_SENSOR_PIN = 25

DEFAULT_LCD_ADDRESS = 0x27

DEFAULT_CONFIG_FILE = "lock_config.json"


# ============================================================
# LCD
# ============================================================

class LCD:
    def __init__(self, address: int = DEFAULT_LCD_ADDRESS):
        self.address = address
        self.lcd = CharLCD1602()

    def init(self) -> None:
        self.lcd.init_lcd(addr=self.address)
        self.clear()

    def clear(self) -> None:
        self.lcd.clear()

    def write(self, line1: str = "", line2: str = "") -> None:
        self.lcd.clear()
        self.lcd.write(0, 0, line1[:16].ljust(16))
        self.lcd.write(0, 1, line2[:16].ljust(16))


# ============================================================
# Segurança da senha
# ============================================================

def sha256_password(password: str, salt_hex: str) -> str:
    data = bytes.fromhex(salt_hex) + password.encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def create_default_config(path: Path, default_password: str) -> dict:
    salt = os.urandom(16).hex()
    password_hash = sha256_password(default_password, salt)

    config = {
        "salt": salt,
        "password_hash": password_hash,
        "max_attempts": 3,
        "cooldown_seconds": 10,
        "unlock_seconds": 5,
    }

    path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    return config


def load_config(path: Path, default_password: str) -> dict:
    if not path.exists():
        return create_default_config(path, default_password)

    return json.loads(path.read_text(encoding="utf-8"))


def verify_password(password: str, config: dict) -> bool:
    candidate_hash = sha256_password(password, config["salt"])
    stored_hash = config["password_hash"]

    return hmac.compare_digest(candidate_hash, stored_hash)


# ============================================================
# Hardware baixo nível
# ============================================================

class LockHardware:
    def __init__(
        self,
        buzzer_pin: int,
        lock_pin: int,
        sensor_pin: int,
        sensor_active_low: bool = True,
        unlock_active_high: bool = True,
    ):
        self.buzzer_pin = buzzer_pin
        self.lock_pin = lock_pin
        self.sensor_pin = sensor_pin
        self.sensor_active_low = sensor_active_low
        self.unlock_active_high = unlock_active_high

        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)

        GPIO.setup(self.buzzer_pin, GPIO.OUT)
        GPIO.setup(self.lock_pin, GPIO.OUT)

        # Sensor digital com pull-up interno.
        # Ligação típica:
        #   GPIO ---- sensor/botão ---- GND
        GPIO.setup(self.sensor_pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

        self.buzzer_off()
        self.lock()

    def cleanup(self) -> None:
        self.buzzer_off()
        self.lock()
        GPIO.cleanup()

    def buzzer_on(self) -> None:
        GPIO.output(self.buzzer_pin, GPIO.HIGH)

    def buzzer_off(self) -> None:
        GPIO.output(self.buzzer_pin, GPIO.LOW)

    def beep(self, duration: float = 0.08, count: int = 1, gap: float = 0.08) -> None:
        for _ in range(count):
            self.buzzer_on()
            time.sleep(duration)
            self.buzzer_off()
            time.sleep(gap)

    def success_beep(self) -> None:
        self.beep(duration=0.06, count=2, gap=0.06)

    def error_beep(self) -> None:
        self.beep(duration=0.35, count=1, gap=0.05)

    def alarm_beep(self) -> None:
        self.beep(duration=0.10, count=5, gap=0.06)

    def unlock(self) -> None:
        value = GPIO.HIGH if self.unlock_active_high else GPIO.LOW
        GPIO.output(self.lock_pin, value)

    def lock(self) -> None:
        value = GPIO.LOW if self.unlock_active_high else GPIO.HIGH
        GPIO.output(self.lock_pin, value)

    def is_locked_sensor(self) -> bool:
        raw = GPIO.input(self.sensor_pin)

        if self.sensor_active_low:
            return raw == GPIO.LOW

        return raw == GPIO.HIGH


# ============================================================
# Testes isolados
# ============================================================

def test_lcd(address: int) -> None:
    lcd = LCD(address)
    lcd.init()
    lcd.write("Teste LCD", f"I2C {hex(address)}")
    time.sleep(3)
    lcd.write("Fechadura", "LCD OK")
    time.sleep(2)
    lcd.clear()


def test_keypad() -> None:
    keypad = Keypad.Keypad(KEYS, ROWS_PINS, COLS_PINS, ROWS, COLS)
    keypad.setDebounceTime(50)

    print("Teste do teclado matricial.")
    print("Pressione teclas. Ctrl+C para sair.")

    try:
        while True:
            key = keypad.getKey()

            if key != keypad.NULL:
                print(f"Tecla: {key}")

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nEncerrando teste de teclado.")


def test_buzzer(buzzer_pin: int) -> None:
    hw = LockHardware(
        buzzer_pin=buzzer_pin,
        lock_pin=DEFAULT_LOCK_PIN,
        sensor_pin=DEFAULT_SENSOR_PIN,
    )

    print("Teste do buzzer. Ctrl+C para sair.")

    try:
        while True:
            print("Bipe sucesso")
            hw.success_beep()
            time.sleep(1)

            print("Bipe erro")
            hw.error_beep()
            time.sleep(1)

            print("Bipe alerta")
            hw.alarm_beep()
            time.sleep(2)

    except KeyboardInterrupt:
        pass

    finally:
        hw.cleanup()


def test_sensor(sensor_pin: int, sensor_active_low: bool) -> None:
    hw = LockHardware(
        buzzer_pin=DEFAULT_BUZZER_PIN,
        lock_pin=DEFAULT_LOCK_PIN,
        sensor_pin=sensor_pin,
        sensor_active_low=sensor_active_low,
    )

    print("Teste do sensor.")
    print("Altere o estado físico do sensor. Ctrl+C para sair.")

    try:
        last_state = None

        while True:
            locked = hw.is_locked_sensor()

            if locked != last_state:
                status = "TRANCADA" if locked else "ABERTA"
                print(f"Sensor: {status}")
                last_state = locked

            time.sleep(0.05)

    except KeyboardInterrupt:
        pass

    finally:
        hw.cleanup()


def test_lock(lock_pin: int, unlock_active_high: bool) -> None:
    hw = LockHardware(
        buzzer_pin=DEFAULT_BUZZER_PIN,
        lock_pin=lock_pin,
        sensor_pin=DEFAULT_SENSOR_PIN,
        unlock_active_high=unlock_active_high,
    )

    print("Teste do atuador da trava.")
    print("Alternando travado/destravado. Ctrl+C para sair.")

    try:
        while True:
            print("Destravando...")
            hw.unlock()
            time.sleep(2)

            print("Travando...")
            hw.lock()
            time.sleep(2)

    except KeyboardInterrupt:
        pass

    finally:
        hw.cleanup()


# ============================================================
# Aplicação principal
# ============================================================

class ElectronicLockApp:
    def __init__(
        self,
        lcd: LCD,
        keypad,
        hw: LockHardware,
        config: dict,
    ):
        self.lcd = lcd
        self.keypad = keypad
        self.hw = hw
        self.config = config

        self.buffer = ""
        self.failed_attempts = 0
        self.blocked_until = 0.0
        self.unlocked_until: Optional[float] = None

    def reset_input(self) -> None:
        self.buffer = ""
        self.show_idle()

    def show_idle(self) -> None:
        sensor_status = "TRANCADA" if self.hw.is_locked_sensor() else "ABERTA"
        self.lcd.write("Digite senha:", f"Sensor:{sensor_status}")

    def show_buffer(self) -> None:
        masked = "*" * len(self.buffer)
        self.lcd.write("Senha:", masked)

    def is_blocked(self) -> bool:
        return time.monotonic() < self.blocked_until

    def remaining_block_time(self) -> int:
        return max(0, int(self.blocked_until - time.monotonic()))

    def lock_now(self) -> None:
        self.unlocked_until = None
        self.hw.lock()
        self.lcd.write("Sistema", "TRANCADO")
        self.hw.beep(duration=0.05, count=1)
        time.sleep(1)
        self.reset_input()

    def unlock_temporarily(self) -> None:
        unlock_seconds = float(self.config["unlock_seconds"])
        self.unlocked_until = time.monotonic() + unlock_seconds

        self.hw.unlock()
        self.lcd.write("Acesso liberado", f"{unlock_seconds:.0f}s aberto")
        self.hw.success_beep()

    def handle_success(self) -> None:
        self.failed_attempts = 0
        self.unlock_temporarily()

    def handle_failure(self) -> None:
        self.failed_attempts += 1
        self.hw.error_beep()

        max_attempts = int(self.config["max_attempts"])

        if self.failed_attempts >= max_attempts:
            cooldown = float(self.config["cooldown_seconds"])
            self.blocked_until = time.monotonic() + cooldown
            self.failed_attempts = 0

            self.lcd.write("Bloqueado", f"Aguarde {int(cooldown)}s")
            time.sleep(1.5)

        else:
            remaining = max_attempts - self.failed_attempts
            self.lcd.write("Acesso negado", f"Restam {remaining}")
            time.sleep(1.5)

        self.reset_input()

    def submit_password(self) -> None:
        if not self.buffer:
            self.lcd.write("Erro", "Senha vazia")
            self.hw.error_beep()
            time.sleep(1)
            self.reset_input()
            return

        if verify_password(self.buffer, self.config):
            self.handle_success()
        else:
            self.handle_failure()

        self.buffer = ""

    def check_sensor_integrity(self) -> None:
        """
        Exemplo simples de alerta:
        se o sistema deveria estar travado, mas sensor acusa aberto,
        dispara alerta.
        """
        now = time.monotonic()
        should_be_locked = self.unlocked_until is None or now > self.unlocked_until

        if should_be_locked:
            self.hw.lock()

            if not self.hw.is_locked_sensor():
                self.lcd.write("ALERTA", "Tranca aberta")
                self.hw.alarm_beep()
                time.sleep(1.0)
                self.show_idle()

    def update_lock_timeout(self) -> None:
        if self.unlocked_until is None:
            return

        if time.monotonic() >= self.unlocked_until:
            self.hw.lock()
            self.unlocked_until = None
            self.lcd.write("Tempo esgotado", "Trancando...")
            self.hw.beep(duration=0.05, count=1)
            time.sleep(1)
            self.reset_input()

    def handle_key(self, key: str) -> None:
        if self.is_blocked():
            remaining = self.remaining_block_time()
            self.lcd.write("Bloqueado", f"Aguarde {remaining}s")
            return

        if key.isdigit():
            if len(self.buffer) < 6:
                self.buffer += key
                self.show_buffer()
            else:
                self.lcd.write("Limite", "6 digitos max")
                self.hw.error_beep()
                time.sleep(0.8)
                self.show_buffer()

        elif key == "*":
            self.buffer = self.buffer[:-1]
            self.show_buffer()

        elif key == "#":
            self.submit_password()

        elif key == "A":
            self.reset_input()

        elif key == "B":
            status = "TRANCADA" if self.hw.is_locked_sensor() else "ABERTA"
            self.lcd.write("Status sensor", status)
            time.sleep(1.5)
            self.show_buffer() if self.buffer else self.show_idle()

        elif key == "C":
            self.lock_now()

        elif key == "D":
            self.unlock_temporarily()

    def run(self) -> None:
        self.lcd.write("Fechadura", "Inicializando")
        time.sleep(1.5)
        self.show_idle()

        print("Fechadura eletrônica iniciada.")
        print("Teclado: 0-9 senha, * backspace, # confirma, A limpa, B sensor, C trava, D destrava.")
        print("Ctrl+C para sair.")

        try:
            while True:
                key = self.keypad.getKey()

                if key != self.keypad.NULL:
                    self.handle_key(key)

                self.update_lock_timeout()
                self.check_sensor_integrity()

                time.sleep(0.02)

        except KeyboardInterrupt:
            print("\nEncerrando...")

        finally:
            self.lcd.write("Encerrando", "")
            time.sleep(1)
            self.lcd.clear()
            self.hw.cleanup()


# ============================================================
# Main
# ============================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Fechadura eletrônica com Raspberry Pi 3"
    )

    parser.add_argument(
        "--mode",
        choices=["run", "lcd", "keypad", "buzzer", "sensor", "lock"],
        default="run",
        help="modo de execução",
    )

    parser.add_argument(
        "--lcd-address",
        default="0x27",
        help="endereço I2C do LCD, normalmente 0x27 ou 0x3F",
    )

    parser.add_argument(
        "--buzzer-pin",
        type=int,
        default=DEFAULT_BUZZER_PIN,
        help="GPIO BCM do buzzer",
    )

    parser.add_argument(
        "--lock-pin",
        type=int,
        default=DEFAULT_LOCK_PIN,
        help="GPIO BCM do atuador da trava",
    )

    parser.add_argument(
        "--sensor-pin",
        type=int,
        default=DEFAULT_SENSOR_PIN,
        help="GPIO BCM do sensor de tranca",
    )

    parser.add_argument(
        "--sensor-active-high",
        action="store_true",
        help="use se o sensor indicar trancado com nível alto",
    )

    parser.add_argument(
        "--unlock-active-low",
        action="store_true",
        help="use se o relé/atuador destravar com nível baixo",
    )

    parser.add_argument(
        "--config",
        default=DEFAULT_CONFIG_FILE,
        help="arquivo JSON com hash da senha",
    )

    parser.add_argument(
        "--default-password",
        default="1234",
        help="senha padrão usada somente na primeira criação do arquivo de configuração",
    )

    args = parser.parse_args()

    lcd_address = int(args.lcd_address, 16)
    sensor_active_low = not args.sensor_active_high
    unlock_active_high = not args.unlock_active_low

    if args.mode == "lcd":
        test_lcd(lcd_address)
        return

    if args.mode == "keypad":
        test_keypad()
        return

    if args.mode == "buzzer":
        test_buzzer(args.buzzer_pin)
        return

    if args.mode == "sensor":
        test_sensor(args.sensor_pin, sensor_active_low)
        return

    if args.mode == "lock":
        test_lock(args.lock_pin, unlock_active_high)
        return

    if args.mode == "run":
        config = load_config(Path(args.config), args.default_password)

        lcd = LCD(lcd_address)
        lcd.init()

        keypad = Keypad.Keypad(KEYS, ROWS_PINS, COLS_PINS, ROWS, COLS)
        keypad.setDebounceTime(50)

        hw = LockHardware(
            buzzer_pin=args.buzzer_pin,
            lock_pin=args.lock_pin,
            sensor_pin=args.sensor_pin,
            sensor_active_low=sensor_active_low,
            unlock_active_high=unlock_active_high,
        )

        app = ElectronicLockApp(
            lcd=lcd,
            keypad=keypad,
            hw=hw,
            config=config,
        )

        app.run()


if __name__ == "__main__":
    main()