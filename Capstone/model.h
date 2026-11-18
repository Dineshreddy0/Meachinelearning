#pragma once
// Placeholder model. Run python train_model.py to replace these centroids.
#define NUM_CLASSES 4
#define NUM_MFCC 13
static const char* COMMAND_LABELS[NUM_CLASSES] = {"LIGHT_ON","LIGHT_OFF","FAN_ON","FAN_OFF"};
static const float MODEL_CENTROIDS[NUM_CLASSES][NUM_MFCC] = {
 {0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0,0,0,0,0}
};
