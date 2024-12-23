#pragma once

#include <stdint.h>

// Fowler–Noll–Vo hash function
static uint64_t strhash_fnv1a(const char *str)
{
    uint64_t hash = 0xcbf29ce484222325;
    int c;
    while ((c = *str++)) {
        hash ^= c;
        hash *= 0x00000100000001b3;
    }
    return hash;
}
static uint64_t memhash_fnv1a(const void *mem, long size)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (const char *byte = mem; byte < (char *)mem + size; ++byte) {
        hash ^= *byte;
        hash *= 0x00000100000001b3;
    }
    return hash;
}

// Daniel J. Bernstein hash function
static uint64_t strhash_djb2(const char *str)
{
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}
static uint64_t memhash_djb2(const void *mem, long size)
{
    uint64_t hash = 5381;
    for (const char *byte = mem; byte < (char *)mem + size; ++byte)
        hash = ((hash << 5) + hash) + *byte;
    return hash;
}

// hash function of sdbm database library, also used by gawk
static uint64_t strhash_sdbm(const char *str)
{
    uint64_t hash = 0;
    int c;
    while ((c = *str++)) hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}
static uint64_t memhash_sdbm(const void *mem, long size)
{
    uint64_t hash = 0;
    for (const char *byte = mem; byte < (char *)mem + size; ++byte)
        hash = *byte + (hash << 6) + (hash << 16) - hash;
    return hash;
}
