#include "canTinyTimber.h"
#include "sciTinyTimber.h"

#define __REG_BUFSIZE 8
#define __REG_TIME MSEC(1000)

typedef struct {
    // System objects/variables
    Object super;
    Serial *sci;
    unsigned char debug;
    
    // Buffer and buffer control
    unsigned char buffer[__REG_BUFSIZE];
    unsigned char read_index;
    unsigned char write_index;
    unsigned char standby;
} Regulator;

#define initRegulator(sci, debug) { initObject(), sci, debug, {0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 1 }

void reg_can_receive(Regulator* self, unsigned char msg);
void __reg_followup(Regulator* self, int unused);
void reg_debug_toggle(Regulator* self, int unused);

#define REG_CAN_RECEIVE(reg, msg) SYNC(reg, reg_can_receive, msg)
#define REG_DEBUG_TOGGLE(reg) SYNC(reg, reg_debug_toggle, 0)

#define __CAN_MESSAGE(msg_mid) {msg_mid, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}}
