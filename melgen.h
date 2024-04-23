#define __MELGEN_START_HALFBEAT     250000
#define __MELGEN_START_KEY          0

#define __MELGEN_DEADLINE           1000
#define __MELGEN_OFFSET             50000

typedef struct {
    Object super;
    ToneGen *tone;
    int halfbeat;
    int key;
    int index;
    int playing;
} MelGen;

#define initMelGen(tone)    { initObject(), tone, __MELGEN_START_HALFBEAT, __MELGEN_START_KEY, 0, 0 }

void __melgen_schedule(MelGen *self, int unused);
void melgen_play(MelGen *self, int unused);
void melgen_stop(MelGen *self, int unused);
void melgen_reset(MelGen *self, int unused);
void melgen_set_key(MelGen *self, int key);
void melgen_set_tempo(MelGen *self, int tempo);
void melgen_set_mute(MelGen *self, int mute);
void melgen_vol_incr(MelGen *self, int unused);
void melgen_vol_decr(MelGen *self, int unused);

#define MELODY_PLAY(melgen)                 ASYNC(melgen, melgen_play, 0)
#define MELODY_STOP(melgen)                 SYNC(melgen, melgen_stop, 0)
#define MELODY_RESET(melgen)                SYNC(melgen, melgen_reset, 0)
#define MELODY_KEY_SET(melgen, key)         SYNC(melgen, melgen_set_key, key)
#define MELODY_TEMPO_SET(melgen, tempo)     SYNC(melgen, melgen_set_tempo, tempo)
#define MELODY_MUTE_ON(melgen)              SYNC(melgen, melgen_set_mute, 0)
#define MELODY_MUTE_OFF(melgen)             SYNC(melgen, melgen_set_mute, 1)
#define MELODY_VOL_INCR(melgen)             SYNC(melgen, melgen_vol_incr, 0)
#define MELODY_VOL_DECR(melgen)             SYNC(melgen, melgen_vol_decr, 0)
