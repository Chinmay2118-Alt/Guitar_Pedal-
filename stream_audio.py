import serial
import numpy as np
from scipy.io import wavfile
import time
import sys

# --- Configuration ---
PORT = '/dev/ttyUSB0'  # Change to /dev/ttyACM0 if necessary
BAUD_RATE = 921600
BLOCK_SIZE = 256
BYTES_PER_BLOCK = BLOCK_SIZE * 2  # 64 int16 samples = 128 bytes

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 stream_audio.py <input_wav_file>")
        return

    input_file = sys.argv[1]
    output_file = input_file.replace('.wav', '_esp32_processed.wav')

    # 1. Load the WAV file using SciPy
    print(f"Loading {input_file}...")
    sample_rate, data = wavfile.read(input_file)
    
    if sample_rate != 48000:
        print(f"Warning: File is {sample_rate}Hz. ESP32 filters expect 48000Hz!")
        
    # Ensure Mono
    if data.ndim > 1:
        data = data[:, 0]
        print("Converted stereo to mono.")
        
    # --- Bulletproof Mathematical Normalization ---
    print(f"Original audio type: {data.dtype}. Mathematically extracting waveform...")
    
    # 1. Convert whatever format it is into high-precision floats
    data_float = data.astype(np.float64)
    
    # 2. Remove any accidental DC offset (center the waveform at 0)
    data_float -= np.mean(data_float)
    
    # 3. Normalize the volume to exactly -1.0 to +1.0
    max_amp = np.max(np.abs(data_float))
    if max_amp > 0:
        data_float = data_float / max_amp
        
    # 4. Scale precisely to the 16-bit integer range (-32767 to +32767)
    data = (data_float * 32767.0).astype(np.int16)
    print("Waveform safely locked into 16-bit format.")


    total_samples = len(data)
    num_blocks = total_samples // BLOCK_SIZE
    output_data = np.zeros_like(data)

    # 2. Open Serial Port and Synchronize
    print(f"Opening {PORT} at {BAUD_RATE} baud...")
    try:
        ser = serial.Serial(PORT, BAUD_RATE, timeout=2.0)
        
        print("Waiting 3 seconds for ESP32 boot sequence to finish...")
        time.sleep(3.0) 
        
        # Destroy all ASCII boot logs
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        print("Buffers flushed. ESP32 is silent. Starting audio stream...\n")
        
    except Exception as e:
        print(f"Failed to open port: {e}")
        return

    start_time = time.time()
    blocks_dropped = 0
    alignment_lost = False
    
    # 3. The Hardware-in-the-Loop Streaming Loop
    for i in range(num_blocks):
        if alignment_lost:
            break

        start_idx = i * BLOCK_SIZE
        end_idx = start_idx + BLOCK_SIZE
        
        block = data[start_idx:end_idx]
        
        # Send 128 bytes to ESP32
        ser.write(block.tobytes())

        # Immediately read 128 bytes back
        received_bytes = ser.read(BYTES_PER_BLOCK)

        if len(received_bytes) == BYTES_PER_BLOCK:
            output_data[start_idx:end_idx] = np.frombuffer(received_bytes, dtype=np.int16)
        else:
            blocks_dropped += 1
            alignment_lost = True
            print(f"\n[ERROR] Serial timeout at block {i}. Alignment lost!")
            break

        if i % 1000 == 0:
            sys.stdout.write(f"\rProgress: {i}/{num_blocks} blocks")
            sys.stdout.flush()

    elapsed = time.time() - start_time
    ser.close()

    print(f"\n\nDone! Processed in {elapsed:.2f} seconds.")
    
    # 4. Save the processed audio using SciPy
    wavfile.write(output_file, sample_rate, output_data)
    print(f"Saved processed audio to -> {output_file}")

if __name__ == "__main__":
    main()
