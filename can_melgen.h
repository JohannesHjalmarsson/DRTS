
#include "canTinyTimber.h"
#include "sciTinyTimber.h"

#define __MELGEN_START_TEMPO                120
#define __MELGEN_MIN_TEMPO                  60
#define __MELGEN_MAX_TEMPO                  240

#define __MELGEN_START_KEY                  5
#define __MELGEN_MIN_KEY                    0
#define __MELGEN_MAX_KEY                    10
#define __MELGEN_OFFSET_KEY             -5

#define __MELGEN_DEADLINE                   1000
#define __MELGEN_OFFSET                     50000

#define __MELGEN_MUSICIAN                   1
#define __MELGEN_CONDUCTOR                  0

#define __MELGEN_MSGID_HEART_BEAT           1
#define __MELGEN_MSGID_PLAY_NOTE            2
#define __MELGEN_MSGID_UPDATE_VALUES        3

#define __MELGEN_HEART_BEAT_PERIOD          MSEC(200)
#define __MELGEN_HEART_BEAT_DEADLINE        MSEC(5)
#define __MELGEN_HEART_BEATS_TIMEOUT        3

#define __MELGEN_PRINT_TEMPO_PERIOD          MSEC(5000)
#define __MELGEN_PRINT_MUTED_PERIOD          MSEC(5000)

#define __MELGEN_REQ_CON_WAIT_TIME          5

#define __SELF                   self->units[self->node]
#define __NODE(i)                self->units[i]

typedef struct {
    unsigned char mode;
    unsigned char active_rank;
    unsigned char req;
    unsigned int timestamp;
    unsigned char timeout;
} MusicUnit;

typedef struct {
    // System objects/variables
    Object super;
    Can *can;
    Serial *sci;
    ToneGen *tone;
    unsigned char debug; // 0: Normal, 1: Debug
    unsigned char init;
    
    // Communication protocol
    unsigned char node;
    int time;
    unsigned char leader_no;
    MusicUnit units[4];
    unsigned char active_units;
    int req_timestamp;
    
    // Music controls
    unsigned char playing;
    unsigned char tempo;
    unsigned char key;
    unsigned char index;
    
    // Report tempo
    unsigned char print_tempo;
    unsigned char print_muted;
    unsigned char force_failiure;
    unsigned char silent;
    unsigned char connected;
} MelGen;

#define initUnit                                    { __MELGEN_MUSICIAN, 0, 0, 0, 1 }
#define initUnits                                   { initUnit, initUnit, initUnit, initUnit }
#define initMelGen(can, sci, tone, node, debug)     { initObject(), can, sci, tone, debug, 0, node, 0, 0, initUnits, 0, 0, 0, __MELGEN_START_TEMPO, __MELGEN_START_KEY, 0, 0, 0, 0, 0, 0}

// Heart beat (mode, leader_no, req_conduct, conduct_type)
#define __MELGEN_CANMSG_HEART_BEAT(msg_nid, msg_m, msg_ln, msg_rc, msg_ct)                    {__MELGEN_MSGID_HEART_BEAT, msg_nid, 4, {msg_m, msg_ln, msg_rc, msg_ct, 0, 0, 0, 0}}

// Note play (recv_node, song_index, tempo, key, volume, leader_no)
#define __MELGEN_CANMSG_NOTE_PLAY(msg_nid, msg_rn, msg_si, msg_t, msg_k, msg_v, msg_ln)       {__MELGEN_MSGID_PLAY_NOTE, msg_nid, 6, {msg_rn, msg_si, msg_t, msg_k, msg_v, msg_ln, 0, 0}}

// Update values (tempo, key, volume, nextnote, play/pause state, leader_no)
#define __MELGEN_CANMSG_UPDATE_VALUES(msg_nid, msg_t, msg_k, msg_v, msg_nn, msg_pp, msg_ln)   {__MELGEN_MSGID_UPDATE_VALUES, msg_nid, 6, {msg_t, msg_k, msg_v, msg_nn, msg_pp, msg_ln, 0, 0}}

void melgen_init(MelGen *self, int unused);
void __melgen_conductor(MelGen *self, int unused);
void __melgen_heart_beat(MelGen *self, int unused);
void melgen_can_receive(MelGen *self, CANMsg *msg);
void melgen_play(MelGen *self, int unused);
void melgen_stop(MelGen *self, int unused);
void melgen_reset(MelGen *self, int unused);
void melgen_set_key(MelGen *self, int key);
void melgen_set_tempo(MelGen *self, int tempo);
void melgen_set_mute(MelGen *self, int mute);
void melgen_vol_incr(MelGen *self, int unused);
void melgen_vol_decr(MelGen *self, int unused);
void meglen_req_cond(MelGen *self, int unused);
void meglen_force_cond(MelGen *self, int unused);
void melgen_report_tempo_toggle(MelGen *self, int unused);
void melgen_report_muted_toggle(MelGen *self, int unused);
void __melgen_report_tempo(MelGen *self, int unused);
void __melgen_report_muted(MelGen *self, int unused);
void melgen_f1_toggle(MelGen *self, int unused);
void melgen_f2(MelGen *self, int unused);
void __melgen_f2_end(MelGen *self, int unused);
void melgen_report(MelGen *self, int unused);
void melgen_debug_toggle(MelGen *self, int unused);

#define MELODY_INIT(melgen)                 SYNC(melgen, melgen_init, 0)
#define MELODY_CAN_RECEIVE(melgen, msg)     SYNC(melgen, melgen_can_receive, msg)
#define MELODY_PLAY(melgen)                 SYNC(melgen, melgen_play, 0)
#define MELODY_STOP(melgen)                 SYNC(melgen, melgen_stop, 0)
#define MELODY_RESET(melgen)                SYNC(melgen, melgen_reset, 0)
#define MELODY_KEY_SET(melgen, key)         SYNC(melgen, melgen_set_key, key)
#define MELODY_TEMPO_SET(melgen, tempo)     SYNC(melgen, melgen_set_tempo, tempo)
#define MELODY_MUTE_ON(melgen)              SYNC(melgen, melgen_set_mute, 0)
#define MELODY_MUTE_OFF(melgen)             SYNC(melgen, melgen_set_mute, 1)
#define MELODY_VOL_INCR(melgen)             SYNC(melgen, melgen_vol_incr, 0)
#define MELODY_VOL_DECR(melgen)             SYNC(melgen, melgen_vol_decr, 0)
#define MELODY_REQ_COND(melgen)             SYNC(melgen, meglen_req_cond, 0)
#define MELODY_FORCE_COND(melgen)           SYNC(melgen, meglen_force_cond, 0)
#define MELODY_PRINT_TEMPO_TOGGLE(melgen)   SYNC(melgen, melgen_report_tempo_toggle, 0)
#define MELODY_PRINT_MUTED_TOGGLE(melgen)   SYNC(melgen, melgen_report_muted_toggle, 0)
#define MELODY_F1_TOGGLE(melgen)            SYNC(melgen, melgen_f1_toggle, 0)
#define MELODY_F2(melgen)                   SYNC(melgen, melgen_f2, 0)
#define MELODY_REPORT(melgen)               SYNC(melgen, melgen_report, 0)
#define MELODY_DEBUG_TOGGLE(melgen)         SYNC(melgen, melgen_debug_toggle, 0)
