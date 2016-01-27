
#ifndef HANDLEBARS_VALUE_H
#define HANDLEBARS_VALUE_H

#include <stddef.h>
#include "handlebars_helpers.h"
#include "handlebars_memory.h"

struct handlebars_map;
struct handlebars_stack;
struct handlebars_value;
struct handlebars_value_handlers;
struct json_object;

enum handlebars_value_type {
	HANDLEBARS_VALUE_TYPE_NULL = 0,
	HANDLEBARS_VALUE_TYPE_BOOLEAN,
    HANDLEBARS_VALUE_TYPE_INTEGER,
	HANDLEBARS_VALUE_TYPE_FLOAT,
    HANDLEBARS_VALUE_TYPE_STRING,
    HANDLEBARS_VALUE_TYPE_ARRAY,
    HANDLEBARS_VALUE_TYPE_MAP,
	HANDLEBARS_VALUE_TYPE_USER,
	HANDLEBARS_VALUE_TYPE_PTR,
	HANDLEBARS_VALUE_TYPE_HELPER
};

struct handlebars_value {
	enum handlebars_value_type type;
	struct handlebars_value_handlers * handlers;
	union {
		long lval;
        short bval;
		double dval;
        char * strval;
        struct handlebars_map * map;
        struct handlebars_stack * stack;
		void * usr;
		void * ptr;
        handlebars_helper_func helper;
	} v;
    int refcount;
	void * ctx;
};

static inline int handlebars_value_addref(struct handlebars_value * value) {
    return ++value->refcount;
}

static inline int handlebars_value_delref(struct handlebars_value * value) {
    --value->refcount;
    if( value->refcount <= 0 ) {
        handlebars_talloc_free(value);
        return 0;
    }
    return value->refcount;
}

static inline int handlebars_value_refcount(struct handlebars_value * value) {
    return value->refcount;
}

static inline short handlebars_value_is_scalar(struct handlebars_value * value) {
    switch( value->type ) {
        case HANDLEBARS_VALUE_TYPE_NULL:
        case HANDLEBARS_VALUE_TYPE_BOOLEAN:
        case HANDLEBARS_VALUE_TYPE_FLOAT:
        case HANDLEBARS_VALUE_TYPE_INTEGER:
        case HANDLEBARS_VALUE_TYPE_STRING:
            return 1;
        default:
            return 0;
    }
}

enum handlebars_value_type handlebars_value_get_type(struct handlebars_value * value);
struct handlebars_value * handlebars_value_map_find(struct handlebars_value * value, const char * key, size_t len);
struct handlebars_value * handlebars_value_array_find(struct handlebars_value * value, size_t index);
const char * handlebars_value_get_strval(struct handlebars_value * value);
size_t handlebars_value_get_strlen(struct handlebars_value * value);
short handlebars_value_get_boolval(struct handlebars_value * value);
long handlebars_value_get_intval(struct handlebars_value * value);
double handlebars_value_get_floatval(struct handlebars_value * value);

char * handlebars_value_expression(void * ctx, struct handlebars_value * value, short escape);

struct handlebars_value * handlebars_value_ctor(void * ctx);
struct handlebars_value * handlebars_value_from_json_string(void *ctx, const char * json);
struct handlebars_value * handlebars_value_from_json_object(void *ctx, struct json_object *json);

#endif
