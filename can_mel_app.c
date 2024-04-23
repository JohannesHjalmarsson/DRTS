#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

#include "tonegen.h"
#include "can_melgen.h"

#define THIS_NODE_ID                 1
#define OTHER_NODE_ID                1


//const int BROTHER_JOHN[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
//const int PERIOD[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

typedef struct {
    Object super;
    int count;
    char buf[20];
    int mute;
} Mel_App;

Mel_App app = { initObject(), 0, "", 0 };

void reader(Mel_App*, int);
void receiver(Mel_App*, int);

//void input_handler(Mel_App*, int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

// MelGen 0
ToneGen tone0 = initToneGen;
MelGen melgen0 = initMelGen(&can0, &sci0, &tone0, THIS_NODE_ID, 0);

// MelGen 1
//ToneGen tone1 = initToneGen;
//MelGen melgen1 = initMelGen(&can0, &sci0, &tone0, OTHER_NODE_ID, 0);

void receiver(Mel_App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    //SCI_WRITE(&sci0, "Can msg received: ");
    //SCI_WRITE(&sci0, msg.buff);
    
    //if (msg.nodeId != 1) {
    //    MELODY_CAN_RECEIVE(&melgen1, &msg);
    //}
    //else if (msg.nodeId != 0) {
    //    MELODY_CAN_RECEIVE(&melgen0, &msg);
    //}
    
    MELODY_CAN_RECEIVE(&melgen0, &msg);
}

void reader(Mel_App *self, int c) {

    // Start melody generator
    if (c == 'z') {
        MELODY_PLAY(&melgen0);
        SCI_WRITE(&sci0, "Starting melody.\n");
        self->count = 0;
        
    // Stop melody generator
    } else if (c == 'x') {
        MELODY_STOP(&melgen0);
        SCI_WRITE(&sci0, "Stopping melody.\n");
        self->count = 0;
        
    // Reset melody generator
    } else if (c == 'c') {
        MELODY_RESET(&melgen0);
        SCI_WRITE(&sci0, "Resetting melody.\n");
        self->count = 0;
        
    // Increment volume
    } else if (c == 'v') {
        MELODY_VOL_INCR(&melgen0);
        SCI_WRITE(&sci0, "Increasing volume.\n");
        self->count = 0;
        
    // Decrement volume
    } else if (c == 'b') {
        MELODY_VOL_DECR(&melgen0);
        SCI_WRITE(&sci0, "Decreasing volume.\n");
        self->count = 0;
        
    // Reset key and tempo
    } else if (c == 'r') {
        MELODY_KEY_SET(&melgen0, __MELGEN_START_KEY);
        MELODY_TEMPO_SET(&melgen0, __MELGEN_START_TEMPO);
        SCI_WRITE(&sci0, "Reset key and tempo.\n");
        self->count = 0;
        
    // Toggle tempo reporting
    } else if (c == 'y') {
        SCI_WRITE(&sci0, "Toggle tempo reports.\n");
        MELODY_PRINT_TEMPO_TOGGLE(&melgen0);
        self->count = 0;
        
    // Request conductorship
    } else if (c == 'e') {
        SCI_WRITE(&sci0, "Requesting conductorship.\n");
        MELODY_REQ_COND(&melgen0);
        self->count = 0;
        
    // Debug report
    } else if (c == 'w') {
        MELODY_REPORT(&melgen0);
        self->count = 0;
        
    // Toggle mute reporting
    } else if (c == 'u') {
        SCI_WRITE(&sci0, "Toggle mute reports.\n");
        MELODY_PRINT_MUTED_TOGGLE(&melgen0);
        self->count = 0;
        
    // Mute on/off
    } else if (c == 'm') {
        if (self->mute == 0) {
            MELODY_MUTE_ON(&melgen0);
            SCI_WRITE(&sci0, "Mute on.\n");
            self->mute = 1;
        } else {
            MELODY_MUTE_OFF(&melgen0);
            SCI_WRITE(&sci0, "Mute off.\n");
            self->mute = 0;
        }
        self->count = 0;
    
    // Toggle fail F1
    } else if (c == 'o') {
        SCI_WRITE(&sci0, "Toggle fail F1.\n");
        MELODY_F1_TOGGLE(&melgen0);
        self->count = 0;
        
    // Toggle fail F2
    } else if (c == 'p') {
        SCI_WRITE(&sci0, "Fail F2.\n");
        MELODY_F2(&melgen0);
        self->count = 0;
        
    // Toggle debug
    } else if (c == 'd') {
        SCI_WRITE(&sci0, "Toggle debug.\n");
        MELODY_DEBUG_TOGGLE(&melgen0);
        self->count = 0;
    
    // Numerical ascii char
    } else if (c >= 48 && c <= 57) { // If c is a numerical ascii character
        self->buf[self->count] = c;
        self->count = self->count + 1;
        SCI_WRITECHAR(&sci0, c);
        
    // Minus sign
    } else if (c == '-' && self->count == 0) { // If c is a minus sign and the first character
        self->buf[0] = '-';
        self->count = self->count + 1;
        SCI_WRITECHAR(&sci0, c);
    
    // End of string (set key)
    } else if (c == 'k') {
        self->buf[self->count] = '\0';
        int key = atoi(self->buf);
        if (key >= -5 && key <= 5) {
            MELODY_KEY_SET(&melgen0, key-__MELGEN_OFFSET_KEY);
            SCI_WRITE(&sci0, "\nSet key.\n");
        } else {
            SCI_WRITE(&sci0, "Key out of bounds. (-5 - 5)\n");
        }
        self->count = 0;
        
    // End of string (set tempo)
    } else if (c == 't') { 
        self->buf[self->count] = '\0';
        int tempo = atoi(self->buf);
        if (tempo >= 60 && tempo <= 240) {
            MELODY_TEMPO_SET(&melgen0, tempo);
            SCI_WRITE(&sci0, "\nSet tempo.\n");
        } else {
            SCI_WRITE(&sci0, "Tempo out of bounds. (60 - 240)\n");
        }
        self->count = 0;

    // Invalid character
    } else { 
        SCI_WRITE(&sci0, "Invalid character!\n");
        self->count = 0;
    }
    
    // Error catching: Array out of bounds!
    if (self->count >= 20) {
        SCI_WRITE(&sci0, "Error: Input too long!\n");
        self->count = 0;
    }
}

void startApp(Mel_App *self, int arg) {
    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    
    MELODY_INIT(&melgen0);
    //MELODY_INIT(&melgen1);
    
    SCI_WRITE(&sci0, "Starting music player!\n");
    SCI_WRITE(&sci0, "Z: Play music\n");
    SCI_WRITE(&sci0, "X: Stop music\n");
    SCI_WRITE(&sci0, "C: Reset music\n");
    SCI_WRITE(&sci0, "V: Volume up\n");
    SCI_WRITE(&sci0, "B: Volume down\n");
    SCI_WRITE(&sci0, "M: Toggle mute\n");
    SCI_WRITE(&sci0, "0-9: Enter integer \n");
    SCI_WRITE(&sci0, "T: Set tempo (integer)\n");
    SCI_WRITE(&sci0, "K: Set key (integer)\n");
    SCI_WRITE(&sci0, "R: Reset key and tempo\n");
    SCI_WRITE(&sci0, "O: Fail F1\n");
    SCI_WRITE(&sci0, "P: Fail F2\n");
    SCI_WRITE(&sci0, "Y: Toggle tempo report\n");
    SCI_WRITE(&sci0, "U: Toggle mute report\n");
    SCI_WRITE(&sci0, "W: Get membership report\n");
    SCI_WRITE(&sci0, "D: Toggle debug mode\n");
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}