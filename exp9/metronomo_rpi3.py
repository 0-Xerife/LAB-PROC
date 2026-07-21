#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PCS3732 - EXP Metrônomo no Raspberry Pi 3

Funcionalidades:
- LED controlado por PWM.
- Servomotor controlado por PWM.
- Buzzer acionado a cada batida.
- Temporização com correção de drift.
- Botões físicos para aumentar/diminuir BPM.
- Botão opcional para ligar/desligar som.

Biblioteca usada:
    gpiozero

Instalação:
    sudo apt update
    sudo apt install -y python3-gpiozero

Opcional, para servo mais estável:
    sudo apt install -y pigpio python3-pigpio
    sudo systemctl enable pigpiod
    sudo systemctl start pigpiod

Exemplos de execução:
    python3 metronomo_rpi3.py --mode led
    python3 metronomo_rpi3.py --mode servo
    python3 metronomo_rpi3.py --mode buzzer
    python3 metronomo_rpi3.py --mode buttons
    python3 metronomo_rpi3.py --mode run --bpm 60

Com pigpio:
    python3 metronomo_rpi3.py --mode run --bpm 60 --pigpio
"""

from __future__ import annotations

import argparse
import signal
import sys
import time
from dataclasses import dataclass

from gpiozero import PWMLED, Button, Buzzer, AngularServo, PWMOutputDevice
from gpiozero.pins.native import NativeFactory


# ============================================================
# Configuração padrão de pinos BCM
# ============================================================

DEFAULT_LED_PIN = 18
DEFAULT_SERVO_PIN = 13
DEFAULT_BUZZER_PIN = 23

DEFAULT_BTN_UP_PIN = 5
DEFAULT_BTN_DOWN_PIN = 6
DEFAULT_BTN_SOUND_PIN = 16


# ============================================================
# Estado do metrônomo
# ============================================================

@dataclass
class MetronomeState:
    bpm: int = 60
    min_bpm: int = 30
    max_bpm: int = 240
    bpm_step: int = 5
    sound_enabled: bool = True
    running: bool = True

    def period_seconds(self) -> float:
        return 60.0 / self.bpm

    def increase_bpm(self) -> None:
        self.bpm = min(self.max_bpm, self.bpm + self.bpm_step)
        print(f"BPM aumentado para {self.bpm}")

    def decrease_bpm(self) -> None:
        self.bpm = max(self.min_bpm, self.bpm - self.bpm_step)
        print(f"BPM diminuído para {self.bpm}")

    def toggle_sound(self) -> None:
        self.sound_enabled = not self.sound_enabled
        status = "ligado" if self.sound_enabled else "desligado"
        print(f"Som {status}")


# ============================================================
# Factory de GPIO
# ============================================================

def get_pin_factory(use_pigpio: bool):
    if not use_pigpio:
        return NativeFactory()

    try:
        from gpiozero.pins.pigpio import PiGPIOFactory
    except ImportError as exc:
        raise RuntimeError(
            "pigpio não instalado. Rode: sudo apt install -y pigpio python3-pigpio"
        ) from exc

    return PiGPIOFactory()


# ============================================================
# Testes isolados
# ============================================================

def test_led_pwm(led_pin: int, factory) -> None:
    """
    Testa LED PWM variando duty cycle.
    """
    led = PWMLED(led_pin, frequency=1000, pin_factory=factory)

    print("Teste LED PWM")
    print("Frequência: 1000 Hz")
    print("Variando duty cycle de 0% até 100%. Pressione Ctrl+C para sair.")

    try:
        while True:
            for i in range(0, 101, 5):
                led.value = i / 100.0
                print(f"Duty cycle: {i}%")
                time.sleep(0.08)

            for i in range(100, -1, -5):
                led.value = i / 100.0
                print(f"Duty cycle: {i}%")
                time.sleep(0.08)

    except KeyboardInterrupt:
        pass

    finally:
        led.off()
        led.close()


def test_servo(servo_pin: int, factory) -> None:
    """
    Testa servo em três posições.
    Pulso típico:
    - 1,0 ms
    - 1,5 ms
    - 2,0 ms
    """
    servo = AngularServo(
        servo_pin,
        min_angle=-90,
        max_angle=90,
        min_pulse_width=0.001,
        max_pulse_width=0.002,
        frame_width=0.020,
        pin_factory=factory,
    )

    print("Teste Servo")
    print("Alternando entre -45°, 0° e 45°. Pressione Ctrl+C para sair.")
    print("Se o servo tremer, use --pigpio e confira alimentação/GND comum.")

    try:
        while True:
            for angle in [-45, 0, 45, 0]:
                print(f"Servo angle = {angle}°")
                servo.angle = angle
                time.sleep(1.0)

    except KeyboardInterrupt:
        pass

    finally:
        servo.angle = 0
        time.sleep(0.3)
        servo.detach()
        servo.close()


def test_buzzer(buzzer_pin: int, factory, passive_buzzer: bool) -> None:
    """
    Testa buzzer.
    - Buzzer ativo: usa Buzzer on/off.
    - Buzzer passivo: usa PWMOutputDevice com frequência audível.
    """
    print("Teste Buzzer")
    print("Emitindo pulso sonoro curto a cada 1 segundo. Pressione Ctrl+C para sair.")

    if passive_buzzer:
        buzzer = PWMOutputDevice(
            buzzer_pin,
            frequency=2000,
            initial_value=0,
            pin_factory=factory,
        )
    else:
        buzzer = Buzzer(buzzer_pin, pin_factory=factory)

    try:
        while True:
            if passive_buzzer:
                buzzer.value = 0.5
                time.sleep(0.08)
                buzzer.value = 0.0
            else:
                buzzer.on()
                time.sleep(0.08)
                buzzer.off()

            print("BEEP")
            time.sleep(0.92)

    except KeyboardInterrupt:
        pass

    finally:
        if passive_buzzer:
            buzzer.value = 0.0
        else:
            buzzer.off()
        buzzer.close()


def test_buttons(btn_up_pin: int, btn_down_pin: int, btn_sound_pin: int, factory) -> None:
    """
    Testa botões físicos com debounce.
    Ligação recomendada:
    - um terminal do botão no GPIO;
    - outro terminal no GND;
    - pull-up interno habilitado.
    """
    state = MetronomeState()

    btn_up = Button(btn_up_pin, pull_up=True, bounce_time=0.2, pin_factory=factory)
    btn_down = Button(btn_down_pin, pull_up=True, bounce_time=0.2, pin_factory=factory)
    btn_sound = Button(btn_sound_pin, pull_up=True, bounce_time=0.2, pin_factory=factory)

    btn_up.when_pressed = state.increase_bpm
    btn_down.when_pressed = state.decrease_bpm
    btn_sound.when_pressed = state.toggle_sound

    print("Teste Botões")
    print(f"Botão +     GPIO {btn_up_pin}")
    print(f"Botão -     GPIO {btn_down_pin}")
    print(f"Botão som   GPIO {btn_sound_pin}")
    print("Pressione os botões. Ctrl+C para sair.")

    try:
        while True:
            time.sleep(0.2)

    except KeyboardInterrupt:
        pass

    finally:
        btn_up.close()
        btn_down.close()
        btn_sound.close()


# ============================================================
# Metrônomo integrado
# ============================================================

class Metronome:
    def __init__(
        self,
        led_pin: int,
        servo_pin: int,
        buzzer_pin: int,
        btn_up_pin: int,
        btn_down_pin: int,
        btn_sound_pin: int,
        bpm: int,
        passive_buzzer: bool,
        factory,
    ):
        self.state = MetronomeState(bpm=bpm)
        self.passive_buzzer = passive_buzzer

        self.led = PWMLED(led_pin, frequency=1000, pin_factory=factory)

        self.servo = AngularServo(
            servo_pin,
            min_angle=-90,
            max_angle=90,
            min_pulse_width=0.001,
            max_pulse_width=0.002,
            frame_width=0.020,
            pin_factory=factory,
        )

        if passive_buzzer:
            self.buzzer = PWMOutputDevice(
                buzzer_pin,
                frequency=2000,
                initial_value=0,
                pin_factory=factory,
            )
        else:
            self.buzzer = Buzzer(buzzer_pin, pin_factory=factory)

        self.btn_up = Button(btn_up_pin, pull_up=True, bounce_time=0.2, pin_factory=factory)
        self.btn_down = Button(btn_down_pin, pull_up=True, bounce_time=0.2, pin_factory=factory)
        self.btn_sound = Button(btn_sound_pin, pull_up=True, bounce_time=0.2, pin_factory=factory)

        self.btn_up.when_pressed = self.state.increase_bpm
        self.btn_down.when_pressed = self.state.decrease_bpm
        self.btn_sound.when_pressed = self.state.toggle_sound

        self.servo_left = -45
        self.servo_right = 45
        self.current_side = False

    def beep_on(self) -> None:
        if not self.state.sound_enabled:
            return

        if self.passive_buzzer:
            self.buzzer.value = 0.5
        else:
            self.buzzer.on()

    def beep_off(self) -> None:
        if self.passive_buzzer:
            self.buzzer.value = 0.0
        else:
            self.buzzer.off()

    def tick(self) -> None:
        """
        Executa uma batida do metrônomo.
        """
        self.current_side = not self.current_side
        angle = self.servo_left if self.current_side else self.servo_right

        self.servo.angle = angle

        self.led.value = 1.0
        self.beep_on()

        time.sleep(0.06)

        self.led.value = 0.0
        self.beep_off()

        print(
            f"Tick | BPM={self.state.bpm} | "
            f"período={self.state.period_seconds():.3f}s | "
            f"servo={angle}° | "
            f"som={'on' if self.state.sound_enabled else 'off'}"
        )

    def run(self) -> None:
        """
        Loop principal com correção de drift.

        A lógica usa o instante absoluto da próxima batida:
            next_tick += period

        Isso evita acumular erro como ocorreria com sleep(1) simples.
        """
        print("=" * 70)
        print("Metrônomo Raspberry Pi 3")
        print("Ctrl+C para sair")
        print("=" * 70)
        print(f"BPM inicial: {self.state.bpm}")
        print("Botões:")
        print("  + BPM")
        print("  - BPM")
        print("  Liga/desliga som")
        print("=" * 70)

        next_tick = time.monotonic()

        try:
            while self.state.running:
                now = time.monotonic()

                if now < next_tick:
                    time.sleep(next_tick - now)

                cycle_start = time.monotonic()

                self.tick()

                period = self.state.period_seconds()
                next_tick += period

                # Caso o sistema atrase muito, ressincroniza para evitar sequência de ticks acumulados.
                if time.monotonic() - next_tick > period:
                    next_tick = time.monotonic() + period

                cycle_end = time.monotonic()
                drift_ms = (cycle_end - cycle_start) * 1000.0

                # Log simples do tempo gasto na atuação.
                print(f"Tempo de atuação: {drift_ms:.2f} ms")

        except KeyboardInterrupt:
            print("\nEncerrando metrônomo...")

        finally:
            self.cleanup()

    def cleanup(self) -> None:
        self.led.off()
        self.beep_off()
        self.servo.angle = 0
        time.sleep(0.2)

        self.servo.detach()

        self.led.close()
        self.servo.close()
        self.buzzer.close()
        self.btn_up.close()
        self.btn_down.close()
        self.btn_sound.close()


# ============================================================
# Main
# ============================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Metrônomo com Raspberry Pi 3, PWM, servo, buzzer e botões."
    )

    parser.add_argument(
        "--mode",
        choices=["led", "servo", "buzzer", "buttons", "run"],
        default="run",
        help="modo de execução",
    )

    parser.add_argument("--bpm", type=int, default=60, help="BPM inicial")
    parser.add_argument("--passive-buzzer", action="store_true", help="usar PWM para buzzer passivo")
    parser.add_argument("--pigpio", action="store_true", help="usar PiGPIOFactory para PWM mais estável")

    parser.add_argument("--led-pin", type=int, default=DEFAULT_LED_PIN, help="GPIO BCM do LED")
    parser.add_argument("--servo-pin", type=int, default=DEFAULT_SERVO_PIN, help="GPIO BCM do servo")
    parser.add_argument("--buzzer-pin", type=int, default=DEFAULT_BUZZER_PIN, help="GPIO BCM do buzzer")

    parser.add_argument("--btn-up-pin", type=int, default=DEFAULT_BTN_UP_PIN, help="GPIO BCM botão BPM+")
    parser.add_argument("--btn-down-pin", type=int, default=DEFAULT_BTN_DOWN_PIN, help="GPIO BCM botão BPM-")
    parser.add_argument("--btn-sound-pin", type=int, default=DEFAULT_BTN_SOUND_PIN, help="GPIO BCM botão liga/desliga som")

    args = parser.parse_args()

    if args.bpm < 30 or args.bpm > 240:
        raise ValueError("Use BPM inicial entre 30 e 240.")

    factory = get_pin_factory(use_pigpio=args.pigpio)

    if args.mode == "led":
        test_led_pwm(args.led_pin, factory)

    elif args.mode == "servo":
        test_servo(args.servo_pin, factory)

    elif args.mode == "buzzer":
        test_buzzer(args.buzzer_pin, factory, args.passive_buzzer)

    elif args.mode == "buttons":
        test_buttons(
            args.btn_up_pin,
            args.btn_down_pin,
            args.btn_sound_pin,
            factory,
        )

    elif args.mode == "run":
        metronome = Metronome(
            led_pin=args.led_pin,
            servo_pin=args.servo_pin,
            buzzer_pin=args.buzzer_pin,
            btn_up_pin=args.btn_up_pin,
            btn_down_pin=args.btn_down_pin,
            btn_sound_pin=args.btn_sound_pin,
            bpm=args.bpm,
            passive_buzzer=args.passive_buzzer,
            factory=factory,
        )

        metronome.run()


if __name__ == "__main__":
    main()