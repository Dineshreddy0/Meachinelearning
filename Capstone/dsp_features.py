import numpy as np
from scipy.signal import resample_poly

SAMPLE_RATE=16000
FRAME_LEN=400       # 25 ms
HOP_LEN=160         # 10 ms
FFT_SIZE=512
N_MELS=20
N_MFCC=13

def _hz_to_mel(hz):
    return 2595.0*np.log10(1.0+hz/700.0)

def _mel_to_hz(mel):
    return 700.0*(10.0**(mel/2595.0)-1.0)

def mel_filterbank():
    low, high = _hz_to_mel(0), _hz_to_mel(SAMPLE_RATE/2)
    mel_pts=np.linspace(low, high, N_MELS+2)
    bins=np.floor((FFT_SIZE+1)*_mel_to_hz(mel_pts)/SAMPLE_RATE).astype(int)
    bank=np.zeros((N_MELS, FFT_SIZE//2+1), dtype=np.float64)
    for m in range(1,N_MELS+1):
        left,center,right=bins[m-1],bins[m],bins[m+1]
        for k in range(left,center):
            if 0 <= k < bank.shape[1] and center>left:
                bank[m-1,k]=(k-left)/(center-left)
        for k in range(center,right):
            if 0 <= k < bank.shape[1] and right>center:
                bank[m-1,k]=(right-k)/(right-center)
    return bank

BANK=mel_filterbank()
HAM=np.hamming(FRAME_LEN)
DCT=np.cos(np.pi/N_MELS*(np.arange(N_MELS)[None,:]+0.5)*np.arange(N_MFCC)[:,None])

def extract_mfcc(audio):
    x=np.asarray(audio,dtype=np.float64).flatten()
    if len(x)==0: return np.zeros(N_MFCC,dtype=np.float32)
    x=x/32768.0 if np.max(np.abs(x))>2 else x
    x=x-np.mean(x)
    if len(x)<FRAME_LEN:
        x=np.pad(x,(0,FRAME_LEN-len(x)))
    feats=[]
    for start in range(0, max(1,len(x)-FRAME_LEN+1), HOP_LEN):
        frame=x[start:start+FRAME_LEN]
        if len(frame)<FRAME_LEN: frame=np.pad(frame,(0,FRAME_LEN-len(frame)))
        frame=(frame-np.mean(frame))*HAM
        spec=np.fft.rfft(frame,n=FFT_SIZE)
        power=(np.abs(spec)**2)/FFT_SIZE
        mel=np.maximum(BANK @ power,1e-10)
        logmel=np.log(mel)
        mfcc=(DCT @ logmel)
        feats.append(mfcc)
        if start+FRAME_LEN>=len(x): break
    if not feats: return np.zeros(N_MFCC,dtype=np.float32)
    return np.mean(feats,axis=0).astype(np.float32)
