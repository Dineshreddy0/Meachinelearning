import os, glob
import numpy as np
from scipy.io.wavfile import read
from dsp_features import extract_mfcc, SAMPLE_RATE

LABELS=["LIGHT_ON","LIGHT_OFF","FAN_ON","FAN_OFF"]

def load_wav(path):
    sr,x=read(path)
    if x.ndim>1: x=x.mean(axis=1)
    if sr != SAMPLE_RATE:
        # Polyphase resampling; convert to float before resampling.
        from scipy.signal import resample_poly
        import math
        g=math.gcd(int(sr),SAMPLE_RATE)
        x=resample_poly(x.astype(np.float32),SAMPLE_RATE//g,int(sr)//g)
    return x.astype(np.float32)

def main():
    centroids=[]
    print("Extracting MFCC features...")
    for label in LABELS:
        paths=glob.glob(os.path.join("dataset",label,"*.wav"))
        if len(paths)<5:
            raise SystemExit(f"Need at least 5 WAV files in dataset/{label}/; found {len(paths)}")
        vectors=np.stack([extract_mfcc(load_wav(p)) for p in paths])
        centroids.append(vectors.mean(axis=0))
        print(f"{label}: {len(paths)} recordings")
    centroids=np.stack(centroids).astype(np.float32)
    out=os.path.join("firmware","VoiceAppliance","model.h")
    os.makedirs(os.path.dirname(out),exist_ok=True)
    with open(out,"w") as f:
        f.write("#pragma once\n#define NUM_CLASSES 4\n#define NUM_MFCC 13\n")
        f.write("static const char* COMMAND_LABELS[NUM_CLASSES] = {"+", ".join('"'+x+'"' for x in LABELS)+"};\n")
        f.write("static const float MODEL_CENTROIDS[NUM_CLASSES][NUM_MFCC] = {\n")
        for row in centroids:
            f.write("  {"+", ".join(f"{v:.8f}f" for v in row)+"},\n")
        f.write("};\n")
    print("Wrote",out)
    print("IMPORTANT: validate with separate recordings not used for training.")

if __name__=="__main__":
    main()
