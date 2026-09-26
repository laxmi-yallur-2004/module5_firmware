#include <Arduino.h>
#include <LiquidCrystal.h>
#include <avr/interrupt.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* Timer1 frequency measurement variables */
volatile uint32_t timer1OverflowCount = 0;
volatile uint32_t lastTimestamp = 0;
volatile uint32_t periodTicks = 0;
volatile uint32_t lastEdgeMillis = 0;
volatile bool firstEdge = true;
volatile bool frequencyValid = false;

/* Current PWM duty */
uint8_t pwmDuty = 0;

/* Software timer counters */
uint32_t taskACount = 0;
uint32_t taskBCount = 0;
uint32_t taskCCount = 0;

/* Next execution times */
uint32_t nextTaskA;
uint32_t nextTaskB;
uint32_t nextTaskC;
uint32_t nextDutyChange;
uint32_t nextLcdPage;

uint8_t dutyIndex = 0;
uint8_t lcdPage = 0;

/* PWM duty sequence */
const uint8_t dutyValues[] = {0, 25, 50, 75, 100};
const uint8_t DUTY_COUNT = sizeof(dutyValues) / sizeof(dutyValues[0]);

/* Software timer periods in milliseconds */
const uint32_t TASK_A_TIME = 1000UL;
const uint32_t TASK_B_TIME = 500UL;
const uint32_t TASK_C_TIME = 250UL;

const uint32_t DUTY_TIME = 3000UL;
const uint32_t LCD_TIME = 1000UL;

/* Timer1: 16 MHz / 8 = 2 MHz */
const uint32_t TIMER1_TICK_HZ = 2000000UL;

/* No signal after 1 second */
const uint32_t SIGNAL_TIMEOUT = 1000UL;


/* Clear old frequency measurement */
void invalidateFrequency()
{
    uint8_t sreg = SREG;
    cli();

    frequencyValid = false;
    firstEdge = true;
    periodTicks = 0;

    SREG = sreg;
}


/* Set PWM duty and handle exact 0% / 100% safely */
void setPwmDuty(uint8_t duty)
{
    if (duty > 100)
        duty = 100;

    pwmDuty = duty;

    /* Exact 0%: disconnect PWM and force D3 LOW */
    if (duty == 0)
    {
        TCCR2A &= ~(1 << COM2B1);
        PORTD &= ~(1 << PORTD3);

        invalidateFrequency();

        Serial.println("PWM DUTY: 0% -> D3 LOW");
        Serial.println("FREQUENCY: WAIT - NO SIGNAL");
        return;
    }

    /* Exact 100%: disconnect PWM and force D3 HIGH */
    if (duty == 100)
    {
        TCCR2A &= ~(1 << COM2B1);
        PORTD |= (1 << PORTD3);

        invalidateFrequency();

        Serial.println("PWM DUTY: 100% -> D3 HIGH");
        Serial.println("FREQUENCY: WAIT - NO SIGNAL");
        return;
    }

    /* Rounded OCR2B value for 25%, 50%, 75% */
    OCR2B = ((uint16_t)duty * 255UL + 50UL) / 100UL;

    TCCR2A |= (1 << COM2B1);

    Serial.print("PWM DUTY: ");
    Serial.print(duty);
    Serial.print("% -> OCR2B=");
    Serial.println(OCR2B);
}


/* Timer2 Fast PWM on D3, frequency about 977 Hz */
void setupPwm()
{
    DDRD |= (1 << DDD3);

    TCCR2A = 0;
    TCCR2B = 0;

    /* Fast PWM, TOP = 255 */
    TCCR2A |= (1 << WGM21) | (1 << WGM20);

    /* Prescaler = 64 */
    TCCR2B |= (1 << CS22);

    OCR2B = 0;
}


/* Timer1 measures the period of the signal on D2 */
void setupFrequencyMeasurement()
{
    /* D2 input with internal pull-up */
    DDRD &= ~(1 << DDD2);
    PORTD |= (1 << PORTD2);

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    timer1OverflowCount = 0;

    /* Timer1 clock = 16 MHz / 8 = 2 MHz */
    TCCR1B |= (1 << CS11);

    /* Clear pending overflow flag */
    TIFR1 = (1 << TOV1);

    TIMSK1 |= (1 << TOIE1);

    /* INT0 on rising edge */
    EICRA |= (1 << ISC01) | (1 << ISC00);

    /* Clear pending external interrupt flag */
    EIFR = (1 << INTF0);

    EIMSK |= (1 << INT0);
}


/* Count Timer1 overflows */
ISR(TIMER1_OVF_vect)
{
    timer1OverflowCount++;
}


/* Capture every rising edge on D2 */
ISR(INT0_vect)
{
    uint16_t count = TCNT1;
    uint32_t overflow = timer1OverflowCount;

    /* Correct overflow/capture race */
    if ((TIFR1 & (1 << TOV1)) && count < 32768U)
        overflow++;

    uint32_t timestamp = (overflow << 16) | count;

    if (firstEdge)
    {
        lastTimestamp = timestamp;
        firstEdge = false;
        frequencyValid = false;
    }
    else
    {
        uint32_t period = timestamp - lastTimestamp;

        if (period > 0)
        {
            periodTicks = period;
            frequencyValid = true;
        }

        lastTimestamp = timestamp;
    }

    lastEdgeMillis = millis();
}


/* Safely read 32-bit period value */
uint32_t getPeriodTicks()
{
    uint32_t value;
    uint8_t sreg = SREG;

    cli();
    value = periodTicks;
    SREG = sreg;

    return value;
}


/* Safely read last edge time */
uint32_t getLastEdgeMillis()
{
    uint32_t value;
    uint8_t sreg = SREG;

    cli();
    value = lastEdgeMillis;
    SREG = sreg;

    return value;
}


/* Safely read frequency status */
bool getFrequencyValid()
{
    bool value;
    uint8_t sreg = SREG;

    cli();
    value = frequencyValid;
    SREG = sreg;

    return value;
}


/* Frequency = Timer1 clock / measured period */
uint32_t calculateFrequency(uint32_t period)
{
    if (period == 0)
        return 0;

    /* Rounded frequency calculation */
    return (TIMER1_TICK_HZ + period / 2UL) / period;
}


/* Software Timer A: 1 second */
void taskA()
{
    taskACount++;

    Serial.print("SOFT TIMER A:");
    Serial.print(taskACount);
    Serial.print(" B:");
    Serial.print(taskBCount);
    Serial.print(" C:");
    Serial.println(taskCCount);
}


/* Software Timer B: 500 ms */
void taskB()
{
    taskBCount++;
}


/* Software Timer C: 250 ms */
void taskC()
{
    taskCCount++;
}


/* Run missed software timer events without blocking */
void runScheduler(uint32_t now)
{
    while ((int32_t)(now - nextTaskB) >= 0)
    {
        taskB();
        nextTaskB += TASK_B_TIME;
    }

    while ((int32_t)(now - nextTaskC) >= 0)
    {
        taskC();
        nextTaskC += TASK_C_TIME;
    }

    while ((int32_t)(now - nextTaskA) >= 0)
    {
        taskA();
        nextTaskA += TASK_A_TIME;
    }
}


/* Detect missing input signal */
void updateFrequency(uint32_t now)
{
    if (!getFrequencyValid())
        return;

    if ((uint32_t)(now - getLastEdgeMillis()) >= SIGNAL_TIMEOUT)
    {
        invalidateFrequency();
        Serial.println("FREQUENCY: WAIT - NO SIGNAL");
    }
}


/* Change PWM duty every 3 seconds */
void updatePwm(uint32_t now)
{
    if ((int32_t)(now - nextDutyChange) < 0)
        return;

    dutyIndex++;

    if (dutyIndex >= DUTY_COUNT)
        dutyIndex = 0;

    setPwmDuty(dutyValues[dutyIndex]);

    nextDutyChange += DUTY_TIME;

    /* Recover scheduler if execution was delayed */
    if ((uint32_t)(now - nextDutyChange) > DUTY_TIME * 2UL)
        nextDutyChange = now + DUTY_TIME;
}


/* Rotate LCD through 3 information pages */
void updateLcd(uint32_t now)
{
    if ((int32_t)(now - nextLcdPage) < 0)
        return;

    nextLcdPage += LCD_TIME;

    bool valid = getFrequencyValid();
    uint32_t ticks = valid ? getPeriodTicks() : 0;
    uint32_t frequency = calculateFrequency(ticks);

    lcd.clear();

    /* Page 1: PWM and frequency */
    if (lcdPage == 0)
    {
        lcd.setCursor(0, 0);
        lcd.print("PWM:");
        lcd.print(pwmDuty);
        lcd.print("%");

        lcd.setCursor(0, 1);

        if (valid)
        {
            lcd.print("FREQ:");
            lcd.print(frequency);
            lcd.print(" Hz");
        }
        else
        {
            lcd.print("FREQ:WAIT");
        }
    }

    /* Page 2: Software timers */
    else if (lcdPage == 1)
    {
        lcd.setCursor(0, 0);
        lcd.print("A:");
        lcd.print(taskACount);
        lcd.print(" B:");
        lcd.print(taskBCount);

        lcd.setCursor(0, 1);
        lcd.print("C:");
        lcd.print(taskCCount);
    }

    /* Page 3: Timer1 measurement */
    else
    {
        lcd.setCursor(0, 0);
        lcd.print("T1:2MHz /8");

        lcd.setCursor(0, 1);

        if (valid)
        {
            lcd.print("TICKS:");
            lcd.print(ticks);
        }
        else
        {
            lcd.print("NO SIGNAL");
        }
    }

    lcdPage++;

    if (lcdPage >= 3)
        lcdPage = 0;
}


void setup()
{
    Serial.begin(115200);
    lcd.begin(16, 2);

    /* Protect timer/interrupt configuration */
    uint8_t sreg = SREG;
    cli();

    setupPwm();
    setupFrequencyMeasurement();

    SREG = sreg;

    uint32_t now = millis();

    nextTaskA = now + TASK_A_TIME;
    nextTaskB = now + TASK_B_TIME;
    nextTaskC = now + TASK_C_TIME;
    nextDutyChange = now + DUTY_TIME;
    nextLcdPage = now + LCD_TIME;

    Serial.println("==============================");
    Serial.println("MODULE 5");
    Serial.println("==============================");
    Serial.println("PWM: TIMER2 D3");
    Serial.println("FREQ: TIMER1 D2");
    Serial.println("D3 -> D2 JUMPER");
    Serial.println("PWM FREQUENCY: ~977 Hz");
    Serial.println("TIMER1 RESERVED: FREQUENCY");
    Serial.println("TIMER2 RESERVED: PWM");
    Serial.println("==============================");

    /* Start at 0% duty */
    setPwmDuty(dutyValues[dutyIndex]);
}


void loop()
{
    uint32_t now = millis();

    runScheduler(now);
    updateFrequency(now);
    updatePwm(now);
    updateLcd(now);
}
