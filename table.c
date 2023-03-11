#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

/**
 * @brief Take a key and an array of buckets, and figure out which bucket the entry belongs to
 * @param entries 
 * @param capacity 
 * @param key 
 * @return 
 */
static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // uint32_t index = key->hash % capacity; - Old slow modulo way
    uint32_t index = key->hash & (capacity - 1); // Cooler bitmask way, since we're always a power of 2.
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        // If key is available, we return it, we have it stored.
        // If key is NULL - it's either not stored, a tombstone (with value true), or available to be used to insert
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry. Can use a tombstone if one available
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        // Otherwise, we have a collision - start using linear probing
        // We insert in the next available bucket we can, wrapping if we reach the end.
        //index = (index + 1) % capacity;
        index = (index + 1) & (capacity - 1);
    }
}

/**
 * @brief Look up a key in a table and update value pointer to its value
 * @param table table to look through
 * @param key key to match
 * @param value pointer to set the matched key to
 * @return true if found in table, false otherwise
 */
bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    // Re-insert every entry if we're resizing, due to bucket placement being based off array size
    // Don't copy over tombstones to save space.
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    // Free the old array
    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

/**
 * @brief Add a key/value pair to a hash table
 * @param table 
 * @param key 
 * @param value 
 * @return true/false if the key is new or not
 */
bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table,capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    // Only increment if empty and not tombstone - we count tombstones as full buckets and
    // they're counted when the initial entry goes in before they become a tombstone.
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    // Find the entry.
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry - Used to prevent gaps in the linear probe
    // Imagine 3 elements all want to be in bucket 2. [1, 2, 3]. If we delete 2,
    // and assume an empty element means we're done probing, we'd never find 3.
    // A tombstone is a sential value used to keep probing. It has key NULL and value true
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

/**
 * @brief Helper method for copying all of the entries of one hash table to another
 * @param from 
 * @param to 
 */
void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i ++){
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

/**
 * @brief Similar to findEntry(), but used for finding strings in the table.
 * @param table 
 * @param chars 
 * @param length 
 * @param hash 
 * @return 
 */
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    //uint32_t index = hash % table->capacity;
    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
            // We found it!
            return entry->key;
        }

        //index = (index + 1) % table->capacity;
        index = (index + 1) & (table->capacity - 1);
    }
}

/**
 * @brief Walk every entry in the table, if an object is not marked, it's about to be deleted.
 * So we delete it here to prevent dangling pointers.
 * @param table
 */
void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}