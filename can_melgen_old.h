
#include "canTinyTimber.h"
#include "sciTinyTimber.h"

#define __MELGEN_START_HALFBEAT     250000
#define __MELGEN_START_TEMPO        120
#define __MELGEN_START_KEY          0

#define __MELGEN_DEADLINE           1000
#define __MELGEN_OFFSET             50000

#define __MELGEN_MUSICIAN           1
#define __MELGEN_CONDUCTOR          0

#define __MSGID_HEART_BEAT          1
#define __MSGID_PLAY_NOTE           2
#define __MSGID_UPDATE_VALUES       3

#define __HEART_BEAT_PERIOD         MSEC(200)
#define __HEART_BEAT_DEADLINE       MSEC(5)
#define __HEART_BEATS_TIMEOUT       3

#define __REQ_CON_WAIT_TIME         5

typedef struct {
    unsigned char mode;
    unsigned char active_rank;
    unsigned char req_con;
    unsigned int timestamp;
    unsigned char timeout;
} MusicUnit;

typedef struct {
    Object super;
    Can *can;
    Serial *sci;
    ToneGen *tone;
    unsigned char debug; // 0: Normal, 1: Debug
    
    // Communication protocol
    unsigned char node;
    unsigned char mode;
    unsigned int hb_time;
    unsigned char req_con;
    unsigned int req_con_timestamp;
    unsigned char leader_no;
    MusicUnit units[4];
    
    // Music controls protocol
    unsigned char active_rank;
    unsigned char active_units;
    unsigned char playing;
    unsigned int halfbeat;
    unsigned int tempo;
    char key;
    unsigned char index;
} MelGen;

#define initUnit                                    { __MELGEN_MUSICIAN, 0, 0, 1 }
#define initUnits                                   { initUnit, initUnit, initUnit, initUnit }
#define initMelGen(can, sci, tone, node, debug)     { initObject(), can, sci, tone, debug, node, __MELGEN_MUSICIAN, 0, 0, 0, 0, initUnits, 0, 0, 0, __MELGEN_START_HALFBEAT, __MELGEN_START_TEMPO, __MELGEN_START_KEY, 0 }

// Heart beat (mode, leader_no, req_conduct, conduct_type)
#define __CANMSG_HEART_BEAT(msg_nid, msg_m, msg_ln, msg_rc, msg_ct)                    {__MSGID_HEART_BEAT, msg_nid, 4, {msg_m, msg_ln, msg_rc, msg_ct, 0, 0, 0, 0}}

// Note play (recv_node, song_index, tempo, key, volume, leader_no)
#define __CANMSG_NOTE_PLAY(msg_nid, msg_rn, msg_si, msg_t, msg_k, msg_v, msg_ln)       {__MSGID_PLAY_NOTE, msg_nid, 6, {msg_rn, msg_si, msg_t, msg_k, msg_v, msg_ln, 0, 0}}

// Update values (tempo, key, volume, nextnote, play/pause state, leader_no)
#define __CANMSG_UPDATE_VALUES(msg_nid, msg_t, msg_k, msg_v, msg_nn, msg_pp, msg_ln)   {__MSGID_UPDATE_VALUES, msg_nid, 6, {msg_t, msg_k, msg_v, msg_nn, msg_pp, msg_ln, 0, 0}}

void __melgen_schedule(MelGen *self, int unused);
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
void __meglen_force_cond(MelGen *self, int unused);
void __melgen_debug_report(MelGen *self, int unused);

#define MELODY_PLAY(melgen)                 SYNC(melgen, melgen_play, 0)
#define MELODY_CAN_RECEIVE(melgen, msg)     SYNC(melgen, melgen_can_receive, msg)
#define MELODY_START_HEARTBEAT(melgen)      ASYNC(melgen, __melgen_heart_beat, 0)
#define MELODY_STOP(melgen)                 SYNC(melgen, melgen_stop, 0)
#define MELODY_RESET(melgen)                SYNC(melgen, melgen_reset, 0)
#define MELODY_KEY_SET(melgen, key)         SYNC(melgen, melgen_set_key, key)
#define MELODY_TEMPO_SET(melgen, tempo)     SYNC(melgen, melgen_set_tempo, tempo)
#define MELODY_MUTE_ON(melgen)              SYNC(melgen, melgen_set_mute, 0)
#define MELODY_MUTE_OFF(melgen)             SYNC(melgen, melgen_set_mute, 1)
#define MELODY_VOL_INCR(melgen)             SYNC(melgen, melgen_vol_incr, 0)
#define MELODY_VOL_DECR(melgen)             SYNC(melgen, melgen_vol_decr, 0)
#define MELODY_FORCE_COND(melgen)           SYNC(melgen, __meglen_force_cond, 0)
#define MELODY_DEBUG_REPORT(melgen)         SYNC(melgen, __melgen_debug_report, 0)
