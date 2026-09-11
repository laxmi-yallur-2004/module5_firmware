# Module 5 Firmware

## Overview

This module demonstrates timer and scheduling concepts using an Arduino Uno.

### Tasks

### Task 1 – PWM and Duty Cycle

File:

```text
freq+duty+update
```

* Uses Timer2 hardware PWM.
* PWM output is on Arduino D3.
* PWM frequency is approximately 977 Hz.
* Duty cycle changes from 0% to 100%.
* Duty cycle is updated every 1 second using `millis()`.

### Task 2 – Input Frequency Measurement

File:

```text
input freq
```

* Uses Timer1 for time measurement.
* D3 generates the test signal.
* D2 receives the signal using an external interrupt.
* Measures the period between rising edges.
* Calculates the input frequency.
* Expected frequency is approximately 500 Hz.

### Task 3 – Software Timer

File:

```text
scheduler soft timer
```

* Uses `millis()` for timing.
* Task A runs every 1 second.
* Task B runs every 2 seconds.
* Task C runs every 5 seconds.
* Demonstrates a non-blocking software scheduler.

## LCD Connections

| LCD | Arduino |
| --- | ------- |
| RS  | D8      |
| EN  | D9      |
| D4  | D4      |
| D5  | D5      |
| D6  | D6      |
| D7  | D7      |

## Hardware

* Arduino Uno
* ATmega328P
* 16x2 LCD

## Main Concepts

* Timer2 PWM
* Duty-cycle control
* Timer1 frequency measurement
* External interrupts
* `millis()`
* Non-blocking software timers

## Expected Output

### Task 1

```text
PWM: 977 Hz
DUTY: 50%
```

The duty cycle changes during operation.

### Task 2

```text
CAPTURE:
FREQ:500 Hz
```

### Task 3

```text
SOFT TIMER
A:5 B:2 C:1
```

The counters increase according to their configured intervals.

## Non-Blocking Design

The firmware uses `millis()` instead of blocking delays for periodic operations.

This allows the Arduino to continue executing other tasks while timing is being performed.
