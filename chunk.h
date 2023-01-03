#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

/**
 * @brief Operation Codes that the lox language supports
 */
typedef enum {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_RETURN,
} OpCode;

/**
 * @brief Chunks are a sequence of bytecode
 */
typedef struct {
    int count; ///< Number of elements allocated in code array
    int capacity; ///< Maximum size of code array
    uint8_t* code; ///< The array of bytes of code
    int* lines; ///< Array of lines to relate to source code, mirrors the code array and only stores the line number for the code
    ValueArray constants; ///< Array of constants used for bytecode
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif
