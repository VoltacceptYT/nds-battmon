// BattMon - ARM9 side. Shows estimated battery % using:
//   Piecewise curve through 4.20/3.85/3.65/3.30 V benchmarks (linear formula shown too)
#include <nds.h>
#include <stdio.h>

#define FIFO_BATT FIFO_USER_01

// ---- Calibration: tweak these after comparing with a multimeter/known charge ----
#define ADC_VREF      2.5f   // TSC internal reference (V)
#define VBAT_DIVIDER  4.0f   // TSC2046 measures VBAT / 4
#define V_EMPTY       3.3f
#define V_FULL        4.2f
#define SAMPLES       16     // moving average length

static volatile u32 reply = 0;
static volatile bool gotReply = false;

static void onReply(u32 value, void *userdata) {
    (void)userdata;
    reply = value;
    gotReply = true;
}

// Piecewise-linear curve through your benchmark points (highest voltage first).
// Add more points here (e.g. from your own discharge test) to refine it.
typedef struct { float v; float pct; } CurvePoint;
static const CurvePoint curve[] = {
    {4.20f, 100.0f},
    {3.85f,  60.0f},
    {3.65f,  30.0f},
    {3.30f,   0.0f},
};
#define CURVE_N (int)(sizeof(curve) / sizeof(curve[0]))

static float percentFromVoltage(float v) {
    if (v >= curve[0].v) return 100.0f;
    if (v <= curve[CURVE_N - 1].v) return 0.0f;
    for (int i = 0; i < CURVE_N - 1; i++) {
        if (v <= curve[i].v && v >= curve[i + 1].v) {
            float t = (v - curve[i + 1].v) / (curve[i].v - curve[i + 1].v);
            return curve[i + 1].pct + t * (curve[i].pct - curve[i + 1].pct);
        }
    }
    return 0.0f;
}

// Your original linear formula, shown alongside for comparison.
static float linearPercent(float v) {
    float p = (v - V_EMPTY) / (V_FULL - V_EMPTY) * 100.0f;
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

int main(void) {
    consoleDemoInit();
    fifoSetValue32Handler(FIFO_BATT, onReply, 0);

    float history[SAMPLES];
    int count = 0, idx = 0, frame = 0;
    int lowFlag = 0;

    while (1) {
        swiWaitForVBlank();

        if (frame++ % 30 == 0) {                 // ~2 reads per second
            gotReply = false;
            fifoSendValue32(FIFO_BATT, 0);
        }

        if (gotReply) {
            gotReply = false;
            u32 r = reply;
            u16 raw = r & 0xFFFF;
            lowFlag = (r >> 16) & 1;

            history[idx] = (raw / 4095.0f) * ADC_VREF * VBAT_DIVIDER;
            idx = (idx + 1) % SAMPLES;
            if (count < SAMPLES) count++;

            float sum = 0;
            for (int i = 0; i < count; i++) sum += history[i];
            float v = sum / count;
            float pct = percentFromVoltage(v);

            consoleClear();
            iprintf("\n  DS Lite Battery Monitor\n");
            iprintf("  -----------------------\n\n");
            iprintf("  Raw ADC : %u / 4095\n", raw);
            iprintf("  Voltage : %d.%02d V\n", (int)v, (int)(v * 100) % 100);
            iprintf("  Battery : ~%d %%\n", (int)(pct + 0.5f));
            iprintf("  (linear : %d %%)\n", (int)(linearPercent(v) + 0.5f));
            iprintf("  Low flag: %s\n\n", lowFlag ? "LOW (red LED)" : "ok");

            int bars = (int)(pct / 5);           // 20-char bar
            iprintf("  [");
            for (int i = 0; i < 20; i++) iprintf(i < bars ? "#" : "-");
            iprintf("]\n\n");
            iprintf("  Note: Li-ion is not linear;\n");
            iprintf("  3.7-3.8V holds a long time.\n");
        }

        scanKeys();
        if (keysDown() & KEY_START) break;
    }
    return 0;
}
