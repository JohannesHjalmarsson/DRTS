#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

#include "tonegen.h"
#include "bg_load.h"

const int BROTHER_JOHN[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

const int PERIOD[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

typedef struct {
    Object super;
    int count;
    char buf[20];
    int int_buf[3];
    int int_count;
} Tone_App;

Tone_App app = { initObject(), 0, "", {}, 0 };

void reader(Tone_App*, int);
void receiver(Tone_App*, int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

ToneGen tone = initToneGen;
BgLoad bgload = initBgLoad;

void receiver(Tone_App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(Tone_App *self, int c) {
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    
    // Control tone generator
    if (c == 'a') {
        TONE_PLAY(&tone);
        SCI_WRITE(&sci0, "Started playing.\n");
    } else if (c == 'd') {
        TONE_STOP(&tone);
        SCI_WRITE(&sci0, "Stopped playing.\n");
    } else if (c == 'w') {
        TONE_VOL_INCR(&tone);
        SCI_WRITE(&sci0, "Increasing volume.\n");
    } else if (c == 's') {
        TONE_VOL_DECR(&tone);
        SCI_WRITE(&sci0, "Decreasing volume.\n");
    } else if (c == 'q') {
        TONE_DEADLINE_ON(&tone);
        SCI_WRITE(&sci0, "Tone generator deadline on.\n");
    } else if (c == 'e') {
        TONE_DEADLINE_OFF(&tone);
        SCI_WRITE(&sci0, "Tone generator deadline off.\n");
    }
    
    // Control background load
    else if (c == 'j') {
        BGLOAD_START(&bgload);
        SCI_WRITE(&sci0, "Started background load.\n");
    } else if (c == 'l') {
        BGLOAD_STOP(&bgload);
        SCI_WRITE(&sci0, "Stopped background load.\n");
    } else if (c == 'i') {
        BGLOAD_RNG_INCR(&bgload);
        SCI_WRITE(&sci0, "Increasing load.\n");
    } else if (c == 'k') {
        BGLOAD_RNG_DECR(&bgload);
        SCI_WRITE(&sci0, "Decreasing load.\n");
    } else if (c == 'u') {
        BGLOAD_DEADLINE_ON(&bgload);
        SCI_WRITE(&sci0, "Background load deadline on.\n");
    } else if (c == 'o') {
        BGLOAD_DEADLINE_OFF(&bgload);
        SCI_WRITE(&sci0, "Background load deadline off.\n");
    }
}

void startApp(Tone_App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
    
    TONE_PERIOD_SET(&tone,500); // 500 us period - 1 kHz tone
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
