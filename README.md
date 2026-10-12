# arduino_signalization_system

## Brief Overview

This project implements a laser-based security alarm system. System periodically samples LDR, at which the laser is pointed, and detects laser beam interrupts (e.g. motion in between the laser and the LDR). In case of a detection, the alarm system is fired off.


## Main Features

- System is a state machine, operating in three states: `OFF`, `ON`, and `DETECTED`.
- System can be turned `ON` or `OFF` with a black button. The blue LED indicates the state `ON`.
- When system is on, LDR is periodically sampled using Timer1.
- When system is on, a user can calibrate the LDR light threshold, at which the system detects motion, with a red button press.
- Alarm system consists of a red LED and a buzzer sound.
- Light threshold and last system state is recovered after a power loss.
- Serial output for sensor readings, system events, and calibration results.

## Build Steps

- Connect the physical components according to the schema presented at the end of the README.
- Connect Arduino Uno R3 to your computer.
- Using Arduino IDE (recommended) or other platform, compile and upload code to the Arduino Uno R3.

## Used Components

- Arduino Uno R3.
- IDUINO laser module.
- Light-dependent resistor (LDR).
- Three 220 ohm resistors for LEDs and buzzer and one 1 K ohm resistor for LDR. 
- One black and one red button (different colors for distinction purposes)
- One blue system LED and one red alarm LED.
- Buzzer.
- Breadboard and connecting wires.

## Interrupts

System has 3 interrupts - one per push button and one timer interrupt. Push button interrupts are meant to change the system state (black button) or calibrate the light threshold (red button), while timer interrupt is used for periodic sampling of the LDR, printing LDR values and playing the buzzer.

## Timer

System uses 16-bit Timer1 as its timer. Timer1 uses CTC mode with a prescaler of 1024. One full cycle of the timer approximately 10ms, after which the timer interrupts. Timer config:
```cpp
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;                            // timer itself
  OCR1A = 156;                          // 10 ms / 0.064 ms = 156.25
  TCCR1B |= (1 << WGM12);               // CTC mode: clear timer on compare match
  TIMSK1 |= (1 << OCIE1A);              // enable compare match A interrupt
  TCCR1B |= (1 << CS12) | (1 << CS10);  // set prescaler 1024, this also starts the timer
```

## 6. EEPROM Storage

EEPROM is used to preserve the system state and calibration threshold across power cycles.

The code defines a magic number and version to validate the stored configuration.

### EEPROM Layout

| Address | Size | Field | Description |
|---|---:|---|---|
| 0 | 1 byte | Magic number | Identifies valid project data |
| 1 | 1 byte | Version | Identifies the EEPROM data format |
| 2 | 1 byte | System state | `0` = OFF, `1` = ON |
| 3 onward | `sizeof(int)` bytes | Light threshold | Stores the calibrated LDR threshold |
