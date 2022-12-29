#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

// Chunks are a sequence of bytecode

/// @brief Initalize a chunk to zero
/// @param chunk - the existing chunk to zero out
void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
}

/// @brief Write a byte to the chunk array, resizing if we're at capcity
/// @param chunk - chunk to append to
/// @param byte - byte to append to chunk
void writeChunk(Chunk* chunk, uint8_t byte) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    initChunk(chunk);
}