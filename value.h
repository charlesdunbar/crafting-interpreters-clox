#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

/**
 * @brief Dynamic array structure used to store constant values
 */
typedef struct {
    int capacity; ///< Maximum size of the array
    int count; ///< Number of elements allocated in values array
    Value* values; ///< Array of actual values
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
