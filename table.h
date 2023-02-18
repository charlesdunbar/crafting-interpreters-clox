#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// Struct that holds a key/value pair for a hash table.
typedef struct {
    ObjString* key;
    Value value;
} Entry;

// Hash table, with an array of Entry, and the count and capacity of the struct.
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void markTable(Table* table);

#endif
