#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

// general purpose set using hashing and open addressing
typedef struct Set Set;
typedef struct SetItem SetItem;
typedef uint64_t SetDataHash(const void *mem, long size);
typedef void *SetDataCopy(void *, const void *, size_t);
typedef void SetDataFree(void *);

struct Set {
    long size, capacity, data_size, max_dist;
    double load_factor;
    SetDataHash *data_hash;
    SetDataCopy *data_copy;
    SetDataFree *data_free;
    SetItem *item;
};

struct SetItem {
    void *data;
    uint64_t hash;
};

#define SetForEach(item, set)                                                       \
    for (SetItem *item = (set)->item; item < (set)->item + (set)->capacity; ++item) \
        if (item->data)

// create an empty set
static Set set_create_full(long capacity, long data_size, double load_factor,
                           SetDataHash *data_hash, SetDataCopy *data_copy, SetDataFree *data_free)
{
    assert(capacity >= 0);
    assert(data_size >= 0);
    assert(0 < load_factor && load_factor < 1);
    assert(data_hash);
    return (Set){
        .capacity = capacity / load_factor + 1,
        .data_size = data_size,
        .load_factor = load_factor,
        .data_hash = data_hash,
        .data_copy = data_copy,
        .data_free = data_free,
    };
}
static Set set_create(long capacity, long data_size)
{
    return set_create_full(capacity, data_size, 0.75, memhash_fnv1a, memcpy, free);
}

static void x__set_create_items(Set *set)
{
    set->item = calloc(set->capacity, sizeof(*set->item));
    assert(set->item);
}

static void x__set_resize_items(Set *set)
{
    assert(set);
    const long _capacity = set->capacity / set->load_factor + 1;
    SetItem *_item = calloc(_capacity, sizeof(*_item));
    assert(_item);
    long _max_dist = 0;
    for (SetItem *item = set->item; item < set->item + set->capacity; ++item) {
        if (!item->data) continue;
        long _dist = 0, _i = item->hash % _capacity;
        while (_item[_i].data) {
            _dist += 1;
            _i = (_i + 1) % _capacity;
        }
        _item[_i].data = item->data;
        _item[_i].hash = item->hash;
        if (_dist > _max_dist) _max_dist = _dist;
    }
    free(set->item);
    set->item = _item;
    set->capacity = _capacity;
    set->max_dist = _max_dist;
}

static void x__set_item_create(const Set *set, SetItem *item, void *data, uint64_t hash)
{
    if (set->data_copy) {
        item->data = malloc(set->data_size);
        assert(item->data);
        set->data_copy(item->data, data, set->data_size);
    }
    else {
        item->data = data;
    }
    item->hash = hash;
}

// insert an item; on collision, keep or replace data and return old data
static void *set_insert(Set *set, void *data, int keep)
{
    assert(set);
    assert(data);
    if (!set->item) x__set_create_items(set);
    if (set->size + 1 > set->capacity * set->load_factor) x__set_resize_items(set);
    const uint64_t hash = set->data_hash(data, set->data_size);
    long dist = 0, i = hash % set->capacity;
    SetItem *item = &set->item[i];
    while (item->data && (item->hash != hash || memcmp(item->data, data, set->data_size))) {
        dist += 1;
        i = (i + 1) % set->capacity;
        item = &set->item[i];
    }
    if (!item->data) {
        x__set_item_create(set, item, data, hash);
        if (dist > set->max_dist) set->max_dist = dist;
    }
    else {
        void *item_data = item->data;
        if (!keep) item->data = data;
        return item_data;
    }
    set->size += 1;
    return 0;
}

// return a copy of the set
static Set set_copy(const Set *set)
{
    assert(set);
    Set copy = set_create_full(set->size, set->data_size, set->load_factor, set->data_hash,
                               set->data_copy, set->data_free);
    if (set->size == 0) return copy;
    for (const SetItem *item = set->item; item < set->item + set->capacity; ++item)
        if (item->data) set_insert(&copy, item->data, 0);
    return copy;
}

// remove an item, and return its data
static void *set_remove(Set *set, const void *data)
{
    assert(set);
    assert(data);
    if (set->size == 0) return 0;
    const uint64_t hash = set->data_hash(data, set->data_size);
    long i = hash % set->capacity;
    SetItem *item = &set->item[i];
    for (long dist = 0; dist <= set->max_dist; ++dist) {
        if (item->data && item->hash == hash && !memcmp(item->data, data, set->data_size)) {
            void *item_data = item->data;
            memset(item, 0, sizeof(*item));
            set->size -= 1;
            return item_data;
        }
        i = (i + 1) % set->capacity;
        item = &set->item[i];
    }
    return 0;
}

// return the data of an item
static void *set_find(const Set *set, const void *data)
{
    assert(set);
    assert(data);
    if (set->size == 0) return 0;
    const uint64_t hash = set->data_hash(data, set->data_size);
    long i = hash % set->capacity;
    SetItem *item = &set->item[i];
    for (long dist = 0; dist <= set->max_dist; ++dist) {
        if (item->data && item->hash == hash && !memcmp(item->data, data, set->data_size))
            return item->data;
        i = (i + 1) % set->capacity;
        item = &set->item[i];
    }
    return 0;
}

// remove all items from the set
static void set_clear(Set *set)
{
    assert(set);
    if (!set->item) return;
    if (set->data_free)
        for (SetItem *item = set->item; item < set->item + set->capacity; ++item)
            if (item->data) set->data_free(item->data);
    free(set->item);
    *set = (Set){0};
}
