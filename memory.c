#include <stdlib.h>

#include "memory.h"
#include "vm.h"

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
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    // Exit if we can't resize
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj* object) {
    switch (object->type) {
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
    }
}

void freeObjects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}
