#define __BGLOAD_PERIOD     1000
#define __BGLOAD_DEADLINE   300

#define __BGLOAD_START_RNG  1000
#define __BGLOAD_MAX_RNG    8000
#define __BGLOAD_MIN_RNG    1000
#define __BGLOAD_RNG_STEP   500

typedef struct {
    Object super;
    int range;
    int running;
    int dlcontrol;
} BgLoad;

#define initBgLoad    { initObject(), __BGLOAD_START_RNG, 0, 1 }

void __bgload_loop(BgLoad *self, int unused);
void __bgload_loop_sync_test(BgLoad *self, int unused);
void bgload_start(BgLoad *self, int unused);
void bgload_stop(BgLoad *self, int unused);
void bgload_rng_incr(BgLoad *self, int unused);
void bgload_rng_decr(BgLoad *self, int unused);
void bgload_rng_set(BgLoad *self, int range);
void bgload_set_dlcontrol(BgLoad *self, int dlcontrol);

#define BGLOAD_START(bgload)            ASYNC(bgload, bgload_start, 0)
#define BGLOAD_STOP(bgload)             SYNC(bgload, bgload_stop, 0)
#define BGLOAD_RNG_INCR(bgload)         SYNC(bgload, bgload_rng_incr, 0)
#define BGLOAD_RNG_DECR(bgload)         SYNC(bgload, bgload_rng_decr, 0)
#define BGLOAD_DEADLINE_ON(bgload)      SYNC(bgload, bgload_set_dlcontrol, 1)
#define BGLOAD_DEADLINE_OFF(bgload)     SYNC(bgload, bgload_set_dlcontrol, 0)
