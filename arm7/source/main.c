#include <nds.h>

#define FIFO_BATT FIFO_USER_01

static u16 readTSC(u8 command) {
    u16 res1, res2;
    int oldIME = enterCriticalSection();

    while (REG_SPICNT & SPI_BUSY);
    REG_SPICNT = SPI_ENABLE | SPI_BAUD_2MHz | SPI_DEVICE_TOUCH | SPI_CONTINUOUS;
    REG_SPIDATA = command;
    while (REG_SPICNT & SPI_BUSY);

    REG_SPIDATA = 0;
    while (REG_SPICNT & SPI_BUSY);
    res1 = REG_SPIDATA;

    REG_SPICNT = SPI_ENABLE | SPI_BAUD_2MHz | SPI_DEVICE_TOUCH;
    REG_SPIDATA = 0;
    while (REG_SPICNT & SPI_BUSY);
    res2 = REG_SPIDATA;

    leaveCriticalSection(oldIME);
    return ((res1 & 0x7F) << 5) | (res2 >> 3);
}

static void battHandler(u32 value, void *userdata) {
    (void)value; (void)userdata;
    for (int ch = 0; ch < 8; ch++) {
        u8 base = 0x80 | (ch << 4);
        readTSC(base | 0x04);
        u16 se = readTSC(base | 0x04);   // single-ended
        readTSC(base);
        u16 df = readTSC(base);          // differential
        fifoSendValue32(FIFO_BATT, ((u32)ch << 24) | se);
        fifoSendValue32(FIFO_BATT, ((u32)(8 + ch) << 24) | df);
    }
    for (int r = 0; r < 5; r++) {
        fifoSendValue32(FIFO_BATT, ((u32)(16 + r) << 24) | readPowerManagement(r));
    }
}

void VcountHandler(void) { inputGetAndSend(); }
void VblankHandler(void) { }

int main(void) {
    readUserSettings();
    ledBlink(0);

    irqInit();
    fifoInit();
    touchInit();
    SetYtrigger(80);

    installSoundFIFO();
    installSystemFIFO();

    irqSet(IRQ_VCOUNT, VcountHandler);
    irqSet(IRQ_VBLANK, VblankHandler);
    irqEnable(IRQ_VBLANK | IRQ_VCOUNT);

    fifoSetValue32Handler(FIFO_BATT, battHandler, 0);

    while (1) swiWaitForVBlank();
    return 0;
}
