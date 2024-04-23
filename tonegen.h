#define __TONEGEN_START_VOLUME  10
#define __TONEGEN_MAX_VOLUME    20
#define __TONEGEN_MIN_VOLUME    0

#define __TONEGEN_START_PERIOD  1136
#define __TONEGEN_DEADLINE      300

typedef struct {
    Object super;
    int out;
    int period;
    int vol;
    int playing;
    int dlcontrol;
    int mute;
} ToneGen;

#define initToneGen    { initObject(), 0, __TONEGEN_START_PERIOD, __TONEGEN_START_VOLUME, 0, 1, 1 }

void __tonegen_switch(ToneGen *self, int unused);
//void __tonegen_switch_sync_test(ToneGen *self, int unused);
void tonegen_play(ToneGen *self, int unused);
void tonegen_stop(ToneGen *self, int unused);
void tonegen_period_set(ToneGen *self, int period);
//void tonegen_set_dlcontrol(ToneGen *self, int dlcontrol);
void tonegen_set_mute(ToneGen *self, int mute);
void tonegen_vol_incr(ToneGen *self, int unused);
void tonegen_vol_decr(ToneGen *self, int unused);

#define TONE_PLAY(tonegen)                  ASYNC(tonegen, tonegen_play, 0)
#define TONE_STOP(tonegen)                  SYNC(tonegen, tonegen_stop, 0)
#define TONE_PERIOD_SET(tonegen, period)    SYNC(tonegen, tonegen_period_set, period)
#define TONE_VOL_INCR(tonegen)              SYNC(tonegen, tonegen_vol_incr, 0)
#define TONE_VOL_DECR(tonegen)              SYNC(tonegen, tonegen_vol_decr, 0)
//#define TONE_DEADLINE_ON(tonegen)           SYNC(tonegen, tonegen_set_dlcontrol, 1)
//#define TONE_DEADLINE_OFF(tonegen)          SYNC(tonegen, tonegen_set_dlcontrol, 0)
#define TONE_MUTE_ON(tonegen)               SYNC(tonegen, tonegen_set_mute, 0)
#define TONE_MUTE_OFF(tonegen)              SYNC(tonegen, tonegen_set_mute, 1)