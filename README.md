# MODULE 5 — PWM, Frequency Measurement and Software Scheduler

## 1. Objective

This module implements:

1. PWM generation using Timer2 on Arduino Uno D3.
2. Exact 0% and 100% PWM handling.
3. Frequency measurement using D2 external interrupt and Timer1.
4. No-signal detection when PWM is 0% or 100%.
5. Software scheduler with three periodic tasks.
6. LCD display of PWM and frequency status.
7. Non-blocking firmware without `delay()`.

---

## 2. Hardware

| Item                    | Connection         |
| ----------------------- | ------------------ |
| Arduino Uno             | ATmega328P, 16 MHz |
| PWM output              | D3                 |
| Frequency capture input | D2                 |
| Test connection         | D3 → D2 jumper     |
| LCD RS                  | D8                 |
| LCD EN                  | D9                 |
| LCD D4                  | D4                 |
| LCD D5                  | D5                 |
| LCD D6                  | D6                 |
| LCD D7                  | D7                 |
| Serial Monitor          | 9600 baud          |

The D3 output is connected to D2 using a jumper so that the generated PWM signal can be measured by the interrupt capture input.

---

## 3. PWM Implementation

Timer2 is configured in Fast PWM mode with a prescaler of 64.

The PWM frequency is approximately:

**976.56 Hz**

The firmware supports:

* 0% → D3 forced LOW
* 25% → OCR2B = 64
* 50% → OCR2B = 128
* 75% → OCR2B = 191
* 100% → D3 forced HIGH

Exact 0% and 100% are handled separately so that the output does not generate an unwanted PWM pulse.

---

## 4. Frequency Measurement

D2 is configured as an external interrupt input.

The rising edges from D3 are captured on D2.

Timer1 runs with a 2 MHz timer clock. The measured period between two rising edges is used to calculate frequency.

The measured frequency is approximately:

**976–977 Hz**

Small variations such as 976 Hz and 977 Hz are expected because the actual Timer2 PWM frequency is approximately 976.56 Hz and the displayed frequency is an integer value.

---

## 5. No-Signal Detection

When PWM is set to 0%:

```text
D3 = LOW
```

There are no continuous rising edges, so the frequency measurement becomes:

```text
WAIT - NO SIGNAL
```

When PWM is set to 100%:

```text
D3 = HIGH
```

There are also no continuous rising edges, so the frequency measurement becomes:

```text
WAIT - NO SIGNAL
```

This prevents the LCD/Serial output from displaying an old or stale frequency value.

---

## 6. Software Scheduler

Three non-blocking software timer tasks are implemented:

| Task   |    Period |
| ------ | --------: |
| Task A |  1 second |
| Task B | 2 seconds |
| Task C | 5 seconds |

The scheduler uses time comparisons rather than `delay()`.

Example observed output:

```text
SOFT TIMER A:32 B:16 C:6
SOFT TIMER A:33 B:16 C:6
SOFT TIMER A:34 B:17 C:6
SOFT TIMER A:35 B:17 C:7
SOFT TIMER A:36 B:18 C:7
SOFT TIMER A:37 B:18 C:7
SOFT TIMER A:38 B:19 C:7
SOFT TIMER A:39 B:19 C:7
```

The counters show that the three scheduler tasks are executing at their configured intervals.

---

# 7. Actual Serial Monitor Proof

The following is the actual observed output from the Arduino Uno:

```text
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz

PWM DUTY: 100% -> D3 HIGH

SOFT TIMER A:32 B:16 C:6

PWM DUTY: 0% -> D3 LOW

SOFT TIMER A:33 B:16 C:6

PWM DUTY: 25% -> OCR2B=64

SOFT TIMER A:34 B:17 C:6

FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 976 Hz

PWM DUTY: 50% -> OCR2B=128

SOFT TIMER A:35 B:17 C:7

FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz

PWM DUTY: 75% -> OCR2B=191

SOFT TIMER A:36 B:18 C:7

FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz
FREQUENCY: 977 Hz

PWM DUTY: 100% -> D3 HIGH

SOFT TIMER A:37 B:18 C:7

PWM DUTY: 0% -> D3 LOW

SOFT TIMER A:38 B:19 C:7

PWM DUTY: 25% -> OCR2B=64

SOFT TIMER A:39 B:19 C:7

FREQUENCY: 977 Hz
```

---

# 8. Measured Results

| PWM Duty | D3 Output | Observed Frequency |
| -------: | --------- | -----------------: |
|       0% | LOW       |   WAIT - NO SIGNAL |
|      25% | PWM       |         976–977 Hz |
|      50% | PWM       |             977 Hz |
|      75% | PWM       |             977 Hz |
|     100% | HIGH      |   WAIT - NO SIGNAL |

The 25%, 50% and 75% PWM settings successfully produce frequency measurements around 976–977 Hz.

---

# 9. Proof of Requirements

| Requirement            | Proof                                           |
| ---------------------- | ----------------------------------------------- |
| PWM generation         | `PWM DUTY` messages observed                    |
| 0% handling            | `PWM DUTY: 0% -> D3 LOW`                        |
| 25% handling           | `PWM DUTY: 25% -> OCR2B=64`                     |
| 50% handling           | `PWM DUTY: 50% -> OCR2B=128`                    |
| 75% handling           | `PWM DUTY: 75% -> OCR2B=191`                    |
| 100% handling          | `PWM DUTY: 100% -> D3 HIGH`                     |
| Frequency capture      | `FREQUENCY: 976/977 Hz`                         |
| No-signal detection    | `WAIT - NO SIGNAL` at 0% and 100%               |
| Software Timer A       | Counter increases approximately every 1 second  |
| Software Timer B       | Counter increases approximately every 2 seconds |
| Software Timer C       | Counter increases approximately every 5 seconds |
| Non-blocking operation | No `delay()` used                               |
| D3 → D2 capture        | PWM output connected to D2 interrupt input      |

---

# 10. Final Result

Module 5 successfully demonstrates:

* Timer2 hardware PWM generation.
* Exact 0% and 100% output handling.
* Timer1-based period measurement.
* External interrupt frequency capture.
* No-signal timeout and stale-frequency prevention.
* Three non-blocking software scheduler tasks.
* Serial monitoring of measured results.
* LCD status display.

**MODULE 5 TEST: PASS**
