# MODULE 5 – PWM, FREQUENCY MEASUREMENT AND SOFTWARE TIMER

## 1. What We Did

In this module we implemented:

* PWM generation using Timer2 on D3
* Frequency measurement using Timer1 on D2
* D3 to D2 jumper for self-test
* 0%, 25%, 50% and 100% PWM duty cycle
* No-signal detection
* Software timers
* LCD monitoring

The code uses no `delay()` and no dynamic memory.

---

# TASK 1 – PWM GENERATION

PWM is generated using **Timer2 on D3**.

PWM frequency:

```text
~977 Hz
```

### PWM Duty Cycle Sequence

```text
100% → 0% → 25% → 50% → 100%
```

### Actual Output

```text
PWM DUTY: 0% -> D3 LOW
PWM DUTY: 25% -> OCR2B=64
PWM DUTY: 50% -> OCR2B=128
PWM DUTY: 100% -> D3 HIGH
```

0% and 100% are handled specially:

```text
0%   → D3 LOW
100% → D3 HIGH
```

---

# TASK 2 – FREQUENCY MEASUREMENT

Frequency is measured using **Timer1 on D2**.

The self-test connection is:

```text
D3 → D2
```

Timer1 runs at:

```text
16 MHz / 8 = 2 MHz
```

When there is no valid signal for the timeout period, the previous frequency is cleared.

### Actual Output

```text
FREQUENCY: WAIT - NO SIGNAL
```

---

# TASK 3 – SOFTWARE TIMER

Three non-blocking software timers are used.

| Timer |  Period |
| ----- | ------: |
| A     | 1000 ms |
| B     |  500 ms |
| C     |  250 ms |

### Actual Output

```text
SOFT TIMER A:1 B:1 C:3
SOFT TIMER A:2 B:3 C:7
SOFT TIMER A:3 B:5 C:11
SOFT TIMER A:4 B:7 C:15
SOFT TIMER A:5 B:9 C:19
SOFT TIMER A:6 B:11 C:23
SOFT TIMER A:7 B:13 C:27
SOFT TIMER A:8 B:15 C:31
SOFT TIMER A:9 B:17 C:35
SOFT TIMER A:10 B:19 C:39
SOFT TIMER A:11 B:21 C:43
SOFT TIMER A:12 B:23 C:47
SOFT TIMER A:13 B:25 C:51
SOFT TIMER A:14 B:27 C:55
```

---

# TASK 4 – LCD DISPLAY

The LCD displays:

* PWM duty cycle
* Frequency
* Software timer counters
* Timer1 information

When no frequency signal is available, the LCD shows:

```text
FREQ:WAIT
```

---

# TIMER ALLOCATION

```text
Timer1 → Frequency Measurement
Timer2 → PWM Generation
Timer0 → Arduino millis()
```

Serial output:

```text
PWM: TIMER2 D3
FREQ: TIMER1 D2
D3 -> D2 JUMPER
PWM FREQUENCY: ~977 Hz
TIMER1 RESERVED: FREQUENCY
TIMER2 RESERVED: PWM
```

---

# ACTUAL SERIAL MONITOR OUTPUT

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
SOFT TIMER A:1 B:1 C:3
SOFT TIMER A:2 B:3 C:7
SOFT TIMER A:3 B:5 C:11
PWM DUTY: 0% -> D3 LOW
FREQUENCY: WAIT - NO SIGNAL
SOFT TIMER A:4 B:7 C:15
SOFT TIMER A:5 B:9 C:19
SOFT TIMER A:6 B:11 C:23
PWM DUTY: 25% -> OCR2B=64
SOFT TIMER A:7 B:13 C:27
SOFT TIMER A:8 B:15 C:31
SOFT TIMER A:9 B:17 C:35
PWM DUTY: 50% -> OCR2B=128
SOFT TIMER A:10 B:19 C:39
SOFT TIMER A:11 B:21 C:43
SOFT TIMER A:12 B:23 C:47
PWM DUTY: 100% -> D3 HIGH
SOFT TIMER A:13 B:25 C:51
FREQUENCY: WAIT - NO SIGNAL
SOFT TIMER A:14 B:27 C:55
```

# RESULT

Module 5 successfully implements:

```text
Timer2 PWM
Timer1 Frequency Measurement
0%, 25%, 50%, 100% Duty Cycle
No-Signal Detection
Software Timers
LCD Monitoring
D3 → D2 Self-Test
```
