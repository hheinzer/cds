#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

// general purpose associative array using open addressing
typedef struct Hmap Hmap;
typedef struct HmapItem HmapItem;
typedef uint64_t HmapKeyHash(const char *);
typedef void *HmapDataCopy(void *, const void *, size_t);
typedef void HmapDataFree(void *);

struct Hmap {
    long size, capacity, data_size, max_dist;
    double load_factor;
    HmapKeyHash *key_hash;
    HmapDataCopy *data_copy;
    HmapDataFree *data_free;
    HmapItem *item;
};

struct HmapItem {
    char *key;
    void *data;
    uint64_t hash;
};

#define HmapForEach(item, hmap)                                                         \
    for (HmapItem *item = (hmap)->item; item < (hmap)->item + (hmap)->capacity; ++item) \
        if (item->key)

// create an empty hmap
static Hmap hmap_create_full(long capacity, long data_size, double load_factor,
                             HmapKeyHash *key_hash, HmapDataCopy *data_copy,
                             HmapDataFree *data_free)
{
    assert(capacity >= 0);
    assert(data_size >= 0);
    assert(0 < load_factor && load_factor < 1);
    assert(key_hash);
    return (Hmap){
        .capacity = capacity / load_factor + 1,
        .data_size = data_size,
        .load_factor = load_factor,
        .key_hash = key_hash,
        .data_copy = data_copy,
        .data_free = data_free,
    };
}
static Hmap hmap_create(long capacity, long data_size)
{
    return hmap_create_full(capacity, data_size, 0.75, strhash_fnv1a, memcpy, free);
}

static void x__hmap_create_items(Hmap *hmap)
{
    hmap->item = calloc(hmap->capacity, sizeof(*hmap->item));
    assert(hmap->item);
}

static void x__hmap_resize_items(Hmap *hmap)
{
    assert(hmap);
    const long _capacity = hmap->capacity / hmap->load_factor + 1;
    HmapItem *_item = calloc(_capacity, sizeof(*_item));
    assert(_item);
    long _max_dist = 0;
    for (HmapItem *item = hmap->item; item < hmap->item + hmap->capacity; ++item) {
        if (!item->key) continue;
        long _dist = 0, _i = item->hash % _capacity;
        while (_item[_i].key) {
            _dist += 1;
            _i = (_i + 1) % _capacity;
        }
        _item[_i].key = item->key;
        _item[_i].data = item->data;
        _item[_i].hash = item->hash;
        if (_dist > _max_dist) _max_dist = _dist;
    }
    free(hmap->item);
    hmap->item = _item;
    hmap->capacity = _capacity;
    hmap->max_dist = _max_dist;
}

static void x__hmap_item_create(const Hmap *hmap, HmapItem *item, const char *key, void *data,
                                uint64_t hash)
{
    item->key = strdup(key);
    assert(item->key);
    if (data && hmap->data_copy) {
        item->data = malloc(hmap->data_size);
        assert(item->data);
        hmap->data_copy(item->data, data, hmap->data_size);
    }
    else {
        item->data = data;
    }
    item->hash = hash;
}

// insert an item with a given key; on collision, keep or replace data and return old data
static void *hmap_insert(Hmap *hmap, const char *key, void *data, int keep)
{
    assert(hmap);
    assert(key);
    if (!hmap->item) x__hmap_create_items(hmap);
    if (hmap->size + 1 > hmap->capacity * hmap->load_factor) x__hmap_resize_items(hmap);
    const uint64_t hash = hmap->key_hash(key);
    long dist = 0, i = hash % hmap->capacity;
    HmapItem *item = &hmap->item[i];
    while (item->key && (item->hash != hash || strcmp(item->key, key))) {
        dist += 1;
        i = (i + 1) % hmap->capacity;
        item = &hmap->item[i];
    }
    if (!item->key) {
        x__hmap_item_create(hmap, item, key, data, hash);
        if (dist > hmap->max_dist) hmap->max_dist = dist;
    }
    else {
        void *item_data = item->data;
        if (!keep) item->data = data;
        return item_data;
    }
    hmap->size += 1;
    return 0;
}

// return a copy of the hmap
static Hmap hmap_copy(const Hmap *hmap)
{
    assert(hmap);
    Hmap copy = hmap_create_full(hmap->size, hmap->data_size, hmap->load_factor, hmap->key_hash,
                                 hmap->data_copy, hmap->data_free);
    if (hmap->size == 0) return copy;
    for (const HmapItem *item = hmap->item; item < hmap->item + hmap->capacity; ++item)
        if (item->key) hmap_insert(&copy, item->key, item->data, 0);
    return copy;
}

// remove an item with a given key, and return its data
static void *hmap_remove(Hmap *hmap, const char *key)
{
    assert(hmap);
    assert(key);
    if (hmap->size == 0) return 0;
    const uint64_t hash = hmap->key_hash(key);
    long i = hash % hmap->capacity;
    HmapItem *item = &hmap->item[i];
    for (long dist = 0; dist <= hmap->max_dist; ++dist) {
        if (item->key && item->hash == hash && !strcmp(item->key, key)) {
            void *data = item->data;
            free(item->key);
            memset(item, 0, sizeof(*item));
            hmap->size -= 1;
            return data;
        }
        i = (i + 1) % hmap->capacity;
        item = &hmap->item[i];
    }
    return 0;
}

// return the data of an item with a given key
static void *hmap_find(const Hmap *hmap, const char *key)
{
    assert(hmap);
    assert(key);
    if (hmap->size == 0) return 0;
    const uint64_t hash = hmap->key_hash(key);
    long i = hash % hmap->capacity;
    HmapItem *item = &hmap->item[i];
    for (long dist = 0; dist <= hmap->max_dist; ++dist) {
        if (item->key && item->hash == hash && !strcmp(item->key, key)) return item->data;
        i = (i + 1) % hmap->capacity;
        item = &hmap->item[i];
    }
    return 0;
}

// remove all items from the hmap
static void hmap_clear(Hmap *hmap)
{
    assert(hmap);
    if (!hmap->item) return;
    for (HmapItem *item = hmap->item; item < hmap->item + hmap->capacity; ++item) {
        if (!item->key) continue;
        free(item->key);
        if (hmap->data_free) hmap->data_free(item->data);
    }
    free(hmap->item);
    *hmap = (Hmap){0};
}
