#pragma once

#include <odroid_system.h>

#include "main.h"

extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef hdma_sai1_a;

#define WIDTH  320
#define HEIGHT 240
#define BPP      4

// Default to 50Hz as it results in more samples than at 60Hz
#define AUDIO_SAMPLE_RATE   (48000)
#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / 50)



typedef enum {
    DMA_TRANSFER_STATE_HF = 0x00,
    DMA_TRANSFER_STATE_TC = 0x01,
} dma_transfer_state_t;
extern dma_transfer_state_t dma_state;
extern uint32_t dma_counter;

extern uint32_t audioBuffer[AUDIO_BUFFER_LENGTH];
extern uint32_t audio_mute;


extern int16_t pendingSamples;
extern int16_t audiobuffer_emulator[AUDIO_BUFFER_LENGTH] __attribute__((section (".audio")));
extern int16_t audiobuffer_dma[AUDIO_BUFFER_LENGTH * 2] __attribute__((section (".audio")));

extern const uint8_t volume_tbl[ODROID_AUDIO_VOLUME_MAX + 1];
