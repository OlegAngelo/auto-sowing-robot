#include <xc.h>
#include <stdio.h> // for sprintf

#pragma config FOSC = XT
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

unsigned char keypadData = 0x00; // keypad data in hex
unsigned char outputText[15];

int robotState = 0;  // 0: idle, 1: opmode
int previousState; // track the previous state
int rowsToPlant = 0;
int stopRequested = 0;  // global flag to exit operation mode early
int previousEncoderState = 0;
int encoderCount = 0;

const char* idleModeText = "==IDLE MODE==";
const char* operationModeText = "==OPERATION MODE==";
const char* questionText = "Rows to plant:";
const char* drillingText = "Drilling...";
const char* plantingText = "Planting...";
const char* travellingText = "Travelling...";
const char* blankLine = "                      ";

void delay(int time)
{
    int i, j;
    for (i = 0; i < time; i++)
    {
        for (j = 0; j < 100; j++);
    }
}

void instCtrl(unsigned char data) {
    PORTD = data;
    RB5 = 0;  // RS = 0 for instruction
    RB6 = 0;  // RW = 0 for write
    RB7 = 1;  // EN = 1 to start pulse
    delay(5);
    RB7 = 0;  // EN = 0 to end pulse
}

void dataCtrl(unsigned char data) {
    PORTD = data;
    RB5 = 1;  // RS = 1 for data
    RB6 = 0;  // RW = 0 for write
    RB7 = 1;  // EN = 1 to start pulse
    delay(5);
    RB7 = 0;  // EN = 0 to end pulse
}

void initLCD() {
    delay(50);       // Wait for power-up

    instCtrl(0x3C); // function set: 8-bit; dual-line
    instCtrl(0x38); // display off
    instCtrl(0x01); // display clear
    instCtrl(0x06); // entry mode: increment; shift off
    instCtrl(0x0C); // display on; cursor off; blink off
}

void printLCD(const char *str) {
    while (*str) {
        dataCtrl(*str++);
    }
}

void goForward(void) {
    // IN1–IN4 = 1001 => RC7=1, RC6=0, RC5=0, RC4=1
    // Mask bits 4–7: clear and set accordingly
    PORTC = (PORTC & 0x0F) | 0x90; // 1001 0000 = 0x90
}

void goBackward(void) {
    // IN1–IN4 = 0110 => RC7=0, RC6=1, RC5=1, RC4=0
    PORTC = (PORTC & 0x0F) | 0x60; // 0110 0000 = 0x60
}

void stopMotors(void) {
    // Clear IN1–IN4 bits (RC4–RC7), preserve lower nibble (RC0–RC3)
    PORTC &= 0x0F;
}

void setLedState(int opmode) {
    if (opmode) {
        RC0 = 0; // led-red OFF
        RC3 = 1; // led-green ON
    }
    else {
        RC0 = 1; // led-red ON
        RC3 = 0; // led-green OFF
    }
}

// Optional mapping if needed (only using 1-9). Adjust based on your keypad layout.
char processKeypadInput() {
    delay(5);

    switch (keypadData) {
        case 0x00: return '1';
        case 0x01: return '2';
        case 0x02: return '3';
        case 0x04: return '4';
        case 0x05: return '5';
        case 0x06: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0D: return '0';
        default: return ' ';
    }
}

void portConfigs (void) {
    TRISA = 0xFF; // input keypad (74C922)
    TRISB = 0x0F; // input button bit 0 / output lcd bits 5:7
    TRISC = 0x00; // output to motors
    TRISD = 0x00; // LCD output

    PORTA = 0x00; // keypad input
    PORTB = 0x00; // btn & lcd(rs, rw, e)
    PORTC = 0x00; // motors & led (robot state) 
    PORTD = 0x00; // lcd data output

    ADCON1 = 0x06; // port a pins are set to digital i/o
}

void drillMode (void) {
    stopMotors();
    instCtrl(0xD7);
    printLCD(blankLine);
    instCtrl(0xD8);
    printLCD(drillingText);
    delay(1000);
}

void plantMode (void) {
    instCtrl(0xD8);
    printLCD(blankLine);
    instCtrl(0xD8);
    printLCD(plantingText);
    delay(1000);
}

void travelMode (void) {
    instCtrl(0xD8);
    printLCD(blankLine);
    instCtrl(0xD8);
    printLCD(travellingText);
    goForward();
    delay(1000);
}

void operationDone (void) {
    stopMotors();
    instCtrl(0xD7);
    printLCD(blankLine);
    instCtrl(0xD7);
    printLCD("Operation done.");
}

void updateRowCount (int intRowCount) {
    instCtrl(0xCF);
    printLCD("   ");
    instCtrl(0xCF);
    sprintf(outputText, "%d", intRowCount);
    printLCD(outputText);
}

void resetDisplay (void) {
    instCtrl(0x81);
    printLCD(blankLine);
    instCtrl(0x84); 
    printLCD(idleModeText);
}

void btnPress(void) {
    if (RB0 == 1) {
        delay(5);
        if (RB0 == 1) {
            while (RB0 == 1); // Debounce: wait for release

            if (robotState == 0 && rowsToPlant >= 1 && rowsToPlant <= 999) {
                robotState = 1;
                setLedState(robotState);
            } else if (robotState == 1) {
                stopRequested = 1;  // <-- just set this flag
            }
        }
    }
}

void setInitialDisplay (void) {
    instCtrl(0x84); // set cursor 1st line col 9
    printLCD(idleModeText);

    instCtrl(0xC0); // set cursor 2nd line col 2
    printLCD(questionText);
}

void initPWM(void) {
    // set RC1 and RC2 as output (PWM pins)
    TRISC1 = 0; // CCP2 pin output
    TRISC2 = 0; // CCP1 pin output

    // timer2 configuration
    T2CON = 0; // Clear Timer2 control register
    T2CON = 0x01; // Prescaler = 4, Timer2 off initially 
    PR2 = 124; // Period register: controls PWM frequency

    // CCP1 and CCP2 in PWM mode
    CCP1CON = 0x0C; // PWM mode for CCP1
    CCP2CON = 0x0C; // PWM mode for CCP2

    // clear duty cycle bits 4 and 5 before setting duty cycle
    CCP1CON &= 0xCF; // clear bits 4 and 5 (DC1B1 and DC1B0)
    CCP2CON &= 0xCF; // clear bits 4 and 5 (DC2B1 and DC2B0)

    // start timer2
    TMR2ON = 1;

    // set initial duty cycle to ~25%
    CCPR1L = 64; // high 8 bits of duty cycle for CCP1
    CCPR2L = 64; // high 8 bits of duty cycle for CCP2
}

void setMotorSpeedLeft(unsigned int duty) {
    if (duty > 1023) duty = 1023;
    CCPR2L = duty >> 2;
    CCP2CON = (CCP2CON & 0xCF) | ((duty & 0x03) << 4);
}

void setMotorSpeedRight(unsigned int duty) {
    if (duty > 1023) duty = 1023;
    CCPR1L = duty >> 2;
    CCP1CON = (CCP1CON & 0xCF) | ((duty & 0x03) << 4);
}

void setMotorSpeed (unsigned int value) {
    setMotorSpeedLeft(value);
    setMotorSpeedRight(value);
}

void main(void) {
    previousState = robotState;

    portConfigs(); 

    initLCD();

    setInitialDisplay();
    
    initPWM();

    /* 
    * if motors sound rough, reduce duty slightly.
    * if they don’t spin, increase duty.
    * lower PR2 = faster PWM frequency (shorter cycle time).
    * higher PR2 = slower PWM frequency (longer cycle time).
    * check info on notes for more info
    * 
    * setMotorSpeed(256);   // ~25%
    * setMotorSpeed(768);   // ~75%
    * setMotorSpeed(1023);   // ~1000%
    */
    setMotorSpeed(512);   // ~50%

    setLedState(robotState); // idle mode initial
    stopMotors();

    while (1) {
        btnPress();

        // logic for changing robot state
        if (robotState != previousState) {
            if (robotState == 1 && rowsToPlant >= 1 && rowsToPlant <= 999) {
                int intRowCount = rowsToPlant;

                instCtrl(0x81); // Line 1, col 5
                printLCD(operationModeText);

                while (intRowCount > 0  && robotState == 1) {
                   
                    drillMode();
                  
                    plantMode();

                    while (encoderCount < 2) {
                        travelMode();
                        if(previousEncoderState != RB1 && RB1 == 1) {
                            encoderCount++;
                        }
                        previousEncoderState = RB1;
                    }
                    encoderCount = 0; //reset encoder count

                    intRowCount--;

                    updateRowCount(intRowCount);
                    
                    if (stopRequested) {
                        stopMotors();
                        robotState = 0;
                        setLedState(robotState);
                        instCtrl(0x81);
                        printLCD(idleModeText);
                        rowsToPlant = 0;
                        stopRequested = 0;  // reset flag
                        break;
                    }
                }

                operationDone();

                robotState = 0;
                setLedState(robotState);

                resetDisplay();

                rowsToPlant = 0; // reset input
            }

            previousState = robotState;
        }

        // logic for keypad input (multi-digit), can only access if idle mode
        if ((RA4 == 1) && (!robotState))
        {
            keypadData = PORTA & 0x0F; // mask data
            char key = processKeypadInput();

            if (key >= '0' && key <= '9') {
                if (rowsToPlant < 100) {  // only allow up to 3 digits
                    rowsToPlant = rowsToPlant * 10 + (key - '0');

                    instCtrl(0xCF);        // adjust to your LCD position
                    printLCD("  ");       // clear previous number
                    instCtrl(0xCF);
                    sprintf(outputText, "%d", rowsToPlant);
                    printLCD(outputText);
                }
            }
            // clear input for other keys
            else if (key == ' ') {
                rowsToPlant = 0;
                instCtrl(0xCF);
                printLCD("    ");
            }

            while (RA4 == 1); // debounce
        }
    }
}
