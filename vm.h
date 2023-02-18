#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 256
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots; //< Point to VM's value stack of the first slot a function uses.
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop; //< Pointer one beyond the last added value, used for knowing where in the stack we are
    Table globals; //< Table for global variables.
    Table strings; //< Table used for string interning - a list of all strings assigned so we can do equality checks.
    ObjUpvalue* openUpvalues; //< Linked list used for checking new upvalues to existing ones to make sure they all point to a same variable if needed.
    Obj* objects;
    int grayCount; //< How many GC objects are marked gray
    int grayCapacity; //< Size of gray stack
    Obj** grayStack; //< Stack used to keep track of gray objects as we GC
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
