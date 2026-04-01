#ifndef BIQUADS_COEFFS_H
#define BIQUADS_COEFFS_H

//--- High-Pass: DC Block (Pole 2) ---
// Cutoff : 3.39 Hz;
const float DC_B0 = 0.999778417900206f;
const float DC_B1 = -0.999778417900206f;
const float DC_A1 = -0.9995568358004122f;

//--- High-Pass: Tone Shape (Pole 1) ---
// Cutoff : 720.48 Hz;
const float TONE_B0 = 0.9549360344717014f;
const float TONE_B1 = -0.9549360344717014f;
const float TONE_A1 = -0.9098720689434028f;

//--- Low-Pass: Anti-Aliasing ---
// Cutoff : 2800.00 Hz;
const float AA_B0 = 0.15635952069919337f;
const float AA_B1 = 0.15635952069919337f;
const float AA_A1 = -0.6872809586016132f;

#endif
