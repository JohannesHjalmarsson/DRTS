#include "TinyTimber.h"
#include "tonegen.h"
#include "melgen.h"
#include <stdlib.h>
#include <stdio.h>

const int BROTHER_JOHN_TONE[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
const int BROTHER_JOHN_HALFBEATS[32] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 4, 2, 2, 4};

const int PERIOD[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

void __melgen_schedule(MelGen *self, int unused) {
    
    // Stop if playing turned off
    if (self->playing == 0) {
        BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);
        return;
    }
    
    SYNC(self->tone, tonegen_period_set, PERIOD[BROTHER_JOHN_TONE[self->index]+self->key+10]);
    
    // Start tone now deadline
    BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_play, 0);
    
    // Stop tone after beat-offset
    SEND(USEC(self->halfbeat*BROTHER_JOHN_HALFBEATS[self->index]-__MELGEN_OFFSET), USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);
    
    // Start next scheduling after beat
    SEND(USEC(self->halfbeat*BROTHER_JOHN_HALFBEATS[self->index]), USEC(__MELGEN_DEADLINE), self, __melgen_schedule, 0);
    
    if (self->index >= 31) {
        self->index = 0;
    } else {
        self->index = self->index + 1;
    }
}

void melgen_play(MelGen *self, int unused) {
    
    // Only start playing if not playing already
    if (self->playing == 0) {
        self->playing = 1;    
    
        // Schedule first scheduling
        ASYNC(self, __melgen_schedule, 0);
    }
}

void melgen_stop(MelGen *self, int unused) {
    self->playing = 0;
}

void melgen_reset(MelGen *self, int unused) {
    //self->playing = 0;
    self->index = 0;
    self->halfbeat = __MELGEN_START_HALFBEAT;
    self->key = __MELGEN_START_KEY;
}

void melgen_set_key(MelGen *self, int key) {
    if (key >= -5 && key <= 5) self->key = key;;
}

void melgen_set_tempo(MelGen *self, int tempo) {
    // Convert bpm into halfbeats
    if (tempo >= 60 && tempo <= 240) self->halfbeat = 30000000/tempo;;
}

void melgen_set_mute(MelGen *self, int mute) {
    SYNC(self->tone, tonegen_set_mute, mute);
}

void melgen_vol_incr(MelGen *self, int unused) {
    TONE_VOL_INCR(self->tone);
}

void melgen_vol_decr(MelGen *self, int unused) {
    TONE_VOL_DECR(self->tone);
}
