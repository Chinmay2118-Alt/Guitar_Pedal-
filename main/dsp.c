#include "dsp.h"
#include "biquads_coeffs.h"
#include "esp_attr.h"
#include <math.h>

volatile float g_dist_gain = 1.0f;
volatile float g_vol_level = 0.8f;

BiquadState state_lp_aa = {0};
BiquadState state_hp_dc = {0};
BiquadState state_hp_tone = {0};

void init_dsp(void) {
    state_lp_aa = (BiquadState){0.0f, 0.0f};
    state_hp_dc = (BiquadState){0.0f, 0.0f};
    state_hp_tone = (BiquadState){0.0f, 0.0f};
}

// ── Biquad Filter (IRAM for speed) ─────────────────────────
static inline float IRAM_ATTR apply_biquad(float x, BiquadState *s, float b0, float b1, float a1) {
    float w0 = x - a1 * s->w1;
    float y = b0 * w0 + b1 * s->w1;
    s->w1 = w0;
    return y;
}

// ── Newton-Raphson Clipper (IRAM for speed) ─────────────────

// Inside dsp.c - Replace the apply_clipper function

static float IRAM_ATTR apply_clipper(float vin) {
    static float prev_Vout = 0.0f;
    
    // Physical Constants for Asymmetry
    const float IS_POS = 2.52e-9f;  // Silicon-like
    const float IS_NEG = 5.0e-7f;   // Germanium-like (Leaks more, clips softer)
    
    vin = fminf(fmaxf(vin, -OPAMP_RAIL), OPAMP_RAIL);
    float vout = prev_Vout; 
    
    for (int i = 0; i < 6; i++) { // Reduced iterations for real-time stability
        float x_safe = vout / VT_F;
        x_safe = fminf(fmaxf(x_safe, -VOUT_VT_MAX), VOUT_VT_MAX);
        
        // Asymmetrical Diode Equation:
        // Instead of 2 * sinh(x), we calculate forward and reverse biased separately
        float exp_p = expf(x_safe);
        float exp_n = expf(-x_safe);
        
        float I_D = IS_POS * (exp_p - 1.0f) - IS_NEG * (exp_n - 1.0f);
        float f = (vin - vout) / R_F - I_D;
        
        // Derivative for Newton-Raphson
        float df = -1.0f / R_F - (IS_POS / VT_F) * exp_p - (IS_NEG / VT_F) * exp_n;
        
        if (fabsf(df) < 1e-15f) df = -1e-15f;
        
        float step = f / df;
        step = fminf(fmaxf(step, -0.4f), 0.4f); // More conservative stepping
        
        vout -= step;
        if (fabsf(step) < NR_EPSILON) break;
    }
    
    if (!isfinite(vout)) {
        vout = 0.0f;
        prev_Vout = 0.0f;
    } else {
        prev_Vout = vout;
    }
    
    return vout;
}

// ── Main Audio Pipeline ──────────────────────────────────────
void process_block(int16_t *input, int16_t *output, uint32_t num_samples) {
    // Read volatiles into local variables once per block
    float local_gain = g_dist_gain;
    float local_vol = g_vol_level;
    
    for (uint32_t i = 0; i < num_samples; i++) {
        // 1. Convert int16 (from UART) to float32 and sanitize
        float x = input[i] * (1.0f / 32768.0f);
        x = fminf(fmaxf(x, -1.0f), 1.0f);
        if (!isfinite(x)) x = 0.0f;
        
        // 2. Anti-aliasing LPF
        x = apply_biquad(x, &state_lp_aa, AA_B0, AA_B1, AA_A1);
        
        // 3. DC Block and Tone Shape filters
        x = apply_biquad(x, &state_hp_dc, DC_B0, DC_B1, DC_A1);
        x = apply_biquad(x, &state_hp_tone, TONE_B0, TONE_B1, TONE_A1);
        
        // 4. Gain Stage
        x *= local_gain;
        
        // 5. Diode Clipper
        x = apply_clipper(x);
        // x = fminf(fmaxf(x,-1.0f),1.0f);

        // 6. Volume
        x *= local_vol;
        
        // 7. Output Sanity Check
        if (!isfinite(x)) x = 0.0f;
        
        // 8. Convert back to int16 for UART output
        output[i] = (int16_t)fminf(fmaxf(x * 32767.0f, -32768.0f), 32767.0f);
    }
}
