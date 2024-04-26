#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

#include "regulator.h"

#define __MAX_MID 127
#define __BURST_DELAY MSEC(500)

typedef struct {
    Object super;
    unsigned char count;
    unsigned char bursting;
} Reg_App;

Reg_App app = { initObject(), 0, 0 };

void reader(Reg_App*, int);
void receiver(Reg_App*, int);
void __burst(Reg_App *self, int unused);
    
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);
Regulator reg0 = initRegulator(&sci0, 0);

void receiver(Reg_App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    REG_CAN_RECEIVE(&reg0, msg.msgId);
}

void reader(Reg_App *self, int c) {
    
    if (c == 'o') {
        CANMsg msg;
        msg.msgId = self->count;
        CAN_SEND(&can0, &msg);
        self->count = self->count + 1;
        if (self->count == __MAX_MID)
            self->count = 0;
    }
    else if (c == 'b') {
        SCI_WRITE(&sci0, "Start burst\n");
        if (!self->bursting) {
            self->bursting = 1;
            ASYNC(self, __burst, 0);
        }
    }
    else if (c == 'x') {
        SCI_WRITE(&sci0, "Stop burst\n");
        self->bursting = 0;
    }
    else if (c == 'd') {
        SCI_WRITE(&sci0, "Toggle debug\n");
        REG_DEBUG_TOGGLE(&reg0);
    }
}

void __burst(Reg_App *self, int unused) {
    if (!self->bursting)
        return;
    
    CANMsg msg;
    msg.msgId = self->count;
    CAN_SEND(&can0, &msg);
    self->count = self->count + 1;
    if (self->count == __MAX_MID)
        self->count = 0;
        
    AFTER(__BURST_DELAY, self, __burst, 0);
}

void startApp(Reg_App *self, int arg) {
    CAN_INIT(&can0);
    SCI_INIT(&sci0);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
