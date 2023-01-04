#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;

/**
 * @brief Reset the stackTop pointer to the beginning of the stack.
 */
static void resetStack() {
    vm.stackTop = vm.stack;
}

/**
 * @brief Initalize the Virtual machine
 */
void initVM() {
    resetStack();
}

void freeVM() {

}

/**
 * @brief Add a value to the VM's stack, incrementing stackTop afterwords.
 * @param value Value to add to stack
 */
void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
    //TODO: test with *vm.stackTop++ = value;
}

/**
 * @brief Return the value at the stackTop pointer after decrementing it, causing the next push() to overwrite the value.
 * @return Value at the stackTop pointer
 */
Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

/**
 * @brief Code used for interpreting bytecode
 * @return Status of the intrepretation, either OK or some error
 */
static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
// Preprocessor hack to mack sure semicolon statements end up in same block
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
        } while (false)

    for(;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT:
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            case OP_NEGATE:   push(-pop()); break;
            case OP_RETURN: {
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

/**
 * @brief interpret a string of source code
 * @return Wheather the interpretation was ok, or some error occured
*/
InterpretResult interpret(const char* source) {
    compile(source);
    return INTERPRET_OK;
}
