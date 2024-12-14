#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// general purpose associative array using open chaining
typedef struct Dict Dict;
typedef struct DictItem DictItem;
typedef size_t DictKeyHash(const char *);
typedef void *DictDataCopy(void *, const void *, size_t);
typedef void DictDataFree(void *);

struct Dict {
    long size, capacity;      // number of items and buckets in dict
    float load_factor;        // load factor for resizing
    DictItem *bucket;         // array of buckets
    DictKeyHash *key_hash;    // pointer to a function for hasing keys
    size_t data_size;         // size of item data in bytes
    DictDataCopy *data_copy;  // pointer to a function for copying data
    DictDataFree *data_free;  // pointer to a function for freeing data
};

struct DictItem {
    char *key;       // pointer to key
    void *data;      // pointer to stored data
    size_t hash;     // raw hash value of key
    DictItem *next;  // point to next item in bucket
};

#define DictForEach(item, dict)                                                                   \
    for (DictItem *bucket = (dict)->bucket; bucket < (dict)->bucket + (dict)->capacity; ++bucket) \
        if (bucket->key)                                                                          \
            for (DictItem *item = bucket; item; item = item->next)

// create an empty dict
static Dict dict_create(long capacity, float load_factor, DictKeyHash *key_hash, size_t data_size,
                        DictDataCopy *data_copy, DictDataFree *data_free)
{
    assert(capacity >= 0);
    assert(0 < load_factor && load_factor < 1);
    assert(key_hash);
    return (Dict){.capacity = capacity,
                  .load_factor = load_factor,
                  .key_hash = key_hash,
                  .data_size = data_size,
                  .data_copy = data_copy,
                  .data_free = data_free};
}

// create dict buckets
static void x__dict_create_buckets(Dict *dict)
{
    dict->bucket = calloc(dict->capacity, sizeof(*dict->bucket));
    assert(dict->bucket);
}

// resize dict buckets
static void x__dict_resize_buckets(Dict *dict)
{
    assert(dict);

    // create new buckets
    const size_t _capacity = dict->capacity / dict->load_factor + 1;
    DictItem *_bucket = calloc(_capacity, sizeof(*_bucket));
    assert(_bucket);

    // insert old items
    for (DictItem *bucket = dict->bucket; bucket < dict->bucket + dict->capacity; ++bucket) {
        if (!bucket->key) continue;
        for (DictItem *item = bucket, *next; item; item = next) {
            next = item->next;  // for the event that item is free'd

            // find position
            const size_t hash = item->hash;  // no need to recompute the hash
            DictItem *_item = &_bucket[hash % _capacity];
            DictItem *_prev = 0;
            while (_item && _item->key && _item->hash != hash && strcmp(_item->key, item->key)) {
                _prev = _item;
                _item = _item->next;
            }

            // insert item
            if (!_item) {  // collision: append item
                if (item == bucket) {
                    _item = malloc(sizeof(*_item));
                    assert(_item);
                    _item->key = item->key;
                    _item->data = item->data;
                    _item->hash = item->hash;
                }
                else {
                    _item = item;
                }
                _item->next = 0;
                _prev->next = _item;
            }
            else if (!_item->key) {  // empty bucket: add item
                _item->key = item->key;
                _item->data = item->data;
                _item->hash = item->hash;
                if (item != bucket) free(item);
            }
            else {  // same key: cannot occur
                assert(0);
            }
        }
    }

    // housekeeping
    free(dict->bucket);
    dict->bucket = _bucket;
    dict->capacity = _capacity;
}

// create a dict item
static void x__dict_item_create(const Dict *dict, DictItem *item, const char *key, void *data,
                                size_t hash)
{
    item->key = strdup(key);
    assert(item->key);
    if (data && dict->data_copy) {
        item->data = malloc(dict->data_size);
        assert(item->data);
        dict->data_copy(item->data, data, dict->data_size);
    }
    else {
        item->data = data;
    }
    item->hash = hash;
}

// insert an item with a given key
[[maybe_unused]] static void *dict_insert(Dict *dict, const char *key, void *data)
{
    assert(dict);
    assert(key);

    // housekeeping
    if (!dict->bucket) x__dict_create_buckets(dict);
    if (dict->size + 1 > dict->capacity * dict->load_factor) x__dict_resize_buckets(dict);

    // find position
    const size_t hash = dict->key_hash(key);
    DictItem *item = &dict->bucket[hash % dict->capacity];
    DictItem *prev = 0;
    while (item && item->key && item->hash != hash && strcmp(item->key, key)) {
        prev = item;
        item = item->next;
    }

    // insert item
    if (!item) {  // collision: append item
        item = malloc(sizeof(*item));
        assert(item);
        x__dict_item_create(dict, item, key, data, hash);
        item->next = 0;
        prev->next = item;
    }
    else if (!item->key) {  // empty bucket: add item
        x__dict_item_create(dict, item, key, data, hash);
    }
    else {  // same key: swap data
        void *item_data = item->data;
        item->data = data;
        return item_data;
    }

    // housekeeping
    dict->size += 1;
    return 0;
}

// return a copy of the dict
[[maybe_unused]] static Dict dict_copy(const Dict *dict)
{
    assert(dict);
    Dict copy = dict_create(dict->capacity, dict->load_factor, dict->key_hash, dict->data_size,
                            dict->data_copy, dict->data_free);
    for (const DictItem *bucket = dict->bucket; bucket < dict->bucket + dict->capacity; ++bucket)
        if (bucket->key)
            for (const DictItem *item = bucket; item; item = item->next)
                dict_insert(&copy, item->key, item->data);
    return copy;
}

// remove an item with a given key, and return its data
[[maybe_unused]] static void *dict_remove(Dict *dict, const char *key)
{
    assert(dict);
    assert(key);

    // find position
    const size_t hash = dict->key_hash(key);
    DictItem *item = &dict->bucket[hash % dict->capacity];
    DictItem *prev = 0;
    while (item && item->key && item->hash != hash && strcmp(item->key, key)) {
        prev = item;
        item = item->next;
    }

    // remove item
    if (!item || !item->key) return 0;  // item not in dict
    void *data = item->data;
    DictItem *next = item->next;
    free(item->key);

    // update bucket
    if (!prev) {  // head item
        if (next) {
            *item = *next;  // move next item up to head
            free(next);
        }
        else {
            *item = (DictItem){0};  // clear head
        }
    }
    else {  // chained item
        free(item);
        prev->next = next;
    }

    // housekeeping
    dict->size -= 1;
    return data;
}

// return an item with a given key
[[maybe_unused]] static void *dict_find(const Dict *dict, const char *key)
{
    assert(dict);
    assert(key);
    const size_t hash = dict->key_hash(key);
    DictItem *item = &dict->bucket[hash % dict->capacity];
    while (item && item->key && item->hash != hash && strcmp(item->key, key)) item = item->next;
    return (!item || !item->key ? 0 : item);
}

// remove all items from the dict
[[maybe_unused]] static void dict_clear(Dict *dict)
{
    assert(dict);
    if (!dict->bucket) return;
    for (DictItem *bucket = dict->bucket; bucket < dict->bucket + dict->capacity; ++bucket) {
        if (!bucket->key) continue;
        free(bucket->key);
        if (dict->data_free) dict->data_free(bucket->data);
        for (DictItem *item = bucket->next, *next; item; item = next) {
            next = item->next;
            free(item->key);
            if (dict->data_free) dict->data_free(item->data);
            free(item);
        }
    }
    free(dict->bucket);
    dict->bucket = 0;
}
