#include "TinyTimber.h"
#include "tonegen.h"
#include "can_melgen.h"
#include <stdlib.h>
#include <stdio.h>

const int BROTHER_JOHN_TONE[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
const int BROTHER_JOHN_HALFBEATS[32] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 4, 2, 2, 4};
const int BROTHER_JOHN_LENGTH = 32;

const int PERIOD[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

void __melgen_schedule(MelGen *self, int unused) {
    
    // Stop if playing turned off or musician
    if (self->playing == 0 || self->mode == __MELGEN_MUSICIAN) {
        //BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);
        self->playing = 0;
        return;
    }
    
    unsigned char player = self->index % self->active_units;
    
    // If conductor is supposed to play, schedule tone
    if (player == self->active_rank) {
        
        // Debug message
        if (self->debug) {
            char text[64];
            sprintf(text,"Debug (%d, %d): Playing note %d self [%d, %d]\n", self->node, self->mode, self->index, self->tempo, self->key);
            SCI_WRITE(self->sci, text);
        }
        
        SYNC(self->tone, tonegen_period_set, PERIOD[BROTHER_JOHN_TONE[self->index]+self->key+10]);
        
        // Start tone now deadline
        BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_play, 0);
        
        // Stop tone after beat-offset
        SEND(USEC(self->halfbeat*BROTHER_JOHN_HALFBEATS[self->index]-__MELGEN_OFFSET), USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);
        
        CANMsg msg = __CANMSG_HEART_BEAT(self->node, self->mode, self->leader_no, self->req_con, 0);
        CAN_SEND(self->can, &msg);
    }
    
    // If other musician is supposed to play, send CAN message to correct unit based on rank
    else {
        unsigned char nodeid = 0;
        for (int i = 0; i < 4; i++) {
            if (player == self->units[i].active_rank) {
                nodeid = i;
            }
        }
        
        // Debug message
        if (self->debug) {
            char text[64];
            sprintf(text,"Debug (%d, %d): Sending note %d to (%d, %d) [%d, %d]\n", self->node, self->mode, self->index, nodeid, self->units[nodeid].mode, self->tempo, self->key);
            SCI_WRITE(self->sci, text);
        }
        
        CANMsg msg = __CANMSG_NOTE_PLAY(self->node, nodeid, self->index, self->tempo, self->key, self->tone->vol, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
    
    // Next index
    if (self->index >= 32-1) {
        self->index = 0;
    } else {
        self->index = self->index + 1;
    }
    
    // Start next scheduling after beat
    SEND(USEC(self->halfbeat*BROTHER_JOHN_HALFBEATS[self->index]), USEC(__MELGEN_DEADLINE), self, __melgen_schedule, 0);
}

void __melgen_heart_beat(MelGen *self, int unused) {
    
    // Increment hb counter
    self->hb_time = self->hb_time + 1;
    self->units[self->node].timestamp = self->hb_time;
    
    // Handle timeouts, ranks and active units
    unsigned char active_units = 0;
    //unsigned char req_needed = 1; 
    for (int i = 0; i < 4; i++) {
        if ((self->node != i) && (self->units[i].timestamp + __HEART_BEATS_TIMEOUT < self->hb_time)) {
            // Debug
            if (self->debug && (self->units[i].timeout == 0)) {
                char text[81];
                sprintf(text,"Debug (%u, %u): Timeout on node %d!\n", self->node, self->mode, i);
                SCI_WRITE(self->sci, text);
            }
            
            self->units[i].timeout = 1;
        }
        else {
            // Ensure timeout is set to 0
            self->units[i].timeout = 0;
            
            // Set active rank
            self->units[i].active_rank = active_units;
            if (i == self->node) {
                self->active_rank = active_units;
            }
            
            // Handle active units
            active_units++;
            
            // Check if conductor request needed
            if ((self->mode == __MELGEN_CONDUCTOR) || (self->req_con == 1) || (self->units[i].mode == __MELGEN_CONDUCTOR) || (self->units[i].req_con == 1)) {
                //req_needed = 0;
            }
        }
    }
    self->active_units = active_units;
    
    // If no conductor requesting or conductor, automatically request conductor
    if (0) { //if (req_needed) {
        if (self->debug) {
            char text[81];
            sprintf(text,"Debug (%u, %u): No conductor or request detected, requesting conductorship...\n", self->node, self->mode);
            SCI_WRITE(self->sci, text);
        }
        
        self->req_con = 1;
        self->req_con_timestamp = self->hb_time;
    }
    
    // If requesting conductor and have waited
    if (self->req_con && (self->req_con_timestamp + __REQ_CON_WAIT_TIME < self->hb_time)) {
        self->mode = __MELGEN_CONDUCTOR;
        self->units[self->node].mode = __MELGEN_CONDUCTOR;
        self->leader_no = self->leader_no + 1;
        self->req_con = 0;
        self->req_con_timestamp = 0;
        
        ASYNC(self, melgen_play, 0);
    }
    
    /*
    // Debug message
    if (self->debug) {
        char text[50];
        sprintf(text,"Debug (%u, %u): Heartbeat [%u, %u]\n", self->node, self->mode, self->my_no, self->req_con);
        SCI_WRITE(self->sci, text);
    }
    */
    
    //char str[28] = "Wasdasdasd";
    //sprintf(str,"Heartbeat (%d, %d, %d): ", self->mode, self->my_no, self->req_con);
    //ASYNC(self, __melgen_debug, &str);
    
    // Send CAN message
    CANMsg msg = __CANMSG_HEART_BEAT(self->node, self->mode, self->leader_no, self->req_con, 0);
    CAN_SEND(self->can, &msg);
    
    // Schedule next heart beat
    SEND(__HEART_BEAT_PERIOD, __HEART_BEAT_DEADLINE, self, __melgen_heart_beat, 0);
}

void melgen_can_receive(MelGen *self, CANMsg *msg) {
    
    // Handle heart beat
    if (msg->msgId == __MSGID_HEART_BEAT) {
        
        // Update unit in units array
        self->units[msg->nodeId].mode = msg->buff[0];
        self->units[msg->nodeId].timestamp = self->hb_time;
        self->units[msg->nodeId].timeout = 0;
        
        // Update current highest leader no
        if (msg->buff[1] > self->leader_no) {
            
            // Give up conductorship
            if (self->mode == __MELGEN_CONDUCTOR) {
                self->mode = __MELGEN_MUSICIAN;
                self->units[self->node].mode = __MELGEN_MUSICIAN;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor detected, conductorship void.\n", self->node, self->mode);
                    SCI_WRITE(self->sci, text);
                }
            }
            self->leader_no = msg->buff[1];
        }
        
        // Stop requesting conductor if other higher rank musician is
        if ((msg->buff[2] == 1) && self->req_con && (msg->nodeId < self->node)) {
            self->req_con = 0;
            
            
            // Debug message
            if (self->debug) {
                char text[81];
                sprintf(text,"Debug (%u, %u): Higher rank request from (%d, %d), ending request.\n", self->node, self->mode, msg->nodeId, self->units[msg->nodeId].mode);
                SCI_WRITE(self->sci, text);
            }
        }
        
        // Handle timeouts, ranks and active units
        unsigned char active_units = 0;
        for (int i = 0; i < 4; i++) {
            if (self->units[i].timeout != 1) {
                
                // Set active rank
                self->units[i].active_rank = active_units;
                if (i == self->node) {
                    self->active_rank = active_units;
                }
                
                // Handle active units
                active_units++;
            }
        }
        self->active_units = active_units;
        
        // Debug message
        /*
        if (self->debug) {
            char text[64];
            sprintf(text,"Debug (%u, %u): Heartbeat from (%u, %u) recieved [%u, %u]\n", self->node, self->mode, msg->nodeId, msg->buff[0], msg->buff[1], msg->buff[2]);
            SCI_WRITE(self->sci, text);
        }
        */
    } 
    
    // Handle play note
    else if (msg->msgId == __MSGID_PLAY_NOTE) {
        
        // Update current highest leader no
        if (msg->buff[5] > self->leader_no) {
            
            // Give up conductorship
            if (self->mode == __MELGEN_CONDUCTOR) {
                self->mode = __MELGEN_MUSICIAN;
                self->units[self->node].mode = __MELGEN_MUSICIAN;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor detected, conductorship void.\n", self->node, self->mode);
                    SCI_WRITE(self->sci, text);
                }
            }
            self->leader_no = msg->buff[5];
        }
        
        // Debug message
        if (self->debug) {
            char text[85];
            sprintf(text,"Debug (%d, %d): Play note from (%d, %d) recieved [%d, %d, %d, %d, %d, %d]\n", self->node, self->mode, msg->nodeId, self->units[msg->nodeId].mode, msg->buff[0], msg->buff[1], msg->buff[2], msg->buff[3], msg->buff[4], msg->buff[5]);
            SCI_WRITE(self->sci, text);
        }
        
        // Play note if musician and message is from conductor
        if ((msg->buff[0] == self->node) && (self->mode == __MELGEN_MUSICIAN) && (msg->buff[5] == self->leader_no)) {
            
            // Update values
            self->index = msg->buff[1];
            self->halfbeat = 30000000/msg->buff[2];
            self->tempo = msg->buff[2]-5;
            self->key = msg->buff[3];
            self->tone->vol = msg->buff[4];
            
            // Debug message
            if (self->debug) {
                char text[64];
                sprintf(text,"Debug (%d, %d): Playing note %d [%d, %d]\n", self->node, self->mode, self->index, self->tempo, self->key);
                SCI_WRITE(self->sci, text);
            }
        
            SYNC(self->tone, tonegen_period_set, PERIOD[BROTHER_JOHN_TONE[self->index]+self->key+10]);

            // Start tone now deadline
            BEFORE(USEC(__TONEGEN_DEADLINE), self->tone, tonegen_play, 0);

            // Stop tone after beat-offset
            SEND(USEC(self->halfbeat*BROTHER_JOHN_HALFBEATS[self->index]-__MELGEN_OFFSET), USEC(__TONEGEN_DEADLINE), self->tone, tonegen_stop, 0);

            // Start next scheduling after beat
            SEND(USEC(self->halfbeat*BROTHER_JOHN_HALFBEATS[self->index]), USEC(__MELGEN_DEADLINE), self, __melgen_schedule, 0);
        }
    }
    
    // Handle update values
    else if (msg->msgId == __MSGID_UPDATE_VALUES) {
        
        // Update current highest leader no
        if (msg->buff[1] > self->leader_no) {
            
            // Give up conductorship
            if (self->mode == __MELGEN_CONDUCTOR) {
                self->mode = __MELGEN_MUSICIAN;
                self->units[self->node].mode = __MELGEN_MUSICIAN;
                
                // Debug message
                if (self->debug) {
                    char text[81];
                    sprintf(text,"Debug (%u, %u): New conductor detected, conductorship void.\n", self->node, self->mode);
                    SCI_WRITE(self->sci, text);
                }
            }
            self->leader_no = msg->buff[1];
        }
        
        // Update values if musician and message is from conductor
        if ((msg->buff[0] == self->node) && (self->mode == __MELGEN_MUSICIAN) && (msg->buff[5] == self->leader_no)) {
            // Update values
            self->halfbeat = 30000000/msg->buff[0];
            self->tempo = msg->buff[0];
            self->key = msg->buff[1];
            self->tone->vol = msg->buff[2];
            self->index = msg->buff[3];
            self->playing = msg->buff[4];
        }
        
        // Debug message
        if (self->debug) {
            char text[89];
            sprintf(text,"Debug (%d, %d): Update values recieved from (%d, %d) [%d, %d, %d, %d, %d, %d]\n", self->node, self->mode, msg->nodeId, self->units[msg->nodeId].mode, msg->buff[0], msg->buff[1], msg->buff[2], msg->buff[3], msg->buff[4], msg->buff[5]);
            SCI_WRITE(self->sci, text);
        }
    }
}

void melgen_play(MelGen *self, int unused) {
    
    // Only start playing if not playing already
    if (self->playing == 0 && self->mode == __MELGEN_CONDUCTOR) {
        self->playing = 1;    
        
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    
        // Schedule first scheduling
        ASYNC(self, __melgen_schedule, 0);
    }
}

void melgen_stop(MelGen *self, int unused) {
    self->playing = 0;
    if (self->mode == __MELGEN_CONDUCTOR) {
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
}

void melgen_reset(MelGen *self, int unused) {
    if (self->mode == __MELGEN_CONDUCTOR) {
        self->index = 0;
        self->halfbeat = __MELGEN_START_HALFBEAT;
        self->tempo = __MELGEN_START_TEMPO; 
        self->key = __MELGEN_START_KEY;
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
}

void melgen_set_key(MelGen *self, int key) {
    if (self->mode == __MELGEN_CONDUCTOR) {
        if (key >= -5 && key <= 5) self->key = key;
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
}

void melgen_set_tempo(MelGen *self, int tempo) {
    if (self->mode == __MELGEN_CONDUCTOR) {
        // Convert bpm into halfbeats
        if (tempo >= 60 && tempo <= 240) { 
            self->halfbeat = 30000000/tempo; 
            self->tempo = tempo; 
            CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
            CAN_SEND(self->can, &msg);
        }
    }
}

void melgen_set_mute(MelGen *self, int mute) {
    if (self->mode == __MELGEN_CONDUCTOR) {
        SYNC(self->tone, tonegen_set_mute, mute);
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
}

void melgen_vol_incr(MelGen *self, int unused) {
    if (self->mode == __MELGEN_CONDUCTOR) {
        TONE_VOL_INCR(self->tone);
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
}

void melgen_vol_decr(MelGen *self, int unused) {
    if (self->mode == __MELGEN_CONDUCTOR) {
        TONE_VOL_DECR(self->tone);
        CANMsg msg = __CANMSG_UPDATE_VALUES(self->node, self->tempo, self->key+5, self->tone->vol, self->index, self->playing, self->leader_no);
        CAN_SEND(self->can, &msg);
    }
}

void __meglen_force_cond(MelGen *self, int unused) {
    if (self->mode == __MELGEN_MUSICIAN) {
        self->mode = __MELGEN_CONDUCTOR;
        self->
        self->leader_no = self->leader_no + 1;
        
        if (self->playing) {
            ASYNC(self, melgen_play, 0);
        }
        
        // Debug message
        if (self->debug) {
            char text[50];
            sprintf(text,"Debug (%u, %u): Forcing conductorship!\n", self->node, self->mode);
            SCI_WRITE(self->sci, text);
        }
    }
}

void __melgen_debug_report(MelGen *self, int unused) {
    
    // Debug message
    if (self->debug) {
        char text[50];
        
        sprintf(text,"Debug Report (%u, %u):\n", self->node, self->mode);
        SCI_WRITE(self->sci, text);
        
        // General information
        sprintf(text,"Node: %d Mode: %d Heartbeat time: %d\n", self->node, self->mode, self->hb_time);
        SCI_WRITE(self->sci, text);
        
        // Requesting conductorship?
        if (self->req_con) {
            sprintf(text,"Requested conductorship with time: %d\n", self->mode);
            SCI_WRITE(self->sci, text);
        } else {
            sprintf(text,"Not requesting conductorship.\n");
            SCI_WRITE(self->sci, text);
        }
        
        // Leadership no.
        sprintf(text,"Leader no: %d\n", self->leader_no);
        SCI_WRITE(self->sci, text);
        
        // Current units:
        for (int i = 0; i < 4; i++) {
            if (self->units[i].timeout == 0) {
                sprintf(text," - Node %d: ", i);
                SCI_WRITE(self->sci, text);
                
                if (self->units[i].mode == __MELGEN_CONDUCTOR) {
                    sprintf(text,"Conductor, ");
                    SCI_WRITE(self->sci, text);
                } else {
                    sprintf(text,"Musician, ");
                    SCI_WRITE(self->sci, text);
                }
                
                if (self->units[i].req_con) {
                    sprintf(text,"requesting, ");
                    SCI_WRITE(self->sci, text);
                } else {
                    sprintf(text,"not requesting, ");
                    SCI_WRITE(self->sci, text);
                }
                
                sprintf(text,"rank: %d, ", self->units[i].active_rank);
                SCI_WRITE(self->sci, text);
                
                sprintf(text,"timestamp: %d\n", self->units[i].timestamp);
                SCI_WRITE(self->sci, text);
                
            } else {
                sprintf(text," - Node %d: Timed out!\n", i);
                SCI_WRITE(self->sci, text);
            }
        }
    }
}