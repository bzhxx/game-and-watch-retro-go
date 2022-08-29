#include <odroid_system.h>

#include "main.h"
#include "appid.h"

#include "common.h"
#include "gw_linker.h"
#include "gw_flash.h"
#include "gw_lcd.h"
#include "gwenesis_savestate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *headerString = "Gene0000";
static char saveBuffer[256];

#define WORK_BLOCK_SIZE (256)
static int flashBlockOffset = 0;
static bool isLastFlashWrite = 0;

// This function fills 4kB blocks and writes them in flash when full
static void SaveGwenesisFlashSaveData(unsigned char *dest, unsigned char *src, int size) {
    int blockNumber = 0;
    for (int i = 0; i < size; i++) {
        saveBuffer[flashBlockOffset] = src[i];
        flashBlockOffset++;
        if ((flashBlockOffset == WORK_BLOCK_SIZE) || (isLastFlashWrite && (i == size-1))) {
            // Write block in flash
            int intDest = (int)dest+blockNumber*WORK_BLOCK_SIZE;
            intDest = intDest & ~(WORK_BLOCK_SIZE-1);
            unsigned char *newDest = (unsigned char *)intDest;
            OSPI_DisableMemoryMappedMode();
            OSPI_Program((int)newDest,(const unsigned char *)saveBuffer,WORK_BLOCK_SIZE);
            OSPI_EnableMemoryMappedMode();
            flashBlockOffset = 0;
            blockNumber++;
        }
    }
    if (size == 0) {
        // force writing last data block
        int intDest = (int)dest+blockNumber*WORK_BLOCK_SIZE;
        intDest = intDest & ~(WORK_BLOCK_SIZE-1);
        unsigned char *newDest = (unsigned char *)intDest;
        OSPI_DisableMemoryMappedMode();
        OSPI_Program((int)newDest,(const unsigned char *)saveBuffer,WORK_BLOCK_SIZE);
        OSPI_EnableMemoryMappedMode();
        flashBlockOffset = 0;
        blockNumber++;
    }
}

struct SaveStateSection {
    int tag;
    int offset;
};

// We have 31*8 Bytes available for sections info
// Do not increase this value without reserving
// another 256 bytes block for header
#define MAX_SECTIONS 31

struct SaveState {
    struct SaveStateSection sections[MAX_SECTIONS];
    uint16_t section;
    int allocSize;
    int size;
    int offset;
    unsigned char *buffer;
    unsigned char  fileName[64];
};

static SaveState gwenesisSaveState;

static int tagFromName(const char* tagName)
{
    int tag = 0;
    int mod = 1;

    while (*tagName) {
        mod *= 19219;
        tag += mod * *tagName++;
    }

    return tag;
}

/* Savestate functions */
int saveGwenesisState(unsigned char *destBuffer, int save_size) {
    // Convert mem mapped pointer to flash address
    int save_address = destBuffer - &__EXTFLASH_BASE__;
    // Fill context data
    gwenesisSaveState.buffer = (unsigned char *)save_address;
    gwenesisSaveState.offset = 0;
    gwenesisSaveState.section = 0;

    // Erase flash memory
    store_erase((const unsigned char *)destBuffer, save_size);

    isLastFlashWrite = 0;
    // Reserve a page of 256 Bytes for the header info
    // We put 0xff to be able to update values at the end of process
    SaveGwenesisFlashSaveData(gwenesisSaveState.buffer,(unsigned char *)headerString,8);
    memset(gwenesisSaveState.sections,0xff,sizeof(gwenesisSaveState.sections[0])*MAX_SECTIONS);
    SaveGwenesisFlashSaveData(gwenesisSaveState.buffer+8,(unsigned char *)gwenesisSaveState.sections,sizeof(gwenesisSaveState.sections[0])*MAX_SECTIONS);
    gwenesisSaveState.offset += 8+sizeof(gwenesisSaveState.sections[0])*MAX_SECTIONS;
    // Start saving data
    gwenesis_save_state();

    // Write dummy data to force writing last block of data
    SaveGwenesisFlashSaveData(gwenesisSaveState.buffer+gwenesisSaveState.offset,NULL,0);

    // Copy header data in the first 256 bytes block of flash
    SaveGwenesisFlashSaveData(gwenesisSaveState.buffer,(unsigned char *)headerString,8);
    isLastFlashWrite = true;
    SaveGwenesisFlashSaveData(gwenesisSaveState.buffer+8,(unsigned char *)gwenesisSaveState.sections,sizeof(gwenesisSaveState.sections[0])*MAX_SECTIONS);

    return gwenesisSaveState.offset;
}

void saveGwenesisStateSet(SaveState* state, const char* tagName, int value)
{
    SaveGwenesisFlashSaveData(state->buffer+state->offset,(unsigned char *)&value,sizeof(int));
    state->offset+=sizeof(int);
}

void saveGwenesisStateSetBuffer(SaveState* state, const char* tagName, void* buffer, int length)
{
    SaveGwenesisFlashSaveData(state->buffer + state->offset, (unsigned char *)buffer, length);
    state->offset += length;
}

SaveState* saveGwenesisStateOpenForWrite(const char* fileName)
{
    // Update section
    gwenesisSaveState.sections[gwenesisSaveState.section].tag = tagFromName(fileName);
    gwenesisSaveState.sections[gwenesisSaveState.section].offset = gwenesisSaveState.offset;
    gwenesisSaveState.section++;
    return &gwenesisSaveState;
}

/* Loadstate functions */
bool initLoadGwenesisState(unsigned char *srcBuffer) {
    gwenesisSaveState.offset = 0;
    // Check for header
    if (memcmp(headerString,srcBuffer,8) == 0) {
        gwenesisSaveState.buffer = srcBuffer;
        // Copy sections header in structure
        memcpy(gwenesisSaveState.sections,gwenesisSaveState.buffer+8,sizeof(gwenesisSaveState.sections[0])*MAX_SECTIONS);
        return true;
    }
    return false;
}

int loadGwenesisState(unsigned char *srcBuffer) {
    gwenesisSaveState.offset = 0;
    // Check for header
    if (memcmp(headerString,srcBuffer,8) == 0) {
        gwenesisSaveState.buffer = srcBuffer;
        // Copy sections header in structure
        memcpy(gwenesisSaveState.sections,gwenesisSaveState.buffer+8,sizeof(gwenesisSaveState.sections[0])*MAX_SECTIONS);
        gwenesis_load_state();
    }
    return gwenesisSaveState.offset;
}

SaveState* saveGwenesisStateOpenForRead(const char* fileName)
{
    // find offset
    int tag = tagFromName(fileName);
    for (int i = 0; i<MAX_SECTIONS; i++) {
        if (gwenesisSaveState.sections[i].tag == tag) {
            // Found tag
            gwenesisSaveState.offset = gwenesisSaveState.sections[i].offset;
            return &gwenesisSaveState;
        }
    }

    return NULL;
}

int saveGwenesisStateGet(SaveState* state, const char* tagName)
{
    int value;

    memcpy(&value, state->buffer + state->offset, sizeof(int));
    state->offset+=sizeof(int);
    return value;
}

void saveGwenesisStateGetBuffer(SaveState* state, const char* tagName, void* buffer, int length)
{
    memcpy(buffer, state->buffer + state->offset, length);
    state->offset += length;
}
