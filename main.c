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

void delay(int time)
{
    int i, j;
    for (i = 0; i < time; i++)
    {
        for (j = 0; j < 100; j++);
    }
}

void instCtrl(unsigned char data) {
    PORTC = data;
    RB5 = 0;  // RS = 0 for instruction
    RB6 = 0;  // RW = 0 for write
    RB7 = 1;  // EN = 1 to start pulse
    delay(5);
    RB7 = 0;  // EN = 0 to end pulse
}

void dataCtrl(unsigned char data) {
    PORTC = data;
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

void goBackward(void) {
    // keep bits 6-7, set bits 0-5 to 0011 0110 (0x35)
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
    }
    else {
        RD6 = 1; // led-red ON
        RD7 = 0; // led-green OFF
    }
}

// Optional mapping if needed (only using 1-9). Adjust based on your keypad layout.
char processKeypadInput() {
    delay(20);

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

void portConfigs (void) {
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
    int robotState = 0; // 0: idle, 1: opmode
    int previousState = robotState; // track the previous state
    unsigned char rowsToPlant;
    const char* idleModeText = "IDLE MODE...";
    const char* question = "Rows to plant:";
    unsigned char outputText[15];

    portConfigs(); 

    initLCD();

    instCtrl(0x02); // set cursor 1st line col 2
    printLCD(question);

    instCtrl(0xD9); // set cursor 4th line
    printLCD(idleModeText);

    setLedState(robotState); // idle mode initial
    stopMotors();

    while (1) {
        if (RB0 == 1) { // button pressed
            delay(50);

            if (RB0 == 1) { // confirm press
                robotState ^= 1; // toggle robot state
                setLedState(robotState);

                while (RB0 == 1); // wait until button is released
            }
        }

        if (robotState != previousState) {
            if (robotState == 0) {
                stopMotors();
            }
            else {
                goForward();
            }

            previousState = robotState; // update previousState
        }

        /* logic for keypad */
        /* can only access if idle mode */
		if ((RA4 == 1) && (robotState == 0))
		{
		    keypadData = PORTA & 0x0F; // mask data

			rowsToPlant = processKeypadInput();

            instCtrl(0x8F); // set cursor to line 1 col 15
            sprintf(outputText, "%c", rowsToPlant);
            printLCD(outputText);

            while (RA4 == 1);
		}
    }
}
