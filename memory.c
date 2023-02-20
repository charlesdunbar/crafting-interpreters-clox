#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

/**
 * @brief Resize or free pointers, based on newSize and oldSize
 *
 * if oldSize == 0 && newSize != 0 === Allocate new block
 * if oldSize != 0 && newSize == 0 === Free pointer
 * if oldSize != 0 && newSize < oldSize === Shrink existing allocation
 * if oldSize != 0 && newSize > oldSize === Grow existing allocation
 * @param pointer pointer to update
 * @param oldSize old capacity size of pointer
 * @param newSize new capacity size of pointer
 * @return NULL if free, otherwise a resized value of pointer
 */
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif        
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    // Exit if we can't resize
    if (result == NULL) exit(1);
    return result;
}

/**
 * @brief Mark an object for GC. Won't get reaped if marked.
 * @param object 
 */
void markObject(Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;

    // We use C realloc so not to call GC when we're GC'ing. yo dawg.
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);

        // If we can't allocate more memory for GC, we fail.
        if (vm.grayStack == NULL) exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

/**
 * @brief Make sure the value is an object (not a number, boolean, or nil)
 * @param value Value to check, and if an object, mark it
 */
void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

/**
 * @brief Iterate over array, marking every value in it.
 * @param array Array to iterate over.
 */
static void markArray(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

/**
 * @brief Mark gray objects as black to tell the GC we've traversed it and don't need to look at it anymore
 * @param object object to mark as visited
 */
static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch(object->type) {
        // Mark any upvalues and any functions in closures.
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        // Mark the function name and any constants in its table.
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        // Mark closed values in upvalues.
        case OBJ_UPVALUE:
            markValue(((ObjUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(OBJ_CLOSURE, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(OBJ_UPVALUE, object);
            break;
        }
    }
}

/**
 * @brief Mark local variables or tempories on the vm stac
 * Then the closures
 * Then the upvalues
 * Then the global variables
 * Then anything the compiler is using
 */
static void markRoots() {
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*)upvalue);
    }

    markTable(&vm.globals);
    markCompilerRoots();
}

/**
 * @brief Walk through gray objects and mark them black once traversed.
 */
static void traceReferences() {
    while (vm.grayCount > 0) {
        Obj* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    // Walk the linked list of every object in the heap
    while (object != NULL) {
        // If object is black (marked), leave it alone
        if (object->isMarked) {
            // Reset black (marked) objects to white (unmarked) for next GC run.
            object->isMarked = false;
            previous = object;
            object = object->next;
        // Else unlink the object
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            // If we're freeing the first node.
            } else {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

    // Mark items for GC
    markRoots();
    // Trace all items, turning gray to black.
    traceReferences();
    // Get rid of string table items if needed.
    tableRemoveWhite(&vm.strings);
    // Get rid of all white (unmarked items) objects.
    sweep();

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.grayStack);
}
