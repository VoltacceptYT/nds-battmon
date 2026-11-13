// BattMon - ARM7 side. Reads the touchscreen controller's VBAT channel.
// Based on the libnds default ARM7 template (arm7 + arm9 combined project).
#include <nds.h>

#define FIFO_BATT FIFO_USER_01

// TSC2046-style control byte: start | channel 2 (VBAT) | 12-bit | single-ended | PD=00
#define TSC_MEASURE_VBAT 0xA4

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
    return ((res1 & 0x7F) << 5) | (res2 >> 3);   // 12-bit result
}

// ARM9 asks; we reply with raw ADC in low 16 bits, PMIC "battery low" flag in bit 16.
static void battHandler(u32 value, void *userdata) {
    (void)value; (void)userdata;
    readTSC(TSC_MEASURE_VBAT);                 // throwaway: let the ADC settle
    u16 raw = readTSC(TSC_MEASURE_VBAT);
    u32 low = readPowerManagement(1) & 1;      // PM reg 1 bit 0 = low battery
    fifoSendValue32(FIFO_BATT, raw | (low << 16));
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
