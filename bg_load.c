#include "TinyTimber.h"
#include "bg_load.h"
#include <stdlib.h>
#include <stdio.h>

void __bgload_loop(BgLoad *self, int unused) {
    
    // Stop if running turned off
    if (self->running == 0) {
        return;
    }
    
    // Empty loop range number of times
    for (int i; i < self->range; i++) {}
    
    // Schedule next loop
    int deadline = 0;
    if (self->dlcontrol) {
        deadline = __BGLOAD_DEADLINE;
    }
    SEND(USEC(__BGLOAD_PERIOD), USEC(deadline), self, __bgload_loop, 0);
}

void __bgload_loop_sync_test(BgLoad *self, int unused) {

    // Stop if running turned off
    if (self->running == 0) {
        return;
    }
    
    // Empty loop range number of times
    for (int i; i < self->range; i++) {}
    
    // Schedule next loop
    int deadline = 0;
    if (self->dlcontrol) {
        deadline = __BGLOAD_DEADLINE;
    }
}

void bgload_start(BgLoad *self, int unused) {
    
    // Only start playing if not playing already
    if (self->running == 0) {
        self->running = 1;    
    
        // Schedule first switch
        int deadline = 0;
        if (self->dlcontrol) {
            deadline = __BGLOAD_DEADLINE;
        }
        SEND(USEC(__BGLOAD_PERIOD), USEC(deadline), self, __bgload_loop, 0);
    }
}

void bgload_stop(BgLoad *self, int unused) {
    self->running = 0;
}

void bgload_set_dlcontrol(BgLoad *self, int dlcontrol) {
    self->dlcontrol = dlcontrol;
}

void bgload_rng_incr(BgLoad *self, int unused) {
    if (self->range < __BGLOAD_MAX_RNG) self->range = self->range + __BGLOAD_RNG_STEP;
}

void bgload_rng_decr(BgLoad *self, int unused) {
    if (self->range > __BGLOAD_MIN_RNG) self->range = self->range - __BGLOAD_RNG_STEP;
}

void bgload_rng_set(BgLoad *self, int range) {
    self->range = range;
}
