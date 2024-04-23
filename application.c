#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    Object super;
    int count;
    char buf[20];
    int int_buf[3];
    int int_count;
} App;

App app = { initObject(), 0, "", {}, 0 };

void reader(App*, int);
void receiver(App*, int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    SCI_WRITE(&sci0, "Rcccv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
    
    if (c >= 48 && c <= 57) { // If c is a numerical ascii character
        SCI_WRITE(&sci0, "NUMERICAL ASCII CHAR\n");
        self->buf[self->count] = c;
        self->count = self->count + 1;
    } 
    else if (c == '-' && self->count == 0) { // If c is a minus sign and the first character
        SCI_WRITE(&sci0, "MINUS\n");
        self->buf[0] = '-';
        self->count = self->count + 1;
    } 
    else if (c == 'f') { // Clear intbuf
        SCI_WRITE(&sci0, "CLEAR INTBUF\n");
        self->int_count = 0;
    } 
    else if (c == 'e') { // End of string (print and reset)
        SCI_WRITE(&sci0, "EOS\n");
        
        self->buf[self->count] = '\0';
        self->count = 0;
        
        self->int_buf[2] = self->int_buf[1];
        self->int_buf[1] = self->int_buf[0];
        self->int_buf[0] = atoi(self->buf);
        
        if (self->int_count < 3) self->int_count = self->int_count + 1;
        
        // Print new integer
        SCI_WRITE(&sci0, "The integer is: \'");
        char str[20];
        sprintf(str,"%d", self->int_buf[0]);
        SCI_WRITE(&sci0, str);
        
        // Find sum and median of int_buf (depending on int_count)
        int sum, median;
        if (self->int_count == 1) {
            sum = self->int_buf[0];
            median = self->int_buf[0];
        } 
        else if (self->int_count == 2) {
            sum = self->int_buf[0] + self->int_buf[1];
            median = sum / 2;
        } 
        else if (self->int_count == 3) {
            sum = self->int_buf[0] + self->int_buf[1] + self->int_buf[2];
            // int_buf[0] is median
            if ((self->int_buf[0] >= self->int_buf[1] && self->int_buf[0] <= self->int_buf[2]) ||
               (self->int_buf[0] >= self->int_buf[2] && self->int_buf[0] <= self->int_buf[1])) {
               median = self->int_buf[0];
            }
            // int_buf[1] is median
            else if ((self->int_buf[1] >= self->int_buf[0] && self->int_buf[1] <= self->int_buf[2]) ||
               (self->int_buf[1] >= self->int_buf[2] && self->int_buf[1] <= self->int_buf[0])) {
               median = self->int_buf[1];
            }
            // int_buf[2] is median
            else {
                median = self->int_buf[2];
            }
        }
        
        // Print sum and median of history
        SCI_WRITE(&sci0, "\', sum of history is: \'");
        sprintf(str,"%d", sum);
        SCI_WRITE(&sci0, str);
        SCI_WRITE(&sci0, "\', median is: \'");
        sprintf(str,"%d", median);
        SCI_WRITE(&sci0, str);
        SCI_WRITE(&sci0, "\'.\n");
        
    } else { // Invalid character
        SCI_WRITE(&sci0, "Invalid character!\n");
        self->count = 0;
    }
    
    // Error catching: Array out of bounds!
    if (self->count >= 20) {
        SCI_WRITE(&sci0, "Error: Input too long!\n");
        self->count = 0;
    }
}

void startApp(App *self, int arg) {
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
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
