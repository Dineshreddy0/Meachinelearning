#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>
#include "config.h"
#include "model.h"

static int32_t rawSamples[CAPTURE_SAMPLES];
static float audioSamples[CAPTURE_SAMPLES];
static float melBank[N_MELS][FFT_SIZE/2+1];
static float ham[FRAME_LEN];
static float dctMat[N_MFCC][N_MELS];

static bool lightOn=false, fanOn=false;
static uint32_t lastAccepted=0;

static float hzToMel(float hz){ return 2595.0f*log10f(1.0f+hz/700.0f); }
static float melToHz(float mel){ return 700.0f*(powf(10.0f,mel/2595.0f)-1.0f); }

void initDSP(){
  for(int i=0;i<FRAME_LEN;i++) ham[i]=0.54f-0.46f*cosf(2.0f*PI*i/(FRAME_LEN-1));
  for(int c=0;c<N_MFCC;c++)
    for(int m=0;m<N_MELS;m++)
      dctMat[c][m]=cosf(PI/N_MELS*(m+0.5f)*c);
  float lo=hzToMel(0), hi=hzToMel(SAMPLE_RATE/2);
  int bins[N_MELS+2];
  for(int i=0;i<N_MELS+2;i++){
    float mel=lo+(hi-lo)*i/(N_MELS+1);
    bins[i]=(int)floorf((FFT_SIZE+1)*melToHz(mel)/SAMPLE_RATE);
  }
  memset(melBank,0,sizeof(melBank));
  for(int m=1;m<=N_MELS;m++){
    int l=bins[m-1], c=bins[m], r=bins[m+1];
    for(int k=l;k<c && k<=FFT_SIZE/2;k++) if(k>=0 && c>l) melBank[m-1][k]=(float)(k-l)/(c-l);
    for(int k=c;k<r && k<=FFT_SIZE/2;k++) if(k>=0 && r>c) melBank[m-1][k]=(float)(r-k)/(r-c);
  }
}

// In-place radix-2 complex FFT.
void fft(float *re,float *im){
  int n=FFT_SIZE,j=0;
  for(int i=1;i<n;i++){
    int bit=n>>1;
    for(;j&bit;bit>>=1) j^=bit;
    j^=bit;
    if(i<j){ float tr=re[i];re[i]=re[j];re[j]=tr; float ti=im[i];im[i]=im[j];im[j]=ti; }
  }
  for(int len=2;len<=n;len<<=1){
    float ang=-2.0f*PI/len, wlenR=cosf(ang), wlenI=sinf(ang);
    for(int i=0;i<n;i+=len){
      float wr=1,wi=0;
      for(int k=0;k<len/2;k++){
        int u=i+k,v=i+k+len/2;
        float vr=re[v]*wr-im[v]*wi, vi=re[v]*wi+im[v]*wr;
        re[v]=re[u]-vr; im[v]=im[u]-vi;
        re[u]+=vr; im[u]+=vi;
        float nwr=wr*wlenR-wi*wlenI; wi=wr*wlenI+wi*wlenR; wr=nwr;
      }
    }
  }
}

bool captureAudio(){
  size_t bytesRead=0;
  int got=0;
  while(got<CAPTURE_SAMPLES){
    size_t want=(CAPTURE_SAMPLES-got)*sizeof(int32_t);
    esp_err_t e=i2s_read(I2S_NUM_0,((uint8_t*)rawSamples)+got*sizeof(int32_t),want,&bytesRead,portMAX_DELAY);
    if(e!=ESP_OK || bytesRead==0) return false;
    got += bytesRead/sizeof(int32_t);
  }
  float mean=0;
  for(int i=0;i<CAPTURE_SAMPLES;i++){
    // INMP441 commonly delivers signed 24-bit data in the upper bits of 32-bit I2S words.
    int32_t s=rawSamples[i]>>8;
    audioSamples[i]=(float)s/8388608.0f;
    mean+=audioSamples[i];
  }
  mean/=CAPTURE_SAMPLES;
  float energy=0;
  for(int i=0;i<CAPTURE_SAMPLES;i++){
    audioSamples[i]-=mean;
    energy+=audioSamples[i]*audioSamples[i];
  }
  float rms=sqrtf(energy/CAPTURE_SAMPLES);
  Serial.printf("RMS: %.4f\n",rms);
  return rms>=VAD_RMS_THRESHOLD;
}

bool extractFeatures(float out[N_MFCC]){
  float sums[N_MFCC]={0};
  int frameCount=0;
  float re[FFT_SIZE], im[FFT_SIZE], power[FFT_SIZE/2+1], logMel[N_MELS];
  for(int start=0;start+FRAME_LEN<=CAPTURE_SAMPLES;start+=HOP_LEN){
    memset(re,0,sizeof(re)); memset(im,0,sizeof(im));
    float frameMean=0;
    for(int i=0;i<FRAME_LEN;i++) frameMean+=audioSamples[start+i];
    frameMean/=FRAME_LEN;
    for(int i=0;i<FRAME_LEN;i++) re[i]=(audioSamples[start+i]-frameMean)*ham[i];
    fft(re,im);
    for(int k=0;k<=FFT_SIZE/2;k++) power[k]=(re[k]*re[k]+im[k]*im[k])/FFT_SIZE;
    for(int m=0;m<N_MELS;m++){
      float e=0;
      for(int k=0;k<=FFT_SIZE/2;k++) e+=melBank[m][k]*power[k];
      logMel[m]=logf(fmaxf(e,1e-10f));
    }
    for(int c=0;c<N_MFCC;c++){
      float v=0;
      for(int m=0;m<N_MELS;m++) v+=dctMat[c][m]*logMel[m];
      sums[c]+=v;
    }
    frameCount++;
  }
  if(frameCount==0) return false;
  for(int c=0;c<N_MFCC;c++) out[c]=sums[c]/frameCount;
  return true;
}

int classify(const float f[N_MFCC],float &best,float &margin){
  float d[NUM_CLASSES];
  for(int c=0;c<NUM_CLASSES;c++){
    float s=0;
    for(int j=0;j<N_MFCC;j++){ float x=f[j]-MODEL_CENTROIDS[c][j]; s+=x*x; }
    d[c]=sqrtf(s);
  }
  int first=0,second=1;
  if(d[second]<d[first]){int t=first;first=second;second=t;}
  for(int c=2;c<NUM_CLASSES;c++){
    if(d[c]<d[first]){second=first;first=c;}
    else if(d[c]<d[second]) second=c;
  }
  best=d[first]; margin=d[second]-d[first];
  return first;
}

void writeRelay(int pin,bool on){
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on?LOW:HIGH) : (on?HIGH:LOW));
}
void applyCommand(int id){
  switch(id){
    case 0: lightOn=true; break;
    case 1: lightOn=false; break;
    case 2: fanOn=true; break;
    case 3: fanOn=false; break;
  }
  writeRelay(RELAY_LIGHT_PIN,lightOn);
  writeRelay(RELAY_FAN_PIN,fanOn);
  Serial.printf("APPLIANCE STATE | LIGHT=%s FAN=%s\n",lightOn?"ON":"OFF",fanOn?"ON":"OFF");
}

void setupI2S(){
  i2s_config_t cfg={
    .mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX),
    .sample_rate=SAMPLE_RATE,
    .bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format=I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format=I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags=ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count=8,
    .dma_buf_len=256,
    .use_apll=false,
    .tx_desc_auto_clear=false,
    .fixed_mclk=0
  };
  i2s_pin_config_t pins={.bck_io_num=I2S_BCLK,.ws_io_num=I2S_WS,.data_out_num=I2S_PIN_NO_CHANGE,.data_in_num=I2S_DIN};
  ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0,&cfg,0,NULL));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0,&pins));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_NUM_0));
}

void setup(){
  Serial.begin(115200);
  pinMode(RELAY_LIGHT_PIN,OUTPUT); pinMode(RELAY_FAN_PIN,OUTPUT); pinMode(STATUS_LED_PIN,OUTPUT);
  // Safe startup: both appliances OFF.
  writeRelay(RELAY_LIGHT_PIN,false); writeRelay(RELAY_FAN_PIN,false);
  setupI2S(); initDSP();
  Serial.println("\nESP32 DSP Voice Appliance Controller ready.");
  Serial.println("Train model.h before using command recognition.");
}

void loop(){
  if(!captureAudio()){ delay(30); return; }
  float features[N_MFCC],distance=0,margin=0;
  if(!extractFeatures(features)) return;
  int id=classify(features,distance,margin);
  bool accepted=(distance<=MAX_DISTANCE && margin>=MIN_MARGIN);
  Serial.printf("Prediction=%s distance=%.3f margin=%.3f accepted=%s\n",
    COMMAND_LABELS[id],distance,margin,accepted?"YES":"NO");
  if(accepted && (millis()-lastAccepted)>COMMAND_LOCKOUT_MS){
    applyCommand(id); lastAccepted=millis();
    digitalWrite(STATUS_LED_PIN,HIGH); delay(100); digitalWrite(STATUS_LED_PIN,LOW);
  }
  delay(50);
}
