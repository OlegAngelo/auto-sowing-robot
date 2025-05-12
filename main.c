#include <xc.h>

#pragma config FOSC = XT
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

void delay(int time)
{
    int i, j;
    for (i = 0; i < time; i++)
    {
        for (j = 0; j < 100; j++);
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

void stateAndDcMotorsConfig(void) {
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

void main(void) {
    TRISB0 = 1; // input button
    RB0 = 0;    // set button to low

    stateAndDcMotorsConfig(); // state and motors configs

    int robotState = 0; // 0: idle, 1: opmode
    setLedState(robotState); // idle mode
    stopMotors();

    int previousState = robotState; // track the previous state

    while (1) {
        if (RB0 == 1) { // button pressed
            delay(50);

            if (RB0 == 1) { // confirm press
                robotState ^= 1; // toggle robot state
                setLedState(robotState);

                while (RB0 == 1); // wait until button is released
            }
        }

        // Only update motors when the state changes
        if (robotState != previousState) {
            if (robotState == 1) {
                goForward(); // start moving forward
            } else {
                stopMotors(); // stop motors
            }
            previousState = robotState; // update previousState
        }
    }
}
