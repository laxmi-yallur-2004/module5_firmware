# Module 5 – PWM, Frequency Measurement and Software Timer

## Task 1 – PWM Generation

### Objective

Generate PWM using **Timer2 on D3** and change the duty cycle automatically.

### Configuration

* Timer2: Fast PWM
* PWM Output: D3
* Prescaler: 64
* PWM Frequency: approximately **977 Hz**

### Duty Cycle Sequence

```text
50% → 0% → 25% → 50% → 75% → 100% → 0% → ...
```

### Expected Output

```text
PWM DUTY: 50% -> OCR2B=128
PWM DUTY: 0% -> D3 LOW
PWM DUTY: 25% -> OCR2B=64
PWM DUTY: 50% -> OCR2B=128
PWM DUTY: 75% -> OCR2B=191
PWM DUTY: 100% -> D3 HIGH
```

---

## Task 2 – Frequency Measurement

### Objective

Measure the PWM frequency using **Timer1 and INT0**.

### Configuration

* Frequency input: D2 / INT0
* Timer1 prescaler: 8
* Timer1 frequency: **2 MHz**
* Timer1 tick: **0.5 µs**
* Frequency calculation:

```text
Frequency = 2,000,000 / PeriodTicks
```

The PWM signal from D3 is measured through D2.

### Expected Result

For active PWM duty cycles such as 25%, 50%, and 75%:

```text
Frequency ≈ 977 Hz
```

For 0% and 100%, the output is constant, so there are no rising edges:

```text
FREQUENCY: WAIT - NO SIGNAL
```

---

## Task 3 – Software Timer

### Objective

Implement three non-blocking software timers using `millis()`.

| Task   |  Period |
| ------ | ------: |
| Task A | 1000 ms |
| Task B |  500 ms |
| Task C |  250 ms |

### Expected Output

```text
SOFT TIMER A:1 B:2 C:4
SOFT TIMER A:2 B:4 C:8
SOFT TIMER A:3 B:6 C:12
SOFT TIMER A:4 B:8 C:16
SOFT TIMER A:5 B:10 C:20
SOFT TIMER A:6 B:12 C:24
SOFT TIMER A:7 B:14 C:28
SOFT TIMER A:8 B:16 C:32
SOFT TIMER A:9 B:18 C:36
SOFT TIMER A:10 B:20 C:40
```

The relationship is:

```text
A : B : C = 1 : 2 : 4
```

### Result

```text
TASK 1: PWM Generation       PASS
TASK 2: Frequency Measurement PASS
TASK 3: Software Timer       PASS

MODULE 5 COMPLETE
```
