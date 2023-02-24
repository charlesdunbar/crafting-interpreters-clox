#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_CLASS(value)     isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)   isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)  isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)  isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)    isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)

#define AS_CLASS(value)     ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)   ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)  ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next; //< Linked list for all upvalues
} ObjUpvalue;

/**
 * @brief Struct to capture local variables from functions
 * All functions get wrapped in this, even if they don't capture local vars.
 * Makes things easier for us though.
 */
typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues; //< Closures may have different number of upvalues, so dynamic array needed. Upvalues also are dynamically allocated, so double pointer time.
    int upvalueCount;
} ObjClosure;

/**
 * @brief Object to hold a class representation.
 */
typedef struct {
    Obj obj;
    ObjString* name;
} ObjClass;

/**
 * @brief Struct defining an instance of a class.
 */
typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields; //< Hash table of the fields for fast lookup.
} ObjInstance;


ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjInstance* newInstance(ObjClass* klass);
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

// Need to use a function because value is used twice.
// A macro definition would call the value twice, which could cause side effects.
// ie: IS_STRING(pop()) would pop twice
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
