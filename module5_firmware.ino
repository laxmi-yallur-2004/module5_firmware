#include <Arduino.h>
#include <LiquidCrystal.h>
#include <avr/interrupt.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* Timer1: frequency measurement */
volatile uint32_t timer1OverflowCount = 0;
volatile uint32_t lastTimestamp = 0;
volatile uint32_t periodTicks = 0;
volatile uint32_t lastEdgeMillis = 0;
volatile bool firstEdge = true;
volatile bool frequencyValid = false;

/* PWM */
uint8_t pwmDuty = 50;

/* Scheduler */
uint32_t taskACount = 0;
uint32_t taskBCount = 0;
uint32_t taskCCount = 0;

uint32_t nextTaskA;
uint32_t nextTaskB;
uint32_t nextTaskC;
uint32_t nextDutyChange;
uint32_t nextLcdPage;

uint8_t dutyIndex = 0;
uint8_t lcdPage = 0;

const uint8_t dutyValues[] = {100, 0, 25, 50, 75};
const uint8_t DUTY_COUNT = sizeof(dutyValues) / sizeof(dutyValues[0]);

const uint32_t TASK_A_TIME = 1000UL;
const uint32_t TASK_B_TIME = 500UL;
const uint32_t TASK_C_TIME = 250UL;
const uint32_t DUTY_TIME = 3000UL;
const uint32_t LCD_TIME = 1000UL;

const uint32_t TIMER1_TICK_HZ = 2000000UL;
const uint32_t SIGNAL_TIMEOUT = 1000UL;

/* ============================================================
   PWM
   ============================================================ */

void setPwmDuty(uint8_t duty)
{
    if (duty > 100)
        duty = 100;

    pwmDuty = duty;

    if (duty == 0)
    {
        TCCR2A &= ~(1 << COM2B1);
        PORTD &= ~(1 << PORTD3);
        Serial.println("PWM DUTY: 0% -> D3 LOW");
        return;
    }

    if (duty == 100)
    {
        TCCR2A &= ~(1 << COM2B1);
        PORTD |= (1 << PORTD3);
        Serial.println("PWM DUTY: 100% -> D3 HIGH");
        return;
    }

    OCR2B = ((uint16_t)duty * 255UL + 50UL) / 100UL;
    TCCR2A |= (1 << COM2B1);

    Serial.print("PWM DUTY: ");
    Serial.print(duty);
    Serial.print("% -> OCR2B=");
    Serial.println(OCR2B);
}

void setupPwm()
{
    DDRD |= (1 << DDD3);

    TCCR2A = 0;
    TCCR2B = 0;

    /* Fast PWM, prescaler 64 */
    TCCR2A |= (1 << WGM21) | (1 << WGM20);
    TCCR2B |= (1 << CS22);

    OCR2B = 128;
}

/* ============================================================
   TIMER1 FREQUENCY MEASUREMENT
   ============================================================ */

void setupFrequencyMeasurement()
{
    /* D2 input + internal pull-up */
    DDRD &= ~(1 << DDD2);
    PORTD |= (1 << PORTD2);

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    timer1OverflowCount = 0;

    /* Timer1: 16 MHz / 8 = 2 MHz */
    TCCR1B |= (1 << CS11);

    /* W1C: clear Timer1 overflow flag */
    TIFR1 = (1 << TOV1);

    TIMSK1 |= (1 << TOIE1);

    /* INT0 rising edge */
    EICRA |= (1 << ISC01) | (1 << ISC00);

    /* W1C: clear INT0 flag */
    EIFR = (1 << INTF0);

    EIMSK |= (1 << INT0);
}

ISR(TIMER1_OVF_vect)
{
    timer1OverflowCount++;
}

ISR(INT0_vect)
{
    uint16_t count = TCNT1;
    uint32_t overflow = timer1OverflowCount;

    /* Handle Timer1 overflow/capture race */
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

uint32_t getPeriodTicks()
{
    uint32_t value;
    uint8_t sreg = SREG;

    cli();
    value = periodTicks;
    SREG = sreg;

    return value;
}

uint32_t getLastEdgeMillis()
{
    uint32_t value;
    uint8_t sreg = SREG;

    cli();
    value = lastEdgeMillis;
    SREG = sreg;

    return value;
}

bool getFrequencyValid()
{
    bool value;
    uint8_t sreg = SREG;

    cli();
    value = frequencyValid;
    SREG = sreg;

    return value;
}

uint32_t calculateFrequency(uint32_t period)
{
    if (period == 0)
        return 0;

    return (TIMER1_TICK_HZ + period / 2UL) / period;
}

/* ============================================================
   SOFTWARE SCHEDULER
   ============================================================ */

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

void taskB()
{
    taskBCount++;
}

void taskC()
{
    taskCCount++;
}

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


/* ============================================================
   FREQUENCY TIMEOUT
   ============================================================ */

void updateFrequency(uint32_t now)
{
    if (!getFrequencyValid())
        return;

    if ((uint32_t)(now - getLastEdgeMillis()) >= SIGNAL_TIMEOUT)
    {
        uint8_t sreg = SREG;

        cli();

        frequencyValid = false;
        firstEdge = true;
        periodTicks = 0;

        SREG = sreg;

        Serial.println("FREQUENCY: WAIT - NO SIGNAL");
    }
}

/* ============================================================
   PWM / LCD UPDATE
   ============================================================ */

void updatePwm(uint32_t now)
{
    if ((int32_t)(now - nextDutyChange) < 0)
        return;

    dutyIndex++;

    if (dutyIndex >= DUTY_COUNT)
        dutyIndex = 0;

    setPwmDuty(dutyValues[dutyIndex]);

    nextDutyChange += DUTY_TIME;

    if ((uint32_t)(now - nextDutyChange) > DUTY_TIME * 2UL)
        nextDutyChange = now + DUTY_TIME;
}

void updateLcd(uint32_t now)
{
    if ((int32_t)(now - nextLcdPage) < 0)
        return;

    nextLcdPage += LCD_TIME;

    bool valid = getFrequencyValid();
    uint32_t ticks = valid ? getPeriodTicks() : 0;
    uint32_t frequency = calculateFrequency(ticks);

    lcd.clear();

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

/* ============================================================
   SETUP / LOOP
   ============================================================ */

void setup()
{
    Serial.begin(115200);
    lcd.begin(16, 2);

    /* Atomic timer initialization */
    uint8_t sreg = SREG;
    cli();

    setupPwm();
    setupFrequencyMeasurement();

    SREG = sreg;

    setPwmDuty(50);

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
}

void loop()
{
    uint32_t now = millis();

    runScheduler(now);
    updateFrequency(now);
    updatePwm(now);
    updateLcd(now);
}
