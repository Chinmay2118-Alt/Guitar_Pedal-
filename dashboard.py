import streamlit as st
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

# --- Page Configuration ---
st.set_page_config(page_title="ESP32 DSP HIL Pipeline", layout="wide")
st.title("🎛️ ESP32 Digital Signal Processing (HIL Validation)")
st.markdown("### MXR-Style Asymmetrical Diode Clipping Emulator")
st.divider()

# --- File Paths ---
INPUT_WAV = "Clean_guitar.wav" # Ensure this matches your actual clean file name
OUTPUT_WAV = "Clean_guitar_esp32_processed.wav"

# --- Audio Playback Section ---
col1, col2 = st.columns(2)

with col1:
    st.subheader("1. Raw Input Signal (Direct Inject)")
    try:
        st.audio(INPUT_WAV, format="audio/wav")
    except FileNotFoundError:
        st.warning(f"File {INPUT_WAV} not found.")

with col2:
    st.subheader("2. ESP32 Processed Output")
    st.markdown("*921.6k Baud UART Stream | Core 1 RTOS Task*")
    try:
        st.audio(OUTPUT_WAV, format="audio/wav")
    except FileNotFoundError:
        st.warning(f"File {OUTPUT_WAV} not found.")

st.divider()

# --- Data Visualization Section ---
st.subheader("📊 DSP Mathematical Validation")
st.markdown("Comparing the time-domain waveform and frequency-domain spectrogram.")

try:
    rate1, data_in = wavfile.read(INPUT_WAV)
    rate2, data_out = wavfile.read(OUTPUT_WAV)

    # Grab a small slice of audio to show the waveform distortion
    slice_start = int(rate1 * 1.5)  # Start 1.5 seconds in
    slice_end = slice_start + int(rate1 * 0.02) # 20ms slice

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 7))

    # Time Domain Plot
    ax1.plot(data_in[slice_start:slice_end], label="Clean Input", alpha=0.6)
    ax1.plot(data_out[slice_start:slice_end], label="Asymmetrical Clipped", color='#FF4B4B', alpha=0.9)
    ax1.set_title("Time Domain: Waveform Flattening (Odd & Even Harmonics)")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Spectrogram Plot
    ax2.specgram(data_out, Fs=rate2, NFFT=1024, noverlap=512, cmap='magma')
    ax2.set_title("Spectrogram: 2.8kHz Biquad LPF Roll-off (Anti-Aliasing)")
    ax2.set_ylabel("Frequency (Hz)")
    ax2.set_xlabel("Time (s)")

    plt.tight_layout()
    st.pyplot(fig)

except Exception as e:
    st.error(f"Could not generate graphs: {e}")

# --- Architecture Section ---
st.divider()
st.subheader("⚙️ System Architecture")
st.markdown("""
* **Hardware:** ESP32 Dual-Core Tensilica LX6
* **OS Framework:** FreeRTOS (ESP-IDF)
* **Algorithm:** Newton-Raphson Numerical Solver (Diode clipping)
* **Filters:** IRAM-Optimized Biquad (DC Block, Tone, Speaker Cab Sim)
* **Data Link:** Hardware-in-the-Loop over UART (921600 bps)
""")
