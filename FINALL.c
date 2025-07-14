#include <xc.h>
#include <stdio.h> // For sprintf

#pragma config FOSC = XT
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// for keypad
int keypadFlag = 0;				 // keypad flag
unsigned char keypadData = 0x00; // keypad data in hex

// More accurate delay function
void delay_ms(unsigned int ms) {
    unsigned int i;
    unsigned char j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 165; j++); // Inner loop adjusted for ~1ms with 4MHz XTAL.  Adjust if needed.
    }
}

void instCtrl(unsigned char data) {
    PORTC = data;
    RB5 = 0;  // RS = 0 for instruction
    RB6 = 0;  // RW = 0 for write
    RB7 = 1;  // EN = 1 to start pulse
    delay_ms(1); // Short delay
    RB7 = 0;  // EN = 0 to end pulse
}

void dataCtrl(unsigned char data) {
    PORTC = data;
    RB5 = 1;  // RS = 1 for data
    RB6 = 0;  // RW = 0 for write
    RB7 = 1;  // EN = 1 to start pulse
    delay_ms(1); // Short delay
    RB7 = 0;  // EN = 0 to end pulse
}

void initLCD() {
    delay_ms(50);       // Wait for power-up

    instCtrl(0x3C); // function set: 8-bit; dual-line
    instCtrl(0x38); // function set: 8-bit; dual-bit - redundant, but good to have.
    instCtrl(0x01); // display clear
    instCtrl(0x06); // entry mode: increment; shift off
    instCtrl(0x0C); // display on; cursor off; blink off
}

void printLCD(const char *str) {
    while (*str) {
        dataCtrl(*str++);
    }
}

void goBackward(void) {
    // keep bits 6-7, set bits 0-5 to 0011 0110 (0x36)
    PORTD = (PORTD & 0xC0) | 0x36;
}

void goForward(void) {
    // keep bits 6-7, set bits 0-5 to 0011 1001 (0x3A)
    PORTD = (PORTD & 0xC0) | 0x39;
}

void stopMotors(void) {
    // keep bits 6-7, clear bits 0-5
    PORTD &= 0xC0;
}

void ledStateAndDcMotorsConfig(void) {
    TRISD = 0x00; // set ports D as output
    PORTD = 0x00; // initialize to off
}

void setLedState(int opmode) {
    if (opmode) {
        RD6 = 0; // led-red OFF
        RD7 = 1; // led-green ON
    } else {
        RD6 = 1; // led-red ON
        RD7 = 0; // led-green OFF
    }
}

// Optional mapping if needed (only using 1-9). Adjust based on your keypad layout.
char processKeypadInput() {
    delay_ms(20); // Debounce

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
        default: return ' ';
    }
}

void portConfigs(void) {
    TRISA = 0xFF; // input keypad (74C922)
    TRISB = 0x0F; // input button bit 0 / output lcd bits 5:7
    TRISC = 0x00; // LCD output
    TRISD = 0x00; // set ports D as output to motors

    PORTA = 0x00; // keypad input
    PORTB = 0x00; // btn & lcd(rs, rw, e)
    PORTC = 0x00; // lcd data output
    PORTD = 0x00; // motors & led (robot state)

    ADCON1 = 0x06; // port a pins are set to digital i/o
}

void main(void) {
    enum { IDLE, RUNNING, PLANTING } robotState = IDLE;
    enum { IDLE_PREV, RUNNING_PREV, PLANTING_PREV } previousState = IDLE_PREV;
    unsigned char rowsToPlant = 0; // Changed to unsigned char
    const char* idleModeText =     "IDLE MODE.......";
    const char* runningModeText =  "RUNNING MODE....";
    const char* plantingModeText = "PLANTING MODE...";
    const char* question = "Rows to plant:";
    const char* chooseNumberText = "Choose 1-9..."; // Added text
    unsigned char outputText[15];
    unsigned int runningTimeCounter = 0;  // in ms
    unsigned int plantingTimeCounter = 0; // in ms
    unsigned char fiveSecFlag = 0;
    unsigned char keypadPressed = 0; // Flag to indicate keypad press
    unsigned char cycleCount = 0;     // Count for RUNNING-PLANTING cycles
    unsigned char targetCycles = 0;    // Number of cycles from keypad
    unsigned char firstRun = 1;
    unsigned int timeoutCounter = 0; // Counter for the 2-second timeout

    portConfigs();

    initLCD();
    delay_ms(100); // Extra delay after LCD init.

    instCtrl(0x02); // set cursor 1st line col 2
    printLCD(question);

    instCtrl(0xC0); // set cursor to the beginning of the second line
    printLCD(idleModeText);

    setLedState(robotState == RUNNING); // idle mode initial
    stopMotors();

    while (1) {
        // logic for keypad
        // can only access if idle mode
        if ((RA4 == 1) && (robotState == IDLE)) {
            keypadData = PORTA & 0x0F; // mask data

            rowsToPlant = processKeypadInput();
            if (rowsToPlant >= '1' && rowsToPlant <= '9') {
                targetCycles = rowsToPlant - '0'; // Convert char to int
                instCtrl(0x8F); // set cursor to line 1 col 15
                sprintf(outputText, "%c", rowsToPlant);
                printLCD(outputText);
                keypadPressed = 1; //set the flag
                timeoutCounter = 0; // Reset timeout counter
            } else {
                instCtrl(0xC0);
                printLCD("Invalid Input");
                delay_ms(2000);
                instCtrl(0xC0);
                printLCD(idleModeText);
            }

            while (RA4 == 1);
        }

        if (RB0 == 1) { // button pressed
            delay_ms(50);

            if (RB0 == 1) { // confirm press
                if (keypadPressed) { // Check if keypad was pressed
                    if (robotState == IDLE) {
                        robotState = RUNNING;
                        runningTimeCounter = 0;
                        fiveSecFlag = 0;
                        cycleCount = 0; // Reset cycle count
                        firstRun = 1;
                        timeoutCounter = 0; //reset
                         if (targetCycles > 0) {
                            sprintf(outputText, "%d", targetCycles);
                            instCtrl(0x8F);
                            printLCD(outputText);
                        }
                    } else if (robotState == RUNNING || robotState == PLANTING) {
                        robotState = IDLE;
                        stopMotors();
                        runningTimeCounter = 0;
                        plantingTimeCounter = 0;
                        fiveSecFlag = 0;
                        cycleCount = 0;
                        firstRun = 1;
                        timeoutCounter = 0;
                        if (targetCycles > 0) {
                            targetCycles--;
                             sprintf(outputText, "%d", targetCycles);
                            instCtrl(0x8F);
                            printLCD(outputText);
                        }
                    }
                    setLedState(robotState == RUNNING);
                    instCtrl(0xC0);  //update LCD
                    if (robotState == RUNNING) {
                        printLCD(runningModeText);
                    } else {
                        printLCD(idleModeText);
                    }
                    if (targetCycles == 0) {
                        keypadPressed = 0;
                        instCtrl(0x8F);
                        printLCD(" ");
                    }


                } else {
                    instCtrl(0xC0);
                    printLCD(chooseNumberText); //tell user to choose a number
                    timeoutCounter = 0; // Start the timeout counter
                }

                while (RB0 == 1); // wait until button is released
            }
        }

        if (robotState != previousState) {
            instCtrl(0xC0); // set cursor to the beginning of the second line
            if (robotState == IDLE) {
                printLCD(idleModeText);
                stopMotors();
                runningTimeCounter = 0;
                plantingTimeCounter = 0;
                fiveSecFlag = 0;
                keypadPressed = 0; //reset
                cycleCount = 0;
                firstRun = 1;
                timeoutCounter = 0;
                instCtrl(0x8F);  //clear the LCD
                printLCD(" ");
            } else if (robotState == RUNNING) {
                printLCD(runningModeText);
                goForward();
                runningTimeCounter = 0;
                fiveSecFlag = 0;
                timeoutCounter = 0;
                 if (targetCycles > 0) {
                    sprintf(outputText, "%d", targetCycles);
                    instCtrl(0x8F);
                    printLCD(outputText);
                }
            } else if (robotState == PLANTING) {
                printLCD(plantingModeText);
                stopMotors();
                plantingTimeCounter = 0;
                plantingTimeCounter = 0;
                fiveSecFlag = 0;
                timeoutCounter = 0;
                 if (targetCycles > 0) {
                    sprintf(outputText, "%d", targetCycles);
                    instCtrl(0x8F);
                    printLCD(outputText);
                }
            }
            previousState = robotState; // update previousState
        }

        // State transition logic using counters
        if (robotState == RUNNING) {
            runningTimeCounter += 10; // Increment counter every 10ms
            if (runningTimeCounter >= 5000 && fiveSecFlag == 0) { // Check for 5 seconds (5000ms)
                robotState = PLANTING;
                runningTimeCounter = 0;
                fiveSecFlag = 1;
            }
        } else if (robotState == PLANTING) {
            plantingTimeCounter += 10;
            if (plantingTimeCounter >= 5000 && fiveSecFlag == 0) {
                robotState = RUNNING;
                plantingTimeCounter = 0;
                fiveSecFlag = 1;
                cycleCount++; // Increment the cycle counter
            }
        }

        // Check for the end of cycles
        if (cycleCount >= targetCycles && targetCycles != 0) {
            robotState = IDLE; // Stop after the desired cycles
            stopMotors();
            instCtrl(0xC0);
            printLCD(idleModeText);
            cycleCount = 0;
            targetCycles = 0;
            keypadPressed = 0;
            timeoutCounter = 0;
            instCtrl(0x8F);  //clear the LCD
            printLCD(" ");
        } else if (targetCycles == 0 && keypadPressed == 1) {
            robotState = IDLE; // Stop after the desired cycles
            stopMotors();
            instCtrl(0xC0);
            printLCD(idleModeText);
            cycleCount = 0;
            targetCycles = 0;
            keypadPressed = 0;
            timeoutCounter = 0;
            instCtrl(0x8F);  //clear the LCD
            printLCD(" ");
        }


        delay_ms(10); // important for the timing loop and debouncing
    }
}

