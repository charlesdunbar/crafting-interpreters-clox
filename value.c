#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"

/**
 * @brief Initalize a value array struct to 0
 * @param array The array to initalize

 */
void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

/**
 * @brief Write a value to the value array struct, increasing its capacity if full
 * @param array The array to write the value to
 * @param value The value to add to the array
 */
void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

/**
 * @brief Free the memory used for a value array struct
 * @param array The value array to free

 */
void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

/**
 * @brief Print a value
 * @param value The value to print
 */
void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: printObject(value); break;
    }
    
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch(a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:       return true;
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:       return AS_OBJ(a) == AS_OBJ(b);
        default:            return false; // Unreachable.
    }
}
