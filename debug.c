#include <stdio.h>

#include "debug.h"
#include "value.h"

/**
 * @brief Print the name of a chunk and disassemble its instructions
 * @param chunk The bytecode chunk to disassemble
 * @param name The name of the chunk

 */
void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

/**
 * @brief Print an instruction using a constant
 * @param name Name of the instruction
 * @param chunk Bytecode to read
 * @param offset offset to read in the bytecode
 * @return offset value + 2 (1 for opcode, one for operand)
 */
static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

/**
 * @brief Print the instruction name used
 * @param name Instruction name to print
 * @param offset offset to increment
 * @return int of offset + 1
 */
static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * @brief Print an opcode and any extra information about the type
 * @param chunk Bytecode chunk to decode
 * @param offset Where to start reading in the bytecode, in bytes
 * @return updated offset value past the just dissassembled instruction
 */
int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && 
        chunk->lines[offset] == chunk->lines[offset - 1]) {
      printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch(instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
