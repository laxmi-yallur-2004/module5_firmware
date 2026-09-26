# Module 5 – PWM, Frequency Measurement and Software Timer

## Task 1 – PWM Generation

### What

Generate a PWM signal on **D3 using Timer2**.

### How

* Timer2 is configured for Fast PWM.
* PWM frequency is approximately **977 Hz**.
* Duty cycle changes automatically every 3 seconds.
* Tested duty cycles: `0%, 25%, 50%, 75%, 100%`.
* Exact 0% forces D3 LOW.
* Exact 100% forces D3 HIGH.

### Connection

```text
Arduino D3 → PWM Output
```

### Output

```text
PWM DUTY: 0% -> D3 LOW
PWM DUTY: 25% -> OCR2B=64
PWM DUTY: 50% -> OCR2B=128
PWM DUTY: 75% -> OCR2B=191
PWM DUTY: 100% -> D3 HIGH
```

---

## Task 2 – Frequency Measurement

### What

Measure the frequency of the PWM signal using **Timer1 and INT0**.

### How

* D3 PWM output is connected to D2.
* D2 uses external interrupt INT0.
* Timer1 runs at **2 MHz**.
* Two rising edges are captured.
* The difference between the edges gives the signal period.
* Frequency is calculated from the measured period.
* If there is no signal for 1 second, the frequency becomes invalid.

### Connection

```text
Arduino D3 → Arduino D2
```

### Output

For PWM duty cycles where a waveform is present:

```text
FREQ: ~977 Hz
```

When there is no signal:

```text
FREQUENCY: WAIT - NO SIGNAL
```

LCD:

```text
PWM:50%
FREQ:977 Hz
```

---

## Task 3 – Software Timer

### What

Create three non-blocking software timers using `millis()`.

### Timer Periods

```text
Task A = 1000 ms
Task B = 500 ms
Task C = 250 ms
```

Therefore:

```text
A : B : C = 1 : 2 : 4
```

### How

* `millis()` is used for timing.
* No `delay()` is used.
* The scheduler uses `while()` to catch up missed executions.

### Output

```text
SOFT TIMER A:1 B:2 C:4
SOFT TIMER A:2 B:4 C:8
SOFT TIMER A:3 B:6 C:12
SOFT TIMER A:4 B:8 C:16
SOFT TIMER A:5 B:10 C:20
SOFT TIMER A:6 B:12 C:24
```

---

## Hardware Connections

| Function        | Arduino Pin |
| --------------- | ----------- |
| LCD RS          | D8          |
| LCD EN          | D9          |
| LCD D4          | D4          |
| LCD D5          | D5          |
| LCD D6          | D6          |
| LCD D7          | D7          |
| PWM Output      | D3          |
| Frequency Input | D2          |
| Test Connection | D3 → D2     |

## Serial Settings

```text
Baud Rate: 115200
Format: 8N1
```

## Main Output

```text
==============================
MODULE 5
==============================
PWM: TIMER2 D3
FREQ: TIMER1 D2
D3 -> D2 JUMPER
PWM FREQUENCY: ~977 Hz
TIMER1 RESERVED: FREQUENCY
TIMER2 RESERVED: PWM
==============================
```

## Result

```text
TASK 1 - PWM GENERATION       PASS
TASK 2 - FREQUENCY MEASUREMENT PASS
TASK 3 - SOFTWARE TIMER       PASS
```
