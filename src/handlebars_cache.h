/**
 * Copyright (C) 2016 John Boehr
 *
 * This file is part of handlebars.c.
 *
 * handlebars.c is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * handlebars.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with handlebars.c.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 * @brief Opcode cache
 */

#ifndef HANDLEBARS_CACHE_H
#define HANDLEBARS_CACHE_H

#include "handlebars.h"

HBS_EXTERN_C_START

struct handlebars_cache;
struct handlebars_compiler;
struct handlebars_map;
struct handlebars_module;
struct handlebars_string;

/**
 * @brief Construct a new simple cache
 * @param[in] context The handlebars context
 * @return The cache
 */
struct handlebars_cache * handlebars_cache_simple_ctor(
    struct handlebars_context * context
) HBS_ATTR_NONNULL_ALL HBS_ATTR_RETURNS_NONNULL;

/**
 * @brief Construct a new LMDB cache. The file specified by path does not have
 *        to exist, but must be writeable.
 * @param[in] context The handlebars context
 * @param[in] path The database file
 * @return The cache
 */
struct handlebars_cache * handlebars_cache_lmdb_ctor(
    struct handlebars_context * context,
    const char * path
) HBS_ATTR_NONNULL_ALL HBS_ATTR_RETURNS_NONNULL;

/**
 * @brief Construct a new mmap cache
 * @param[in] context The handlebars context
 * @param[in] size The size of the mmap block, in bytes
 * @param[in] entries The fixed number of entries in the hash table
 * @return The cache
 */
struct handlebars_cache * handlebars_cache_mmap_ctor(
    struct handlebars_context * context,
    size_t size,
    size_t entries
) HBS_ATTR_NONNULL_ALL HBS_ATTR_RETURNS_NONNULL;

/**
 * @brief Destruct a cache
 * @param[in] cache The cache
 * @return void
 */
void handlebars_cache_dtor(struct handlebars_cache * cache) HBS_ATTR_NONNULL_ALL;

#ifndef HANDLEBARS_CACHE_PRIVATE

/**
 * @brief Lookup a program from the cache.
 * @param[in] cache The cache
 * @param[in] key The cache key, Can be a filename, actual template, or arbitrary string
 * @return The cache entry, or NULL
 */
struct handlebars_module * handlebars_cache_find(
    struct handlebars_cache * cache,
    struct handlebars_string * key
);

/**
 * @brief Add a program to the cache. Adding the same key twice is an error.
 * @param[in] cache The cache
 * @param[in] key The cache key. Can be a filename, actual template, or arbitrary string
 * @param[in] program The program
 * @return The cache entry
 */
void handlebars_cache_add(
    struct handlebars_cache * cache,
    struct handlebars_string * key,
    struct handlebars_module * module
);

/**
 * @brief Garbage collect the cache
 * @param[in] cache The cache
 * @return The number of entries removed
 */
int handlebars_cache_gc(struct handlebars_cache * cache);

/**
 * @brief Reset the cache
 * @param[in] cache The cache
 * @return void
 */
void handlebars_cache_reset(struct handlebars_cache * cache);

void handlebars_cache_release(
    struct handlebars_cache * cache,
    struct handlebars_string * key,
    struct handlebars_module * module
);

#else /* HANDLEBARS_CACHE_PRIVATE */

typedef void (*handlebars_cache_add_func)(
    struct handlebars_cache * cache,
    struct handlebars_string * tmpl,
    struct handlebars_module * module
);

typedef struct handlebars_module * (*handlebars_cache_find_func)(
    struct handlebars_cache * cache,
    struct handlebars_string * tmpl
);

typedef int (*handlebars_cache_gc_func)(
    struct handlebars_cache * cache
);

typedef void (*handlebars_cache_release_func)(
        struct handlebars_cache * cache,
        struct handlebars_string * tmpl,
        struct handlebars_module * module
);

typedef struct handlebars_cache_stat (*handlebars_cache_stat_func)(
    struct handlebars_cache * cache
);

typedef void (*handlebars_cache_reset_func)(
    struct handlebars_cache * cache
);

struct handlebars_cache_stat {
    const char * name;

    //! The total size of the cache in bytes
    size_t total_size;

    //! The total size of the cache in bytes
    size_t current_size;

    //! The total size of the table in bytes, or zero for dynamic allocation
    size_t total_table_size;

    //! The current size of the cache table in bytes
    size_t current_table_size;

    //! The total size of the data segment in bytes, or zero for dynamic allocation
    size_t total_data_size;

    //! The current size of the data segment in bytes, or zero for dynamic allocation
    size_t current_data_size;

    //! The total number of entries in the cache, or zero for dynamic allocation
    size_t total_entries;

    //! The current number of entries in the cache
    size_t current_entries;

    //! The number of cache hits
    size_t hits;

    //! The number of cache misses
    size_t misses;

    //! The number of cache entries currently being executed
    size_t refcount;

    //! The number of hash table collisions
    size_t collisions;
};

/**
 * @brief In-memory opcode cache.
 */
struct handlebars_cache {
    //! Common header
    struct handlebars_context ctx;

    //! Opaque pointer for implementation use
    void * internal;

    handlebars_cache_add_func add;

    handlebars_cache_find_func find;

    handlebars_cache_gc_func gc;

    handlebars_cache_release_func release;

    handlebars_cache_stat_func stat;

    handlebars_cache_reset_func reset;

    //! The max amount of time to keep an entry, in seconds, or zero to disable
    double max_age;

    //! The max number of entries to keep, or zero to disable
    size_t max_entries;

    //! The max size of all entries, or zero to disable
    size_t max_size;
};

/**
 * @brief Fetch cache statistics
 * @param[in] cache The cache
 * @return The cache statistics
 */
inline struct handlebars_cache_stat handlebars_cache_stat(struct handlebars_cache * cache)
{
    return cache->stat(cache);
}

inline struct handlebars_module * handlebars_cache_find(
    struct handlebars_cache * cache,
    struct handlebars_string * key
) {
    return cache->find(cache, key);
}

inline void handlebars_cache_add(
    struct handlebars_cache * cache,
    struct handlebars_string * tmpl,
    struct handlebars_module * module
) {
    cache->add(cache, tmpl, module);
}

inline int handlebars_cache_gc(struct handlebars_cache * cache)
{
    return cache->gc(cache);
}

inline void handlebars_cache_reset(struct handlebars_cache * cache)
{
    cache->gc(cache);
}

inline void handlebars_cache_release(
    struct handlebars_cache * cache,
    struct handlebars_string * key,
    struct handlebars_module * module
) {
    cache->release(cache, key, module);
}

#endif /* HANDLEBARS_CACHE_PRIVATE */

HBS_EXTERN_C_END

#endif /* HANDLEBARS_CACHE_H */
