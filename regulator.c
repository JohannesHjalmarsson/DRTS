#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "regulator.h"
#include <stdlib.h>
#include <stdio.h>

void reg_can_receive(Regulator* self, unsigned char msg) {
    
    // Debug message
    if (self->debug) {
        char text[55];
        sprintf(text, "Debug: Message %d incoming!\n", msg);
        SCI_WRITE(self->sci, text);
    }
        
    // If the system is standby, i.e. the buffer is empty and messages can be handled immidiately
    if (self->standby) {
        
        // System no longer on standby
        self->standby = 0;
        
        // Debug message
        if (self->debug) {
            char text[55];
            sprintf(text, "Debug: Exiting standby mode and handling message %d!\n", msg);
            SCI_WRITE(self->sci, text);
        }
        
        // Handle message
        char text[58];
        sprintf(text, "Message %d immidiately delivered!\n", msg);
        SCI_WRITE(self->sci, text);
        
        // Schedule first followup
        AFTER(__REG_TIME, self, __reg_followup, 0);
    }
    
    // If the system buffer is full and a message has to be discarded :(
    else if ((self->write_index == self->read_index-1) || 
        ((self->write_index == __REG_BUFSIZE-1) && (self->read_index == 0))) {
        
        // Debug message
        if (self->debug) {
            char text[31];
            sprintf(text, "Debug: Discarded message %d!\n", msg);
            SCI_WRITE(self->sci, text);
        }
        
        return;
    }
    
    // If the system is not standby, and messages need to be regulated
    else {
        
        // Debug message
        if (self->debug) {
            char text[50];
            sprintf(text, "Debug: Added message %d to place %d in buffer!\n", msg, self->write_index);
            SCI_WRITE(self->sci, text);
        }
        
        // Add message to buffer
        self->buffer[self->write_index] = msg;
        
        // Increment write index
        self->write_index++;
        if (self->write_index == __REG_BUFSIZE)
            self->write_index = 0;
    }
}

void __reg_followup(Regulator* self, int unused) {
    
    Time start = CURRENT_OFFSET();
    
    // If message buffer is empty, return to standby mode
    if (self->read_index == self->write_index) {
        
        // Debug message
        if (self->debug) {
            char text[44];
            sprintf(text, "Debug: Buffer empty, returning to standby!\n");
            SCI_WRITE(self->sci, text);
        }
        
        // Return to standby mode
        self->standby = 1;
        return;
    }
    
    // If message buffer is not empty, handle next message
    else {
        // Debug message
        if (self->debug) {
            char text[51];
            sprintf(text, "Debug: Read message %d from place %d in buffer!\n", self->buffer[self->read_index], self->read_index);
            SCI_WRITE(self->sci, text);
        }
        
        // Handle message
        char text[55];
        sprintf(text, "Message %d ready for delivery!\n", self->buffer[self->read_index]);
        SCI_WRITE(self->sci, text);
        
        // Increment read index
        self->read_index++;
        if (self->read_index == __REG_BUFSIZE)
            self->read_index = 0;
        
        // Schedule next followup
        AFTER(__REG_TIME, self, __reg_followup, 0);
        
        Time end = CURRENT_OFFSET();
        Time diff = end-start;
        sprintf(text, "Time diff: %li (%li - %li)\n", diff, end, start);
        SCI_WRITE(self->sci, text);
    }
}

void reg_debug_toggle(Regulator* self, int unused) {
    
    // Toggle debug
    if (self->debug == 1) {
        self->debug = 0;
    }
    else {
        self->debug = 1;
    }
}