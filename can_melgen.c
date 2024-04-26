#include "TinyTimber.h"
#include "tonegen.h"
#include "can_melgen.h"
#include <stdlib.h>
#include <stdio.h>

#define TASK_4 1

// TODO: Fix the mute

const int BROTHER_JOHN_TONE[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
const int BROTHER_JOHN_HALFBEATS[32] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 4, 2, 2, 4};
const int BROTHER_JOHN_LENGTH = 32;

const int PERIOD[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

void melgen_init(MelGen *self, int unused) {
    if (self->init == 0) {
        ASYNC(self, __melgen_heart_beat, 0);
        self->init = 1;
    }
}

// Scheduler/CAN message send for conductor mode
void __melgen_conductor(MelGen *self, int unused) {
    
    // Stop if playing turned off or musician
    if (self->playing == 0 || __SELF.mode == __MELGEN_MUSICIAN) {
        return;
    }
    
    // Determine which active 
    unsigned char player_active_rank = self->index % self->active_units;
    
    // Calculate tempo halfbeat period
    unsigned int half_beat = 30000000/self->tempo;
    
    // If conductor is supposed to play, schedule tone
    if (player_active_rank == __SELF.active_rank) {
        
        // Debug message
        if (self->debug) {
            char text[64];
            sprintf(text,"Debug (%d, %d): Conductor playing note %d self [tempo: %d, key: %d]\n", self->node, __SELF.mode, self->index, self->tempo, self->key);
            SCI_WRITE(self->sci, text);
        }
        
        if (!self->force_failiure) {
            SYNC(self->tone, tonegen_period_set, PERIOD[BROTHER_JOHN_TONE[self->index]+self->key+10+__MELGEN_OFFSET_KEY]);
            
            // Start tone now deadline
            BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_play, 0);
            
            // Stop tone after beat-offset
            SEND(USEC(half_beat*BROTHER_JOHN_HALFBEATS[self->index]-__MELGEN_OFFSET), USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);
        }

        // Send CAN message
        CANMsg msg = __MELGEN_CANMSG_NOTE_PLAY(self->node, self->node, self->index, self->tempo, self->key, self->tone->vol, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    } 
    
    // If a musician is supposed to play note, send CAN message
    else {
        
        // Determine node id of note player
        unsigned char player_node = self->node;
        for (int i = 0; i < 4; i++) {
            if (player_active_rank == __NODE(i).active_rank) {
                player_node = i;
            }
        }
        
        // Debug message
        if (self->debug) {
            char text[73];
            sprintf(text,"Debug (%d, %d): Sending note %d to (%d, %d) [tempo: %d, key: %d]\n", self->node, __SELF.mode, self->index, player_node, __NODE(player_node).mode, self->tempo, self->key);
            SCI_WRITE(self->sci, text);
        }
        
        // Send CAN message
        CANMsg msg = __MELGEN_CANMSG_NOTE_PLAY(self->node, player_node, self->index, self->tempo, self->key, self->tone->vol, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
    
    // Schedule next task
    SEND(USEC(half_beat*BROTHER_JOHN_HALFBEATS[self->index]), USEC(__MELGEN_DEADLINE), self, __melgen_conductor, 0);
    
    // Next index
    if (self->index >= 31) {
        self->index = 0;
    } else {
        self->index = self->index + 1;
    }
}

void __melgen_heart_beat(MelGen *self, int unused) {
    
    // Increment heartbeat time
    self->time = self->time + 1;
    __SELF.timestamp = self->time;
    __SELF.timeout = 0;
    
    // Handle timeouts, ranks and active units
    unsigned char active_units = 0;
    #ifdef TASK_4
    unsigned char cond_needed = 1;
    #endif
    for (int i = 0; i < 4; i++) {
        if ((__NODE(i).timestamp + __MELGEN_HEART_BEATS_TIMEOUT < self->time)) {
            
            // Debug
            if (self->debug && (self->units[i].timeout == 0)) {
                char text[81];
                sprintf(text,"Debug (%u, %u): Timeout on node %d!\n", self->node, __SELF.mode, i);
                SCI_WRITE(self->sci, text);
            }
            
            __NODE(i).timeout = 1;
        }
        else {
            // Set active rank
            __NODE(i).active_rank = active_units;
            
            // Handle active units
            active_units++;
            
            // Check if conductor request needed
            #ifdef TASK_4
            if ((__NODE(i).mode == __MELGEN_CONDUCTOR) || (__NODE(i).req == 1)) {
                cond_needed = 0;
            }
            #endif
        }
    }
    self->active_units = active_units;
    
    // Find out if system is alone on network
    unsigned char alone = 1;
    for (int i = 0; i < 4; i++) {
        if (__NODE(i).timeout == 0 && i != self->node)
            alone = 0;
    }

    // If connected to network and then disconnected, set silent to 1.
    if (self->connected && alone && !self->silent) {
        self->silent = 1;
        SCI_WRITE(self->sci, "Enter silent failiure.\n");
    }
    
    // If conductor is needed and is lowest rank, claim conductorship unless alone on the network
    #ifdef TASK_4
    if (cond_needed && (__SELF.active_rank == 0) && !alone) {
        
        // Set self as conductor and end request
        __SELF.mode = __MELGEN_CONDUCTOR;
        __SELF.req = 0;
        self->req_timestamp = 0;
        
        self->leader_no = self->leader_no + 1;
        
        SCI_WRITE(self->sci, "Claimed conductorship.\n");
        
        if (self->playing) {
            ASYNC(self, __melgen_conductor, 0);
        }
    }
    #endif

    // If requesting conductor and have waited
    else if (__SELF.req && (self->req_timestamp + __MELGEN_REQ_CON_WAIT_TIME < self->time)) {
    
        // Set self as conductor and end request
        __SELF.mode = __MELGEN_CONDUCTOR;
        __SELF.req = 0;
        self->req_timestamp = 0;
        
        self->leader_no = self->leader_no + 1;
        
        SCI_WRITE(self->sci, "Claimed conductorship.\n");
        
        if (self->playing) {
            ASYNC(self, __melgen_conductor, 0);
        }
    }
    
    // Debug message
    if (self->debug) {
        char text[50];
        sprintf(text,"Debug (%u, %u): Heartbeat [leader_no: %u, req: %u]\n", self->node, __SELF.mode, self->leader_no, __SELF.req);
        SCI_WRITE(self->sci, text);
    }
    
    // Send CAN message
    CANMsg msg = __MELGEN_CANMSG_HEART_BEAT(self->node, __SELF.mode, self->leader_no, __SELF.req, 0);
    if (!self->force_failiure) {
        CAN_SEND(self->can, &msg);
    }
    
    // Schedule next heart beat
    SEND(__MELGEN_HEART_BEAT_PERIOD, __MELGEN_HEART_BEAT_DEADLINE, self, __melgen_heart_beat, 0);
}

void melgen_can_receive(MelGen *self, CANMsg *msg) {
    
    // Force silence
    if (self->force_failiure) {
        return;
    }
    
    // Check if just exited silent failiure
    if (self->silent) {
        self->silent = 0;
        SCI_WRITE(self->sci, "Leave silent failiure.\n");

        #ifdef TASK_4
        // Set self to musician
        __SELF.mode = __MELGEN_MUSICIAN;
        
        SCI_WRITE(self->sci, "Joining back as musician!\n");
        #endif

        if (__SELF.mode == __MELGEN_MUSICIAN)
            self->playing = 0;
    }
    
    // Handle heart beat
    if (msg->msgId == __MELGEN_MSGID_HEART_BEAT) {
        
        // Set connected to 1
        if (!self->connected) {
            self->connected = 1;
            SCI_WRITE(self->sci, "System connected.\n");
        }

        // Update mode and timestamp in units array
        __NODE(msg->nodeId).mode = msg->buff[0];
        __NODE(msg->nodeId).timestamp = self->time;
        __NODE(msg->nodeId).req = msg->buff[2];
        
        // Update timeout
        if (__NODE(msg->nodeId).timeout) {
            
            // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%d, %d): Timed out node %d back!\n", self->node, __SELF.mode, msg->nodeId);
                    SCI_WRITE(self->sci, text);
                }
            
            __NODE(msg->nodeId).timeout = 0;
        }
        
        // Compare requests, stop request
        if (__NODE(msg->nodeId).req && __SELF.req && (msg->nodeId < self->node)) {
            
            // Erase own request
            __SELF.req = 0;
            
            // Debug message
            if (self->debug) {
                char text[81];
                sprintf(text,"Debug (%u, %u): Higher rank request from (%d, %d), ending request.\n", self->node, __SELF.mode, msg->nodeId, __NODE(msg->nodeId).mode);
                SCI_WRITE(self->sci, text);
            }
        }
        
        // Check for increments in leader_no, give up conductorship, stop request
        if (msg->buff[1] > self->leader_no) {
            
            if (self->force_failiure)
                return;
            
            // Give up conductorship if leader_no increments
            if (__SELF.mode == __MELGEN_CONDUCTOR) {
                
                // Set self to musician
                __SELF.mode = __MELGEN_MUSICIAN;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor, conductorship void!\n", self->node, __SELF.mode);
                    SCI_WRITE(self->sci, text);
                } else {
                    SCI_WRITE(self->sci, "Conductorship void!\n");
                }
            }
            
            // Stop request if leader_no increments
            if (__SELF.req == 1) {
                
                // Set self to musician
                __SELF.req = 0;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor, request ended!\n", self->node, __SELF.mode);
                    SCI_WRITE(self->sci, text);
                }
            }
            
            // Increment leader_no
            self->leader_no = msg->buff[1];
        }
        
        // Handle timeouts, ranks and active units
        unsigned char active_units = 0;
        for (int i = 0; i < 4; i++) {
            if (self->units[i].timeout != 1) {
                
                // Set active rank
                self->units[i].active_rank = active_units;
                if (i == self->node) {
                    __NODE(i).active_rank = active_units;
                }
                
                // Handle active units
                active_units++;
            }
        }
        self->active_units = active_units;
        
        // Debug message
        if (self->debug) {
            char text[80];
            sprintf(text,"Debug (%u, %u): Hearbeat from (%u, %u) recieved [leader_no: %u, req: %u]\n", self->node, __SELF.mode, msg->nodeId, __NODE(msg->nodeId).mode, msg->buff[1], msg->buff[2]);
            SCI_WRITE(self->sci, text);
        }
    } 
    
    // Handle play note
    else if (msg->msgId == __MELGEN_MSGID_PLAY_NOTE) {
        
        // Check for increments in leader_no, give up conductorship, stop request
        if (msg->buff[5] > self->leader_no) {
            
            // Give up conductorship if leader_no increments
            if (__SELF.mode == __MELGEN_CONDUCTOR) {
                
                // Set self to musician
                __SELF.mode = __MELGEN_MUSICIAN;
                
                // Debug message
                if (self->debug) {
                    char text[39];
                    sprintf(text,"Debug (%u, %u): Conductorship void!\n", self->node, __SELF.mode);
                    SCI_WRITE(self->sci, text);
                } else {
                    SCI_WRITE(self->sci, "Conductorship void!\n");
                }
            }
            
            // Stop request if leader_no increments
            if (__SELF.req == 1) {
                
                // Set self to musician
                __SELF.req = 0;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor, request ended!\n", self->node, __SELF.mode);
                    SCI_WRITE(self->sci, text);
                }
            }
            
            // Increment leader_no
            self->leader_no = msg->buff[5];
        }
        
        // Debug message
        if (self->debug) {
            char text[131];
            sprintf(text,"Debug (%d, %d): Play note from (%d, %d) recieved [node: %d, index: %d, tempo: %d, key: %d, volume: %d, leader_no: %d]\n", self->node, __SELF.mode, msg->nodeId, __NODE(msg->nodeId).mode, msg->buff[0], msg->buff[1], msg->buff[2], msg->buff[3], msg->buff[4], msg->buff[5]);
            SCI_WRITE(self->sci, text);
        }
        
        // Play note if musician and message is from conductor
        if ((__SELF.mode == __MELGEN_MUSICIAN) && (__NODE(msg->nodeId).mode == __MELGEN_CONDUCTOR) && (msg->buff[0] == self->node) && (msg->buff[5] == self->leader_no)) {
            
            // Update values
            self->index = msg->buff[1];
            self->tempo = msg->buff[2];
            self->key = msg->buff[3];
            self->tone->vol = msg->buff[4];
            self->playing = 1;
            
            // Debug message
            if (self->debug) {
                char text[47];
                sprintf(text,"Debug (%d, %d): Playing note %d [%d, %d]\n", self->node, __SELF.mode, self->index, self->tempo, self->key);
                SCI_WRITE(self->sci, text);
            }
        
            SYNC(self->tone, tonegen_period_set, PERIOD[BROTHER_JOHN_TONE[self->index]+self->key+10+__MELGEN_OFFSET_KEY]);

            // Start tone now deadline
            BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_play, 0);
            
            // Calculate tempo halfbeat period
            unsigned int half_beat = 30000000/self->tempo;

            // Stop tone after beat-offset
            SEND(USEC(half_beat*BROTHER_JOHN_HALFBEATS[self->index]-__MELGEN_OFFSET), USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);
        }
    }
    
    // Handle update values
    else if (msg->msgId == __MELGEN_MSGID_UPDATE_VALUES) {
        
        // Check for increments in leader_no, give up conductorship, stop request
        if (msg->buff[5] > self->leader_no) {
            
            // Give up conductorship if leader_no increments
            if (__SELF.mode == __MELGEN_CONDUCTOR) {
                
                // Set self to musician
                __SELF.mode = __MELGEN_MUSICIAN;
                
                // Debug message
                if (self->debug) {
                    char text[39];
                    sprintf(text,"Debug (%u, %u): Conductorship void!\n", self->node, __SELF.mode);
                    SCI_WRITE(self->sci, text);
                } else {
                    SCI_WRITE(self->sci, "Conductorship void!\n");
                }
            }
            
            // Stop request if leader_no increments
            if (__SELF.req == 1) {
                
                // Set self to musician
                __SELF.req = 0;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor, request ended!\n", self->node, __SELF.mode);
                    SCI_WRITE(self->sci, text);
                }
            }
            
            // Increment leader_no
            self->leader_no = msg->buff[5];
        }
        
        // Debug message
        if (self->debug) {
            char text[133];
            sprintf(text,"Debug (%d, %d): Update values recieved from (%d, %d) [tempo: %d, key: %d, vol: %d, index: %d, playing: %d, leader_no: %d]\n", self->node, __SELF.mode, msg->nodeId, __NODE(msg->nodeId).mode, msg->buff[0], msg->buff[1], msg->buff[2], msg->buff[3], msg->buff[4], msg->buff[5]);
            SCI_WRITE(self->sci, text);
        }
        
        // Update values if musician and message is from conductor
        if ((__SELF.mode == __MELGEN_MUSICIAN) && (__NODE(msg->nodeId).mode == __MELGEN_CONDUCTOR) && (msg->buff[5] == self->leader_no)) {
            
            // Update values
            self->tempo = msg->buff[0];
            self->key = msg->buff[1];
            self->tone->vol = msg->buff[2];
            self->index = msg->buff[3];
            self->playing = msg->buff[4];
        }
    }
}

void melgen_play(MelGen *self, int unused) {
    
    // Only start playing if not playing already
    if (self->playing == 0 && __SELF.mode == __MELGEN_CONDUCTOR) {
        
        // Update playing
        self->playing = 1;
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    
        // Schedule first conductor task
        ASYNC(self, __melgen_conductor, 0);
    }
}

void melgen_stop(MelGen *self, int unused) {
    if (__SELF.mode == __MELGEN_CONDUCTOR) {
        self->playing = 0;
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
}

void melgen_reset(MelGen *self, int unused) {
    if (__SELF.mode == __MELGEN_CONDUCTOR) {
        self->index = 0;
        self->tempo = __MELGEN_START_TEMPO; 
        self->key = __MELGEN_START_KEY;
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
}

void melgen_set_key(MelGen *self, int key) {
    if (__SELF.mode == __MELGEN_CONDUCTOR) {
        // 
        if (key < __MELGEN_MIN_KEY) {
            key = __MELGEN_MIN_KEY;
        } 
        else if (key > __MELGEN_MAX_KEY) {
            key = __MELGEN_MAX_KEY;
        }
        self->key = key;
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
}

void melgen_set_tempo(MelGen *self, int tempo) {
    if (__SELF.mode == __MELGEN_CONDUCTOR) {
        if (tempo < __MELGEN_MIN_TEMPO) {
            tempo = __MELGEN_MIN_TEMPO;
        }
        else if (tempo > __MELGEN_MAX_TEMPO) {
            tempo = __MELGEN_MAX_TEMPO;
        }
        self->tempo = tempo;
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
}

void melgen_set_mute(MelGen *self, int mute) {
    SYNC(self->tone, tonegen_set_mute, mute);
}

void melgen_vol_incr(MelGen *self, int unused) {
    if (__SELF.mode == __MELGEN_CONDUCTOR) {
        TONE_VOL_INCR(self->tone);
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
}

void melgen_vol_decr(MelGen *self, int unused) {
    if (__SELF.mode == __MELGEN_CONDUCTOR) {
        TONE_VOL_DECR(self->tone);
        
        // Send update message
        CANMsg msg = __MELGEN_CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key, self->tone->vol, self->index, self->playing, self->leader_no);
        if (!self->force_failiure) {
            CAN_SEND(self->can, &msg);
        }
    }
}

void meglen_req_cond(MelGen *self, int unused) {
    
    // Only req conductor if not already conductor and not req
    if (__SELF.mode == __MELGEN_MUSICIAN && !__SELF.req) {
        
        // Update req
        __SELF.req = 1;
        
        // Debug message
        if (self->debug) {
            char text[50];
            sprintf(text, "Debug (%u, %u): Requesting conductorship!\n", self->node, __SELF.mode);
            SCI_WRITE(self->sci, text);
        }
        
        // Ensure no higher rank node is requesting conductorship
        for (int i = 0; i < 4; i++) {
            // Compare requests, stop request
            if (__NODE(i).req && __SELF.req && (i < self->node)) {
                
                // Erase own request
                __SELF.req = 0;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): Higher rank request from (%d, %d), ending request.\n", self->node, __SELF.mode, i, __NODE(i).mode);
                    SCI_WRITE(self->sci, text);
                }
            }
        }
    }
}

void meglen_force_cond(MelGen *self, int unused) {
    
    // Only force conductor if not already conductor
    if (__SELF.mode == __MELGEN_MUSICIAN) {
        
        // Update mode
        __SELF.mode = __MELGEN_CONDUCTOR;
        self->leader_no = self->leader_no + 1;
        
        // Debug message
        if (self->debug) {
            char text[50];
            sprintf(text, "Debug (%u, %u): Forcing conductorship!\n", self->node, __SELF.mode);
            SCI_WRITE(self->sci, text);
        }
        
        if (self->playing) {
            ASYNC(self, __melgen_conductor, 0);
        }
    }
}

void melgen_report_tempo_toggle(MelGen *self, int unused) {
    if (self->print_tempo == 1) {
        self->print_tempo = 0;
    }
    else {
        self->print_tempo = 1;
        ASYNC(self, __melgen_report_tempo, 0);
    }
}

void melgen_report_muted_toggle(MelGen *self, int unused) {
    if (self->print_muted == 1) {
        self->print_muted = 0;
    }
    else {
        self->print_muted = 1;
        ASYNC(self, __melgen_report_muted, 0);
    }
}

void __melgen_report_tempo(MelGen *self, int unused) {
    if (self->print_tempo == 0) {
        return;
    }
    
    // Report current tempo
    char text[20];
    sprintf(text,"Current tempo: %d\n", self->tempo);
    SCI_WRITE(self->sci, text);
    
    AFTER(__MELGEN_PRINT_TEMPO_PERIOD, self, __melgen_report_tempo, 0);
}

void __melgen_report_muted(MelGen *self, int unused) {
    if (self->print_muted == 0) {
        return;
    }
    
    // Report current tempo
    if (self->tone->mute) {
        SCI_WRITE(self->sci, "Currently muted.\n");
    }
    else {
        SCI_WRITE(self->sci, "Currently not muted.\n");
    }
    
    AFTER(__MELGEN_PRINT_TEMPO_PERIOD, self, __melgen_report_muted, 0);
}

void melgen_f1_toggle(MelGen *self, int unused) {
    if (self->force_failiure == 1) {
        self->force_failiure = 0;
    }
    else {
        self->force_failiure = 1;
    }
}

void melgen_f2(MelGen *self, int unused) {
    self->force_failiure = 1;
    int fail_time = 5 + (self->time % 5);
    
    char text[20];
    sprintf(text, "Fail time: %d!\n", fail_time);
    SCI_WRITE(self->sci, text);

    AFTER(MSEC(1000 * fail_time), self, __melgen_f2_end, 0);
}

void __melgen_f2_end(MelGen *self, int unused) {
    self->force_failiure = 0;
}

void melgen_report(MelGen *self, int unused) {
    char text[50];
        
    if (self->debug) {
        sprintf(text,"Debug Report (node: %u, mode: %u):\n", self->node, __SELF.mode);
        SCI_WRITE(self->sci, text);
        
        // General information
        sprintf(text,"Heartbeat time: %d\n", self->time);
        SCI_WRITE(self->sci, text);
        
        // Requesting conductorship?
        if (__SELF.req) {
            sprintf(text,"Requested conductorship with time: %d\n", __SELF.mode);
            SCI_WRITE(self->sci, text);
        } else {
            sprintf(text,"Not requesting conductorship.\n");
            SCI_WRITE(self->sci, text);
        }
        
        // Leadership no.
        sprintf(text,"Leader number: %d\n", self->leader_no);
        SCI_WRITE(self->sci, text);
    }

    else {
        SCI_WRITE(self->sci, "Membership report\n");
    }
    
    // Current units:
    for (int i = 0; i < 4; i++) {
        if (__NODE(i).timeout == 0) {
            sprintf(text," - Node (%d): ", i);
            SCI_WRITE(self->sci, text);
            
            if (__NODE(i).mode == __MELGEN_CONDUCTOR) {
                sprintf(text,"Conductor, ");
                SCI_WRITE(self->sci, text);
            } else {
                sprintf(text,"Musician, ");
                SCI_WRITE(self->sci, text);
            }
            
            if (__NODE(i).req) {
                sprintf(text,"requesting, ");
                SCI_WRITE(self->sci, text);
            } else {
                sprintf(text,"not requesting, ");
                SCI_WRITE(self->sci, text);
            }
            
            sprintf(text,"rank: %d, ", self->units[i].active_rank);
            SCI_WRITE(self->sci, text);
            
            sprintf(text,"timestamp: %d ", self->units[i].timestamp);
            SCI_WRITE(self->sci, text);
            
            if (self->node == i) {
                sprintf(text,"(This is me!)\n");
                SCI_WRITE(self->sci, text);
            } else {
                sprintf(text,"\n");
                SCI_WRITE(self->sci, text);
            }
        } else {
            sprintf(text," - Node (%d): Timed out!\n", i);
            SCI_WRITE(self->sci, text);
        }
    }
    
    if (self->debug) {
        if (self->playing) {
                sprintf(text,"Currently playing.\n");
                SCI_WRITE(self->sci, text);
        } else {
                sprintf(text,"Currently not playing.\n");
                SCI_WRITE(self->sci, text);
        }
        sprintf(text,"Tempo: %d, Key: %d\n", self->tempo, self->key+__MELGEN_OFFSET_KEY);
        SCI_WRITE(self->sci, text);
        sprintf(text,"Note index: %d, Voume: %d\n", self->index, self->tone->vol);
        SCI_WRITE(self->sci, text);
    }
}

void melgen_debug_toggle(MelGen *self, int unused) {
    if (self->debug == 1) {
        self->debug = 0;
    }
    else {
        self->debug = 1;
    }
}