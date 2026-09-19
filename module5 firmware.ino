

#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


/* =========================================================
   PIN DEFINITIONS
   ========================================================= */

/*
   Timer2 PWM output.

   Arduino Uno:
   D3 = OC2B
*/

const uint8_t PWM_PIN = 3;


/*
   Frequency capture input.

   Arduino Uno:
   D2 = INT0
*/

const uint8_t CAPTURE_PIN = 2;


/* =========================================================
   TASK 1 : PWM
   ========================================================= */

uint8_t dutyCycle = 50;

unsigned long lastPwmUpdate = 0;

const unsigned long PWM_UPDATE_INTERVAL = 1000UL;


/* =========================================================
   TASK 2 : FREQUENCY MEASUREMENT
   ========================================================= */

/*
   Timer1 runs at:

   16 MHz / 8 = 2 MHz

   Therefore:

   1 Timer1 tick = 0.5 microseconds
*/

volatile uint16_t lastTimer1Value = 0;

volatile uint16_t capturedPeriodTicks = 0;

volatile bool newCapture = false;


/*
   Extended Timer1 overflow counter.

   Timer1 is 16-bit, so this allows the measurement
   to continue correctly even when Timer1 overflows.
*/

volatile uint32_t timer1OverflowCount = 0;


/*
   Number of Timer1 overflows between two rising edges.
*/

volatile uint32_t previousOverflowCount = 0;


/*
   Flag indicating that at least one valid edge has
   already been captured.
*/

volatile bool firstCaptureDone = false;


/*
   Measured frequency.

   Updated by main loop.
*/

unsigned long measuredFrequency = 0;


/*
   LCD update timer for Task 2.
*/

unsigned long lastFrequencyDisplay = 0;

const unsigned long FREQUENCY_DISPLAY_INTERVAL = 500UL;


/* =========================================================
   TASK 3 : SOFTWARE SCHEDULER
   ========================================================= */

const unsigned long TASK_A_INTERVAL = 1000UL;
const unsigned long TASK_B_INTERVAL = 2000UL;
const unsigned long TASK_C_INTERVAL = 5000UL;


unsigned long lastTaskA = 0;
unsigned long lastTaskB = 0;
unsigned long lastTaskC = 0;


unsigned int countA = 0;
unsigned int countB = 0;
unsigned int countC = 0;


/* =========================================================
   TASK 3 : DISPLAY CONTROL
   ========================================================= */

bool schedulerDisplayEnabled = true;


/* =========================================================
   TASK 2 : TIMER1 OVERFLOW ISR
   ========================================================= */

/*
   Timer1 is used as a free-running 16-bit timer.

   Every time Timer1 overflows:

       0xFFFF -> 0

   this interrupt increments the software overflow counter.
*/

ISR(TIMER1_OVF_vect)
{
    timer1OverflowCount++;
}


/* =========================================================
   TASK 2 : EXTERNAL INTERRUPT
   ========================================================= */

/*
   D2 receives the PWM signal from D3.

   Every rising edge is captured.
*/

void captureEdge()
{
    uint16_t currentTimerValue = TCNT1;

    uint32_t currentOverflowCount = timer1OverflowCount;


    /*
       First edge:
       There is no previous edge yet.

       Just save it.
    */

    if (!firstCaptureDone)
    {
        lastTimer1Value = currentTimerValue;

        previousOverflowCount = currentOverflowCount;

        firstCaptureDone = true;

        return;
    }


    /*
       Calculate number of Timer1 ticks between
       the previous rising edge and this rising edge.
    */

    uint32_t overflowDifference =
        currentOverflowCount - previousOverflowCount;


    uint32_t currentTicks;


    /*
       Handle Timer1 overflow.

       If no overflow occurred:

           current - previous

       If one or more overflows occurred:

           overflowDifference * 65536
           + current
           - previous
    */

    if (currentTimerValue >= lastTimer1Value)
    {
        currentTicks =
            (overflowDifference * 65536UL)
            + (currentTimerValue - lastTimer1Value);
    }
    else
    {
        /*
           Timer1 wrapped during the measurement.

           The overflow counter already records this.
        */

        currentTicks =
            (overflowDifference * 65536UL)
            + currentTimerValue
            - lastTimer1Value;
    }


    /*
       Save result.

       Disable nested interrupts while updating
       shared variables.
    */

    capturedPeriodTicks =
        (uint16_t)currentTicks;


    /*
       For the normal ~977 Hz signal, the period is
       approximately 2048 Timer1 ticks.

       This fits safely inside uint16_t.

       If a larger period is required, the measurement
       can be extended further.
    */

    newCapture = true;


    /*
       Save current edge as previous edge.
    */

    lastTimer1Value = currentTimerValue;

    previousOverflowCount = currentOverflowCount;
}


/* =========================================================
   TASK 1 : TIMER2 PWM INITIALIZATION
   ========================================================= */

void initializePWM()
{
    /*
       D3 = OC2B
    */

    pinMode(PWM_PIN, OUTPUT);


    /*
       Clear Timer2 registers before configuration.
    */

    TCCR2A = 0;

    TCCR2B = 0;


    /*
       Fast PWM mode.

       WGM20 = 1
       WGM21 = 1

       Mode = Fast PWM, TOP = 0xFF
    */

    TCCR2A |=
        (1 << WGM20) |
        (1 << WGM21);


    /*
       Non-inverting PWM on OC2B.

       D3 = OC2B.
    */

    TCCR2A |=
        (1 << COM2B1);


    /*
       Timer2 prescaler = 64.

       CS22 = 1

       Frequency:

       16 MHz / (64 * 256)
       = 976.5625 Hz

       Approximately 977 Hz.
    */

    TCCR2B |=
        (1 << CS22);


    /*
       Initial duty cycle = 50%.
    */

    OCR2B = 128;
}


/* =========================================================
   TASK 2 : TIMER1 INITIALIZATION
   ========================================================= */

void initializeFrequencyMeasurement()
{
    /*
       D2 = INT0 input.
    */

    pinMode(CAPTURE_PIN, INPUT);


    /*
       Timer1 normal mode.
    */

    TCCR1A = 0;

    TCCR1B = 0;


    /*
       Timer1 prescaler = 8.

       Timer clock:

       16 MHz / 8 = 2 MHz
    */

    TCCR1B |=
        (1 << CS11);


    /*
       Start Timer1 from zero.
    */

    TCNT1 = 0;


    /*
       Enable Timer1 overflow interrupt.
    */

    TIMSK1 |=
        (1 << TOIE1);


    /*
       External interrupt on D2.

       Trigger on rising edge.
    */

    attachInterrupt(
        digitalPinToInterrupt(CAPTURE_PIN),
        captureEdge,
        RISING
    );
}


/* =========================================================
   TASK 1 : UPDATE PWM DUTY
   ========================================================= */

void updatePwmDuty()
{
    /*
       Change duty cycle:

       50
       75
       100
       0
       25
       50
       ...

       Every 1 second.
    */

    dutyCycle += 25;


    if (dutyCycle > 100)
    {
        dutyCycle = 0;
    }


    /*
       Convert:

       0%   -> 0
       25%  -> approximately 64
       50%  -> approximately 128
       75%  -> approximately 191
       100% -> 255
    */

    OCR2B =
        (uint8_t)
        ((uint16_t)dutyCycle * 255UL / 100UL);
}


/* =========================================================
   TASK 2 : UPDATE FREQUENCY
   ========================================================= */

void updateFrequency()
{
    uint16_t ticks = 0;


    /*
       Check whether ISR captured a new period.
    */

    if (newCapture)
    {
        noInterrupts();

        ticks = capturedPeriodTicks;

        newCapture = false;

        interrupts();
    }


    /*
       Calculate frequency.

       Timer1 clock = 2 MHz.

       Frequency:

           2,000,000 / period_ticks
    */

    if (ticks > 0)
    {
        measuredFrequency =
            2000000UL / ticks;
    }
}


/* =========================================================
   TASK 2 : DISPLAY FREQUENCY
   ========================================================= */

void displayFrequency()
{
    /*
       Do not update LCD continuously.

       Update every 500 ms.
    */

    unsigned long currentTime = millis();


    if ((currentTime - lastFrequencyDisplay)
        >= FREQUENCY_DISPLAY_INTERVAL)
    {
        lastFrequencyDisplay = currentTime;


        /*
           Read the latest captured frequency.
        */

        updateFrequency();


        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("PWM:");
        lcd.print(dutyCycle);
        lcd.print("%");


        lcd.setCursor(0, 1);
        lcd.print("FREQ:");

        if (measuredFrequency > 0)
        {
            lcd.print(measuredFrequency);
            lcd.print("Hz");
        }
        else
        {
            lcd.print("WAIT");
        }
    }
}


/* =========================================================
   TASK 3 : SOFTWARE SCHEDULER
   ========================================================= */

void runSoftwareScheduler()
{
    unsigned long currentTime = millis();

    bool taskExecuted = false;


    /* -----------------------------------------------------
       TASK A
       Every 1 second
       ----------------------------------------------------- */

    if ((currentTime - lastTaskA)
        >= TASK_A_INTERVAL)
    {
        lastTaskA += TASK_A_INTERVAL;

        countA++;

        taskExecuted = true;
    }


    /* -----------------------------------------------------
       TASK B
       Every 2 seconds
       ----------------------------------------------------- */

    if ((currentTime - lastTaskB)
        >= TASK_B_INTERVAL)
    {
        lastTaskB += TASK_B_INTERVAL;

        countB++;

        taskExecuted = true;
    }


    /* -----------------------------------------------------
       TASK C
       Every 5 seconds
       ----------------------------------------------------- */

    if ((currentTime - lastTaskC)
        >= TASK_C_INTERVAL)
    {
        lastTaskC += TASK_C_INTERVAL;

        countC++;

        taskExecuted = true;
    }


    /*
       LCD is already being used for PWM/frequency.

       Therefore scheduler counters are printed to
       Serial instead of fighting for the same LCD display.
    */

    if (taskExecuted)
    {
        Serial.print("SOFT TIMER  A:");
        Serial.print(countA);

        Serial.print(" B:");
        Serial.print(countB);

        Serial.print(" C:");
        Serial.println(countC);
    }
}


/* =========================================================
   SETUP
   ========================================================= */

void setup()
{
    /*
       Serial output.
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
       TASK 1:
       Initialize Timer2 PWM.
    */

    initializePWM();


    /*
       TASK 2:
       Initialize Timer1 measurement.
    */

    initializeFrequencyMeasurement();


    /*
       Initialize software scheduler timers.
    */

    unsigned long startTime = millis();

    lastPwmUpdate = startTime;

    lastFrequencyDisplay = startTime;

    lastTaskA = startTime;
    lastTaskB = startTime;
    lastTaskC = startTime;


    /*
       Initial information.
    */

    Serial.println();
    Serial.println("==============================");
    Serial.println("      MODULE 5 FIRMWARE");
    Serial.println("==============================");

    Serial.println("TASK 1: TIMER2 PWM");
    Serial.println("PWM PIN: D3");
    Serial.println("PWM FREQUENCY: ~977 Hz");
    Serial.println("INITIAL DUTY: 50%");

    Serial.println();

    Serial.println("TASK 2: FREQUENCY MEASUREMENT");
    Serial.println("SIGNAL: D3");
    Serial.println("CAPTURE: D2");
    Serial.println("TIMER1: 2 MHz");

    Serial.println();

    Serial.println("TASK 3: SOFTWARE SCHEDULER");
    Serial.println("TASK A: 1 second");
    Serial.println("TASK B: 2 seconds");
    Serial.println("TASK C: 5 seconds");

    Serial.println();

    Serial.println("NON-BLOCKING MODE");
    Serial.println("==============================");


    /*
       Initial LCD display.
    */

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("PWM:50%");

    lcd.setCursor(0, 1);
    lcd.print("FREQ:WAIT");
}


/* =========================================================
   MAIN LOOP
   ========================================================= */

void loop()
{
    unsigned long currentTime = millis();


    /* =====================================================
       TASK 1 : PWM DUTY UPDATE
       ===================================================== */

    if ((currentTime - lastPwmUpdate)
        >= PWM_UPDATE_INTERVAL)
    {
        lastPwmUpdate += PWM_UPDATE_INTERVAL;

        updatePwmDuty();


        /*
           Print PWM information.
        */

        Serial.print("PWM DUTY: ");
        Serial.print(dutyCycle);
        Serial.println("%");
    }


    /* =====================================================
       TASK 2 : FREQUENCY DISPLAY
       ===================================================== */

    displayFrequency();


    /* =====================================================
       TASK 3 : SOFTWARE SCHEDULER
       ===================================================== */

    runSoftwareScheduler();
}

