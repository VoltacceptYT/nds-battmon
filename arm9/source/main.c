#include <nds.h>
#include <stdio.h>

#define FIFO_BATT FIFO_USER_01

static volatile u16 vals[24];

static void onReply(u32 v, void *userdata) {
    (void)userdata;
    u32 tag = v >> 24;
    if (tag < 24) vals[tag] = v & 0xFFFF;
}

int main(void) {
    consoleDemoInit();
    fifoSetValue32Handler(FIFO_BATT, onReply, 0);

    int frame = 0;
    while (1) {
        swiWaitForVBlank();
        frame++;
        if (frame % 30 == 0) fifoSendValue32(FIFO_BATT, 0);

        if (frame % 30 == 10) {
            consoleClear();
            iprintf("TSC raw  SE=single DF=diff\n\n");
            for (int c = 0; c < 8; c++)
                iprintf("ch%d SE:%4d DF:%4d\n", c, vals[c], vals[8 + c]);
            iprintf("\nPM regs 0-4 (hex):\n %02X %02X %02X %02X %02X\n",
                    vals[16], vals[17], vals[18], vals[19], vals[20]);
        }
    }
    return 0;
}
