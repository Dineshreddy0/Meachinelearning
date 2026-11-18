import argparse, os, time
import sounddevice as sd
from scipy.io.wavfile import write

RATE = 16000
DURATION = 1.2

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--label", required=True, choices=["LIGHT_ON","LIGHT_OFF","FAN_ON","FAN_OFF","UNKNOWN"])
    p.add_argument("--count", type=int, default=20)
    p.add_argument("--duration", type=float, default=DURATION)
    a = p.parse_args()
    folder = os.path.join("dataset", a.label)
    os.makedirs(folder, exist_ok=True)
    print(f"Recording {a.count} samples for {a.label} at {RATE} Hz.")
    for i in range(a.count):
        input(f"Press Enter, then say '{a.label.replace('_',' ')}' clearly ({i+1}/{a.count})...")
        audio = sd.rec(int(a.duration*RATE), samplerate=RATE, channels=1, dtype="int16")
        sd.wait()
        path = os.path.join(folder, f"{a.label.lower()}_{int(time.time())}_{i:03d}.wav")
        write(path, RATE, audio)
        print("Saved", path)
    print("Done.")

if __name__ == "__main__":
    main()
