#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

#include "tonegen.h"
#include "bg_load.h"

const int BROTHER_JOHN[32] = {0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

const int PERIOD[25] = {2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

typedef struct {
    Object super;
    int count;
    char buf[20];
    int int_buf[3];
    int int_count;
} WCET_Test_App;

WCET_Test_App app = { initObject(), 0, "", {}, 0 };

void reader(WCET_Test_App*, int);
void receiver(WCET_Test_App*, int);

int array_max(int myArray [], int size);
int array_avg(int myArray [], int size);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

ToneGen tone = initToneGen;
BgLoad bgload = initBgLoad;

void receiver(WCET_Test_App *self, int unused) {
    //CANMsg msg;
    //CAN_RECEIVE(&can0, &msg);
    //SCI_WRITE(&sci0, "Can msg received: ");
    //SCI_WRITE(&sci0, msg.buff);
}

void reader(WCET_Test_App *self, int c) {
    //SCI_WRITE(&sci0, "Rcv: \'");
    //SCI_WRITECHAR(&sci0, c);
    //SCI_WRITE(&sci0, "\'\n");
}

void startApp(WCET_Test_App *self, int arg) {
    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    
    TONE_PERIOD_SET(&tone,500); // 500 us period - 1 kHz tone
    
    int samples[500] = {};
    Time start, end;
    int max, avg;
    
    // BgLoad WCST 1
    SCI_WRITE(&sci0, "BgLoad WCST 1 \n");
    SYNC(&bgload, bgload_rng_set, 2000);
    
    for (int i = 0; i < 500; i++) {
        start = CURRENT_OFFSET();
        SYNC(&bgload, __bgload_loop_sync_test, 0);
        end = CURRENT_OFFSET();
        samples[i] = USEC(start) - USEC(end);
    }
    max = array_max(samples, 500);
    avg = array_avg(samples, 500);
    
    SCI_WRITE(&sci0, "Maximum: \'");
    SCI_WRITECHAR(&sci0, max);
    SCI_WRITE(&sci0, "\', average: \'");
    SCI_WRITECHAR(&sci0, avg);
    SCI_WRITE(&sci0, "\'\n");
    
    // BgLoad WCST 2
    SCI_WRITE(&sci0, "BgLoad WCST 2 \n");
    SYNC(&bgload, bgload_rng_set, 1000);
    
    for (int i = 0; i < 500; i++) {
        start = CURRENT_OFFSET();
        SYNC(&bgload, __bgload_loop_sync_test, 0);
        end = CURRENT_OFFSET();
        samples[i] = USEC(start) - USEC(end);
    }
    max = array_max(samples, 500);
    avg = array_avg(samples, 500);
    
    SCI_WRITE(&sci0, "Maximum: \'");
    SCI_WRITECHAR(&sci0, max);
    SCI_WRITE(&sci0, "\', average: \'");
    SCI_WRITECHAR(&sci0, avg);
    SCI_WRITE(&sci0, "\'\n");
    
    // ToneGen WCST
    SCI_WRITE(&sci0, "ToneGen WCST \n");
    
    for (int i = 0; i < 500; i++) {
        start = CURRENT_OFFSET();
        SYNC(&tone, __tonegen_switch_sync_test, 0);
        end = CURRENT_OFFSET();
        samples[i] = USEC(start) - USEC(end);
    }
    max = array_max(samples, 500);
    avg = array_avg(samples, 500);
    
    SCI_WRITE(&sci0, "Maximum: \'");
    SCI_WRITECHAR(&sci0, max);
    SCI_WRITE(&sci0, "\', average: \'");
    SCI_WRITECHAR(&sci0, avg);
    SCI_WRITE(&sci0, "\'\n");
    
}

int array_max(int array[], int size) {
    int maxv = array[0];

    for (int i = 1; i < size; ++i) {
        if ( array[i] > maxv ) {
            maxv = array[i];
        }
    }
    return maxv;
}

int array_avg(int array[], int size) {
    int sum = array[0];

    for (int i = 1; i < size; ++i) {
        sum += array[i];
    }
    return sum/size;
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
