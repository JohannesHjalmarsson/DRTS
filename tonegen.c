#include "TinyTimber.h"
#include "tonegen.h"
#include <stdlib.h>
#include <stdio.h>

volatile int *__TONEGEN_DAC_ADDR = (int *) 0x4000741C;

void __tonegen_switch(ToneGen *self, int unused) {
    
    // Stop if playing turned off
    if (self->playing == 0) {
        self->out = 0;
        *__TONEGEN_DAC_ADDR = 0;
        return;
    }
    
    // Invert out and apply volume change
    if (self->out != 0) {
        self->out = 0;
    } else {
        self->out = self->vol * self->mute;
    }
    
    // Write to DAC
    *__TONEGEN_DAC_ADDR = self->out;
    
    // Schedule next switch
    int deadline = 0;
    if (self->dlcontrol) {
        deadline = __TONEGEN_DEADLINE;
    }
    SEND(USEC(self->period), USEC(deadline), self, __tonegen_switch, 0);
}

/*
void __tonegen_switch_sync_test(ToneGen *self, int unused) {
    
    // Stop if playing turned off
    if (self->playing == 0) {
        self->out = 0;
        *__TONEGEN_DAC_ADDR = 0;
        return;
    }
    
    // Invert out and apply volume change
    if (self->out != 0) {
        self->out = 0;
    } else {
        self->out = self->vol;
    }
    
    // Write to DAC
    *__TONEGEN_DAC_ADDR = self->out;
}
*/

void tonegen_play(ToneGen *self, int unused) {
    
    // Only start playing if not playing already
    if (self->playing == 0) {
        self->playing = 1;    
    
        // Schedule first switch
        BEFORE(USEC(__TONEGEN_DEADLINE), self, __tonegen_switch, 0);
    }
}

void tonegen_stop(ToneGen *self, int unused) {
    self->playing = 0;
}

void tonegen_period_set(ToneGen *self, int period) {
    self->period = period;
}

/*
void tonegen_set_dlcontrol(ToneGen *self, int dlcontrol) {
    self->dlcontrol = dlcontrol;
}
*/

void tonegen_set_mute(ToneGen *self, int mute) {
    self->mute = mute;
}

void tonegen_vol_incr(ToneGen *self, int unused) {
    if (self->vol < __TONEGEN_MAX_VOLUME) self->vol = self->vol + 1;
}

void tonegen_vol_decr(ToneGen *self, int unused) {
    if (self->vol > __TONEGEN_MIN_VOLUME) self->vol = self->vol - 1;
}
