#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

// 

/**
 * @brief Initalize a chunk to zero
 * @param chunk the existing chunk to zero out
 */
void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

/// @brief 
/// @param chunk 
/// @param byte 
/**
 * @brief Write a byte to the chunk array, resizing if we're at capcity
 * @param chunk chunk to append to
 * @param byte byte to append to chunk
 * @param line the source code line number the chunk is associated with. Used for printing where errors are.
 */
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

/**
 * @brief Add a constant to a bytecode
 * @param chunk The bytecode to add the constant to
 * @param value The value to add to the bytecode
 * @return Index of where the value was added in the chunk
 */
int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    // Return the index where the constant was appended so we can located it later
    return chunk->constants.count - 1;
}

/**
 * @brief Free a bytecode
 * @param chunk The bytecode to free
 */
void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}
