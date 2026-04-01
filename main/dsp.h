#ifndef DSP_H
#define DSP_H

#include <stdint.h>
#include <math.h>

// Core physical constants for MXR diode clipper
// Critical: The 'f' suffix forces float32 instead of double
#define IS_F 2.52e-9f
#define VT_F 0.02585f
#define R_F 2200.0f

// Algorithm safety constraints
#define VOUT_VT_MAX 20.0f  // Clamps sinhf() input to prevent overflow
#define OPAMP_RAIL 9.0f    // Hard clip after the gain stage
#define NR_MAX_ITER 8      // Budget limits us to 8 iterations max
#define NR_EPSILON 1e-6f   // Float32 appropriate convergence threshold

// Audio block settings
#define BLOCK_SIZE 256
#define SAMPLE_RATE 48000

// Shared knob variables (written by control task, read by audio task)
extern volatile float g_dist_gain;
extern volatile float g_vol_level;

// Filter state memory
typedef struct {
    float w1;
    float w2;
} BiquadState;

extern BiquadState state_lp_aa;
extern BiquadState state_hp_dc;
extern BiquadState state_hp_tone;

// Function prototypes
void init_dsp(void);
void process_block(int16_t *input, int16_t *output, uint32_t num_samples);

#endif // DSP_H
