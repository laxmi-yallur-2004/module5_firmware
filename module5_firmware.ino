/*
   ============================================================
   MODULE 5 FIRMWARE
   Arduino Uno / ATmega328P / 16 MHz

   TASK 1:
   Timer2 hardware PWM on D3
   PWM frequency = approximately 976.56 Hz

   TASK 2:
   Timer1 frequency measurement using D2 / INT0
   REQUIRED TEST CONNECTION:
       D3 -> D2

   TASK 3:
   Non-blocking software scheduler

   LCD:
       RS = D8
       EN = D9
       D4 = D4
       D5 = D5
       D6 = D6
       D7 = D7

   SERIAL:
       9600 baud

   IMPORTANT:
   - Single .ino file
   - One setup()
   - One loop()
   - One LiquidCrystal object
   - No delay()
   - No dynamic memory
   - Timer0 is used by millis()
   - Timer1 is reserved for frequency measurement
   - Timer2 is reserved for PWM
   ============================================================
*/

#include <LiquidCrystal.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/* ============================================================
   LCD
   ============================================================ */

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* ============================================================
   PIN DEFINITIONS
   ============================================================ */

const uint8_t PWM_PIN = 3;
const uint8_t CAPTURE_PIN = 2;

/* ============================================================
   TIMER CONSTANTS
   ============================================================ */

const uint32_t TIMER1_TICK_HZ = 2000000UL;

/*
   Timer2:
   16 MHz / 64 / 256
   = 976.5625 Hz
*/
const uint32_t PWM_FREQUENCY_HZ = 976UL;

/*
   If no D2 rising edge is received for this long,
   the old frequency is considered invalid.
*/
const uint32_t FREQUENCY_TIMEOUT_MS = 1500UL;

/* ============================================================
   PWM VARIABLES
   ============================================================ */

uint8_t dutyCycle = 50;

uint32_t lastPwmUpdate = 0;

const uint32_t PWM_UPDATE_INTERVAL = 1000UL;

bool pwmConstantOutput = false;

/* ============================================================
   TIMER1 FREQUENCY VARIABLES
   ============================================================ */

/*
   Timer1 is 16-bit.

   This software counter extends Timer1 to 32 bits.
*/
volatile uint32_t timer1OverflowCount = 0;

/*
   Previous complete 32-bit Timer1 timestamp.
*/
volatile uint32_t previousCaptureTimestamp = 0;

/*
   Latest measured period.

   IMPORTANT:
   uint32_t is used instead of uint16_t.
*/
volatile uint32_t capturedPeriodTicks = 0;

/*
   Indicates that a new capture measurement
   is available to the main loop.
*/
volatile bool newCapture = false;

/*
   Indicates whether the first edge has arrived.
*/
volatile bool firstCaptureDone = false;

/*
   Time of the most recent D2 rising edge.
*/
volatile uint32_t lastCaptureMillis = 0;

/*
   Number of captured rising edges.
*/
volatile uint32_t captureCount = 0;

/*
   Latest calculated frequency.
*/
uint32_t measuredFrequency = 0;

bool frequencyValid = false;

/* ============================================================
   SOFTWARE SCHEDULER
   ============================================================ */

const uint32_t TASK_A_INTERVAL = 1000UL;
const uint32_t TASK_B_INTERVAL = 2000UL;
const uint32_t TASK_C_INTERVAL = 5000UL;

uint32_t lastTaskA = 0;
uint32_t lastTaskB = 0;
uint32_t lastTaskC = 0;

uint32_t countA = 0;
uint32_t countB = 0;
uint32_t countC = 0;

/* ============================================================
   LCD PAGE CONTROL
   ============================================================ */

uint8_t lcdPage = 0;

uint32_t lastLcdPageChange = 0;

const uint32_t LCD_PAGE_INTERVAL = 2000UL;

/* ============================================================
   FREQUENCY DISPLAY TIMER
   ============================================================ */

uint32_t lastFrequencyDisplay = 0;

const uint32_t FREQUENCY_DISPLAY_INTERVAL = 250UL;

/* ============================================================
   PWM CONSTANT OUTPUT
   ============================================================ */

void setPwmConstantOutput(bool highLevel)
{
    /*
       Disconnect Timer2 from OC2B/D3.
    */
    TCCR2A &= ~((1 << COM2B1) | (1 << COM2B0));

    /*
       Force D3 to a known constant level.
    */
    if (highLevel)
    {
        PORTD |= (1 << PD3);
    }
    else
    {
        PORTD &= ~(1 << PD3);
    }

    pwmConstantOutput = true;
}

/* ============================================================
   ENABLE TIMER2 PWM
   ============================================================ */

void enablePwmOutput()
{
    /*
       Non-inverting PWM on OC2B/D3.
    */
    TCCR2A &= ~(1 << COM2B0);
    TCCR2A |= (1 << COM2B1);

    pwmConstantOutput = false;
}

/* ============================================================
   APPLY PWM DUTY
   ============================================================ */

void applyPwmDuty()
{
    /*
       --------------------------------------------------------
       EXACT 0%
       --------------------------------------------------------
    */

    if (dutyCycle == 0)
    {
        /*
           Do NOT depend on OCR2B = 0.

           Force D3 LOW.
        */
        setPwmConstantOutput(false);

        /*
           There are no rising edges.
           Therefore old frequency is invalid.
        */
        noInterrupts();

        frequencyValid = false;
        measuredFrequency = 0;

        firstCaptureDone = false;
        newCapture = false;

        interrupts();

        return;
    }

    /*
       --------------------------------------------------------
       EXACT 100%
       --------------------------------------------------------
    */

    if (dutyCycle == 100)
    {
        /*
           Force D3 HIGH.
        */
        setPwmConstantOutput(true);

        /*
           There are no rising edges.
           Therefore old frequency is invalid.
        */
        noInterrupts();

        frequencyValid = false;
        measuredFrequency = 0;

        firstCaptureDone = false;
        newCapture = false;

        interrupts();

        return;
    }

    /*
       --------------------------------------------------------
       1% to 99%
       --------------------------------------------------------

       Rounded conversion:

       OCR2B =
           (duty * 255 + 50) / 100

       Examples:

       25% = 64
       50% = 128
       75% = 191
    */

    uint8_t compareValue =
        (uint8_t)(((uint16_t)dutyCycle * 255UL + 50UL) / 100UL);

    OCR2B = compareValue;

    enablePwmOutput();
}

/* ============================================================
   INITIALIZE TIMER2 PWM
   ============================================================ */

void initializePWM()
{
    /*
       D3 = OC2B.
    */
    DDRD |= (1 << DDD3);

    /*
       Start LOW.
    */
    PORTD &= ~(1 << PD3);

    /*
       Clear Timer2 configuration.
    */
    TCCR2A = 0;
    TCCR2B = 0;

    /*
       Fast PWM, TOP = 255.

       WGM22 = 0
       WGM21 = 1
       WGM20 = 1
    */
    TCCR2A |= (1 << WGM21) | (1 << WGM20);

    /*
       Timer2 prescaler = 64.

       16 MHz / 64 / 256
       = 976.5625 Hz
    */
    TCCR2B |= (1 << CS22);

    /*
       Initial duty = 50%.
    */
    OCR2B = 128;

    /*
       Enable non-inverting PWM.
    */
    TCCR2A |= (1 << COM2B1);

    pwmConstantOutput = false;
}

/* ============================================================
   READ EXTENDED TIMER1 TIMESTAMP
   ============================================================ */

uint32_t readExtendedTimer1Timestamp()
{
    /*
       Read current Timer1 count.
    */
    uint16_t timerValue = TCNT1;

    /*
       Read software overflow count.
    */
    uint32_t overflowValue = timer1OverflowCount;

    /*
       Check whether Timer1 overflow happened but
       the overflow ISR has not executed yet.
    */
    if ((TIFR1 & (1 << TOV1)) != 0)
    {
        /*
           If TCNT1 is small, the timer has already
           wrapped around.
        */
        if (timerValue < 0x8000U)
        {
            overflowValue++;
        }
    }

    /*
       Combine overflow count + 16-bit timer count.

       Result is a 32-bit timestamp.
    */
    return (overflowValue << 16) | timerValue;
}

/* ============================================================
   TIMER1 OVERFLOW ISR
   ============================================================ */

ISR(TIMER1_OVF_vect)
{
    timer1OverflowCount++;
}

/* ============================================================
   D2 / INT0 ISR
   ============================================================ */

/*
   THIS WAS THE MISSING PART.

   D2 on Arduino Uno = INT0.

   Every rising edge on D2 enters this ISR.
*/
ISR(INT0_vect)
{
    captureEdge();
}

/* ============================================================
   D2 CAPTURE FUNCTION
   ============================================================ */

void captureEdge()
{
    /*
       Read a consistent extended Timer1 timestamp.
    */
    uint32_t currentTimestamp =
        readExtendedTimer1Timestamp();

    /*
       Record time of latest edge.
    */
    lastCaptureMillis = millis();

    captureCount++;

    /*
       First edge cannot calculate a period yet.
    */
    if (!firstCaptureDone)
    {
        previousCaptureTimestamp =
            currentTimestamp;

        firstCaptureDone = true;

        return;
    }

    /*
       Calculate period.

       32-bit subtraction also handles timestamp wrap.
    */
    uint32_t currentPeriod =
        currentTimestamp - previousCaptureTimestamp;

    /*
       Save current timestamp.
    */
    previousCaptureTimestamp =
        currentTimestamp;

    /*
       Reject invalid zero period.
    */
    if (currentPeriod == 0)
    {
        return;
    }

    /*
       Store complete 32-bit period.
    */
    capturedPeriodTicks =
        currentPeriod;

    /*
       Tell main loop that a new measurement exists.
    */
    newCapture = true;
}

/* ============================================================
   INITIALIZE TIMER1 FREQUENCY MEASUREMENT
   ============================================================ */

void initializeFrequencyMeasurement()
{
    /*
       D2 = INT0.

       Pull-up prevents the pin from floating.

       REQUIRED TEST:
           D3 -> D2
    */
    pinMode(CAPTURE_PIN, INPUT_PULLUP);

    /*
       Timer1 normal mode.
    */
    TCCR1A = 0;
    TCCR1B = 0;

    /*
       Start Timer1 from zero.
    */
    TCNT1 = 0;

    /*
       Clear any old Timer1 overflow flag.

       Writing 1 clears the flag.
    */
    TIFR1 |= (1 << TOV1);

    /*
       Reset software state.
    */
    timer1OverflowCount = 0;
    previousCaptureTimestamp = 0;
    capturedPeriodTicks = 0;

    firstCaptureDone = false;
    newCapture = false;

    captureCount = 0;
    lastCaptureMillis = 0;

    /*
       Timer1 prescaler = 8.

       16 MHz / 8 = 2 MHz.
    */
    TCCR1B |= (1 << CS11);

    /*
       Enable Timer1 overflow interrupt.
    */
    TIMSK1 |= (1 << TOIE1);

    /*
       INT0 rising edge.

       ISC01 = 1
       ISC00 = 1
    */
    EICRA |= (1 << ISC01) | (1 << ISC00);

    /*
       Clear pending INT0 flag.
    */
    EIFR |= (1 << INTF0);

    /*
       Enable INT0.
    */
    EIMSK |= (1 << INT0);
}

/* ============================================================
   PROCESS NEW FREQUENCY
   ============================================================ */

void updateFrequencyMeasurement()
{
    uint32_t ticks = 0;

    bool haveNewCapture = false;

    /*
       Copy ISR data atomically.
    */
    noInterrupts();

    if (newCapture)
    {
        ticks = capturedPeriodTicks;

        newCapture = false;

        haveNewCapture = true;
    }

    interrupts();

    /*
       Calculate frequency only when a new
       valid period exists.
    */
    if (haveNewCapture && ticks > 0)
    {
        /*
           Timer1 = 2,000,000 ticks/second.

           Rounded integer division:
           
           (2000000 + ticks/2) / ticks
        */
        measuredFrequency =
            (TIMER1_TICK_HZ + (ticks / 2UL))
            / ticks;

        frequencyValid = true;

        /*
           Print actual calculated frequency.
        */
        Serial.print("FREQUENCY: ");
        Serial.print(measuredFrequency);
        Serial.println(" Hz");
    }
}

/* ============================================================
   NO-SIGNAL TIMEOUT
   ============================================================ */

void checkFrequencyTimeout()
{
    uint32_t latestEdgeTime;

    bool captureStarted;

    uint8_t currentDuty;

    /*
       Copy shared ISR data safely.
    */
    noInterrupts();

    latestEdgeTime =
        lastCaptureMillis;

    captureStarted =
        firstCaptureDone;

    currentDuty =
        dutyCycle;

    interrupts();

    /*
       0% and 100% intentionally have no rising edges.
    */
    if (currentDuty == 0 ||
        currentDuty == 100)
    {
        frequencyValid = false;
        measuredFrequency = 0;

        return;
    }

    /*
       No first edge received.
    */
    if (!captureStarted)
    {
        frequencyValid = false;
        measuredFrequency = 0;

        return;
    }

    /*
       Check timeout.
    */
    uint32_t currentTime =
        millis();

    if ((uint32_t)(currentTime - latestEdgeTime)
        >= FREQUENCY_TIMEOUT_MS)
    {
        /*
           Old frequency is no longer valid.
        */
        frequencyValid = false;
        measuredFrequency = 0;

        /*
           Wait for a completely new first edge.
        */
        noInterrupts();

        firstCaptureDone = false;
        newCapture = false;

        interrupts();

        Serial.println("FREQUENCY: WAIT - NO SIGNAL");
    }
}

/* ============================================================
   FREQUENCY UPDATE
   ============================================================ */

void updateFrequency()
{
    uint32_t currentTime =
        millis();

    if ((uint32_t)(currentTime -
                   lastFrequencyDisplay)
        < FREQUENCY_DISPLAY_INTERVAL)
    {
        return;
    }

    lastFrequencyDisplay =
        currentTime;

    updateFrequencyMeasurement();

    checkFrequencyTimeout();
}

/* ============================================================
   UPDATE PWM DUTY
   ============================================================ */

void updatePwmDuty()
{
    /*
       Sequence:

       50%
       75%
       100%
       0%
       25%
       50%
       ...
    */

    dutyCycle += 25;

    if (dutyCycle > 100)
    {
        dutyCycle = 0;
    }

    /*
       Apply new PWM.
    */
    applyPwmDuty();

    /*
       Print PWM status.
    */
    Serial.print("PWM DUTY: ");
    Serial.print(dutyCycle);
    Serial.print("%");

    if (dutyCycle == 0)
    {
        Serial.println(" -> D3 LOW");
    }
    else if (dutyCycle == 100)
    {
        Serial.println(" -> D3 HIGH");
    }
    else
    {
        Serial.print(" -> OCR2B=");
        Serial.println(OCR2B);
    }
}

/* ============================================================
   SOFTWARE SCHEDULER
   ============================================================ */

void runSoftwareScheduler()
{
    uint32_t currentTime =
        millis();

    bool taskExecuted = false;

    /*
       --------------------------------------------------------
       TASK A - 1 second
       --------------------------------------------------------
    */

    uint32_t elapsedA =
        currentTime - lastTaskA;

    if (elapsedA >= TASK_A_INTERVAL)
    {
        /*
           Calculate ALL missed intervals.
        */
        uint32_t missedA =
            elapsedA / TASK_A_INTERVAL;

        lastTaskA +=
            missedA * TASK_A_INTERVAL;

        countA += missedA;

        taskExecuted = true;
    }

    /*
       --------------------------------------------------------
       TASK B - 2 seconds
       --------------------------------------------------------
    */

    uint32_t elapsedB =
        currentTime - lastTaskB;

    if (elapsedB >= TASK_B_INTERVAL)
    {
        uint32_t missedB =
            elapsedB / TASK_B_INTERVAL;

        lastTaskB +=
            missedB * TASK_B_INTERVAL;

        countB += missedB;

        taskExecuted = true;
    }

    /*
       --------------------------------------------------------
       TASK C - 5 seconds
       --------------------------------------------------------
    */

    uint32_t elapsedC =
        currentTime - lastTaskC;

    if (elapsedC >= TASK_C_INTERVAL)
    {
        uint32_t missedC =
            elapsedC / TASK_C_INTERVAL;

        lastTaskC +=
            missedC * TASK_C_INTERVAL;

        countC += missedC;

        taskExecuted = true;
    }

    /*
       Print scheduler result.
    */
    if (taskExecuted)
    {
        Serial.print("SOFT TIMER A:");
        Serial.print(countA);

        Serial.print(" B:");
        Serial.print(countB);

        Serial.print(" C:");
        Serial.println(countC);
    }
}

/* ============================================================
   LCD PAGE 0
   ============================================================ */

void displayPagePWM()
{
    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("PWM:");
    lcd.print(dutyCycle);
    lcd.print("%");

    lcd.setCursor(0, 1);

    lcd.print("FREQ:");

    if (frequencyValid)
    {
        lcd.print(measuredFrequency);
        lcd.print("Hz");
    }
    else
    {
        lcd.print("WAIT");
    }
}

/* ============================================================
   LCD PAGE 1
   ============================================================ */

void displayPageFrequency()
{
    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("CAPTURE:D2");

    lcd.setCursor(0, 1);

    if (frequencyValid)
    {
        lcd.print("FREQ:");
        lcd.print(measuredFrequency);
        lcd.print("Hz");
    }
    else
    {
        lcd.print("FREQ:WAIT");
    }
}

/* ============================================================
   LCD PAGE 2
   ============================================================ */

void displayPageScheduler()
{
    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("A:");
    lcd.print(countA);

    lcd.print(" B:");
    lcd.print(countB);

    lcd.setCursor(0, 1);

    lcd.print("C:");
    lcd.print(countC);

    lcd.print(" SCHEDULER");
}

/* ============================================================
   LCD PAGE ROTATION
   ============================================================ */

void updateLCD()
{
    uint32_t currentTime =
        millis();

    if ((uint32_t)(currentTime -
                   lastLcdPageChange)
        < LCD_PAGE_INTERVAL)
    {
        return;
    }

    lastLcdPageChange =
        currentTime;

    /*
       Rotate:

       PAGE 0
       PAGE 1
       PAGE 2
       PAGE 0
       ...
    */

    lcdPage++;

    if (lcdPage > 2)
    {
        lcdPage = 0;
    }

    if (lcdPage == 0)
    {
        displayPagePWM();
    }
    else if (lcdPage == 1)
    {
        displayPageFrequency();
    }
    else
    {
        displayPageScheduler();
    }
}

/* ============================================================
   SETUP
   ============================================================ */

void setup()
{
    /*
       Serial.
    */
    Serial.begin(9600);

    /*
       LCD.
    */
    lcd.begin(16, 2);

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("MODULE 5");

    lcd.setCursor(0, 1);
    lcd.print("INITIALIZING");

    /*
       Timer2 PWM.
    */
    initializePWM();

    /*
       Timer1 + INT0 frequency capture.
    */
    initializeFrequencyMeasurement();

    /*
       Enable global interrupts.
    */
    sei();

    /*
       Start time.
    */
    uint32_t startTime =
        millis();

    /*
       Initialize scheduler timers.
    */
    lastPwmUpdate =
        startTime;

    lastFrequencyDisplay =
        startTime;

    lastTaskA =
        startTime;

    lastTaskB =
        startTime;

    lastTaskC =
        startTime;

    lastLcdPageChange =
        startTime;

    /*
       Initial PWM = 50%.
    */
    dutyCycle = 50;

    applyPwmDuty();

    /*
       Serial startup information.
    */
    Serial.println();
    Serial.println("==============================");
    Serial.println("     MODULE 5 FIRMWARE");
    Serial.println("==============================");

    Serial.println("TASK 1: TIMER2 PWM");
    Serial.println("PWM PIN: D3");
    Serial.println("PWM FREQUENCY: ~976.56 Hz");
    Serial.println("INITIAL DUTY: 50%");

    Serial.println();

    Serial.println("TASK 2: TIMER1 FREQUENCY");
    Serial.println("CAPTURE PIN: D2");
    Serial.println("TEST CONNECTION: D3 -> D2");
    Serial.println("TIMER1 CLOCK: 2 MHz");

    Serial.println();

    Serial.println("TASK 3: SOFTWARE SCHEDULER");
    Serial.println("TASK A: 1 SECOND");
    Serial.println("TASK B: 2 SECONDS");
    Serial.println("TASK C: 5 SECONDS");

    Serial.println();

    Serial.println("0%  -> D3 LOW");
    Serial.println("100% -> D3 HIGH");
    Serial.println("NO SIGNAL TIMEOUT: 1500 ms");
    Serial.println("INT0 CAPTURE: ENABLED");
    Serial.println("==============================");

    /*
       Initial LCD.
    */
    displayPagePWM();
}

/* ============================================================
   MAIN LOOP
   ============================================================ */

void loop()
{
    uint32_t currentTime =
        millis();

    /* ========================================================
       TASK 1 - PWM
       ======================================================== */

    if ((uint32_t)(currentTime -
                   lastPwmUpdate)
        >= PWM_UPDATE_INTERVAL)
    {
        /*
           Calculate how many PWM intervals elapsed.
        */
        uint32_t missed =
            (currentTime -
             lastPwmUpdate)
            / PWM_UPDATE_INTERVAL;

        /*
           Move scheduler reference forward.
        */
        lastPwmUpdate +=
            missed * PWM_UPDATE_INTERVAL;

        /*
           Advance PWM once for every elapsed interval.
        */
        while (missed > 0)
        {
            updatePwmDuty();

            missed--;
        }
    }

    /* ========================================================
       TASK 2 - FREQUENCY
       ======================================================== */

    updateFrequency();

    /* ========================================================
       TASK 3 - SOFTWARE SCHEDULER
       ======================================================== */

    runSoftwareScheduler();

    /* ========================================================
       LCD
       ======================================================== */

    updateLCD();
}
