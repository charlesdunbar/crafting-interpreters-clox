#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip; //< Instruction pointer, or program counter
    Value stack[STACK_MAX];
    Value* stackTop; //< Pointer one beyond the last added value, used for knowing where in the stack we are
    Table globals; //< Table for global variables.
    Table strings; //< Table used for string interning - a list of all strings assigned so we can do equality checks.
    Obj* objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif
