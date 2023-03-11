#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING
// We're hacking big now, and throwing all types into a 64 bit type. 64 bit pointers only really use 48 bits, 
// so we can live with that and 3 extra bits for other types and use NaN from the IEEE spec to store everything in here.
// The trick is converting it all around.

#define SIGN_BIT    ((uint64_t)0x8000000000000000) //< Top bit for sign. If it's 1, we have an object. Otherwise one of our other types.
#define QNAN        ((uint64_t)0x7ffc000000000000) //< Quiet NaN mask, equal to bits 62 through 50 out of 63 being set to 1.

// Two lowest 64 bit values used for type tag
#define TAG_NIL     1 // 01.
#define TAG_FALSE   2 // 10.
#define TAG_TRUE    3 // 11.

typedef uint64_t Value;

#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL) // If it wasn't macros, we could use ((v) == TRUE_VAL || (v) == FALSE_VAL), but can't mention v twice due to side effects.
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL) // If not true, it's false.
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value)       ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN))) // Get rid of sign and qnan bits to get the object pointer.

#define BOOL_VAL(b)     ((b) ? TRUE_VAL: FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE)) 
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE)) 
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL)) // Cast dance and bitwise or with QNAN and 1st bit set.
#define NUMBER_VAL(num) numToValue(num)
#define OBJ_VAL(obj)    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else 

/**
 * @brief enum for determing value type
 */
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ, //< Heap pointer for larger objects
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Check for value type macros
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

// Cast a value macros
#define AS_OBJ(value)       ((value).as.obj)
#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)

// Print an value macros
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

/**
 * @brief Dynamic array structure used to store constant values
 */
typedef struct {
    int capacity; ///< Maximum size of the array
    int count; ///< Number of elements allocated in values array
    Value* values; ///< Array of actual values
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
