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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#define HANDLEBARS_HELPERS_PRIVATE

#include "handlebars.h"
#include "handlebars_memory.h"
#include "handlebars_private.h"
#include "handlebars_value_private.h"

#include "handlebars_helpers.h"
#include "handlebars_map.h"
#include "handlebars_stack.h"
#include "handlebars_string.h"
#include "handlebars_value.h"
#include "handlebars_vm.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#include "handlebars_helpers_ht.h"
#pragma GCC diagnostic pop



const size_t HANDLEBARS_OPTIONS_SIZE = sizeof(struct handlebars_options);

#undef CONTEXT
#define CONTEXT HBSCTX(options->vm)

void handlebars_options_deinit(struct handlebars_options * options)
{
    if (options->data) {
        options->data = NULL;
    }
    if (options->hash) {
        handlebars_value_dtor(options->hash);
    }
    if (options->scope) {
        handlebars_value_dtor(options->scope);
    }
    //handlebars_stack_dtor(options->params);
    //handlebars_talloc_free(options->name);
}

struct handlebars_value * handlebars_builtin_each(HANDLEBARS_HELPER_ARGS)
{
    struct handlebars_value * context;
    struct handlebars_string * result_str = handlebars_string_ctor(CONTEXT, HBS_STRL(""));
    short use_data;
    struct handlebars_string * tmp;
    size_t i = 0;
    size_t len;
    HANDLEBARS_VALUE_DECL(rv2);
    HANDLEBARS_VALUE_DECL(index);
    HANDLEBARS_VALUE_DECL(key);
    HANDLEBARS_VALUE_DECL(first);
    HANDLEBARS_VALUE_DECL(last);
    HANDLEBARS_VALUE_DECL(data);
    HANDLEBARS_VALUE_DECL(block_params);
    struct handlebars_map * data_map = NULL;

    use_data = (options->data != NULL);

    if( argc < 1 ) {
        handlebars_throw(CONTEXT, HANDLEBARS_ERROR, "Must pass iterator to #each");
    }

    context = argv[0];

    if( handlebars_value_is_callable(context) ) {
        struct handlebars_options options2 = {0};
        int argc2 = 1;
        //struct handlebars_value * argv2[argc2];
        struct handlebars_value ** argv2 = alloca(sizeof(struct handlebars_value *) * argc2);
        options2.vm = options->vm;
        options2.scope = options->scope;
        argv2[0] = options->scope;
        struct handlebars_value * ret = handlebars_value_call(context, argc2, argv2, &options2, rv2);
        if( !ret ) {
            handlebars_options_deinit(&options2);
            goto whoopsie;
        }
        context = ret;
        handlebars_options_deinit(&options2);
    }

    if( handlebars_value_get_type(context) != HANDLEBARS_VALUE_TYPE_MAP && handlebars_value_get_type(context) != HANDLEBARS_VALUE_TYPE_ARRAY ) {
        use_data = false;
        goto whoopsie;
    }

    if( use_data ) {
        handlebars_value_array(block_params, handlebars_stack_ctor(CONTEXT, 2));

        if( handlebars_value_get_type(options->data) == HANDLEBARS_VALUE_TYPE_MAP ) {
            data_map = handlebars_map_ctor(CONTEXT, handlebars_value_count(options->data) + 4);
            HANDLEBARS_VALUE_FOREACH_KV(options->data, options_key, child) {
                data_map = handlebars_map_update(data_map, options_key, child);
            } HANDLEBARS_VALUE_FOREACH_END();
        } else {
            data_map = handlebars_map_ctor(CONTEXT, 4);
        }
        handlebars_map_addref(data_map);
    }

    len = handlebars_value_count(context);
    if (len <= 0) goto whoopsie;
    len--;

    HANDLEBARS_VALUE_FOREACH_IDX_KV(context, it_index, it_key, it_child) {
        // Disabled for Regressions - Undefined helper context
        // if( it.current->type == HANDLEBARS_VALUE_TYPE_NULL ) {
        //     i++;
        //     continue;
        // }

        if( it_key /*it->value->type == HANDLEBARS_VALUE_TYPE_MAP*/ ) {
            handlebars_value_str(key, it_key);
        } else {
            handlebars_value_integer(key, it_index);
        }

        if( use_data ) {
            if( it_index ) { // @todo zero?
                handlebars_value_integer(index, it_index);
            } else {
                handlebars_value_integer(index, i);
            }
            handlebars_value_boolean(first, i == 0);
            handlebars_value_boolean(last, i == len);

            handlebars_value_array_set(block_params, 0, it_child);
            handlebars_value_array_set(block_params, 1, key);
        }

        data_map = handlebars_map_str_update(data_map, HBS_STRL("index"), index);
        data_map = handlebars_map_str_update(data_map, HBS_STRL("key"), key);
        data_map = handlebars_map_str_update(data_map, HBS_STRL("first"), first);
        data_map = handlebars_map_str_update(data_map, HBS_STRL("last"), last);
        handlebars_value_map(data, data_map);

        tmp = handlebars_vm_execute_program_ex(options->vm, options->program, it_child, data, block_params);
        result_str = handlebars_string_append(HBSCTX(options->vm), result_str, HBS_STR_STRL(tmp));

        handlebars_value_null(data);

        i++;
    } HANDLEBARS_VALUE_FOREACH_END();

whoopsie:
    if( i == 0 ) {
        tmp = handlebars_vm_execute_program(options->vm, options->inverse, options->scope);
        assert(tmp != NULL);
        result_str = handlebars_string_append(HBSCTX(options->vm), result_str, HBS_STR_STRL(tmp));
    }

    handlebars_value_str(rv, result_str);

    if( use_data ) {
        handlebars_map_delref(data_map);
    }
    //handlebars_value_try_delref(context);  // @todo double-check

    HANDLEBARS_VALUE_UNDECL(rv2);
    HANDLEBARS_VALUE_UNDECL(block_params);
    HANDLEBARS_VALUE_UNDECL(data);
    HANDLEBARS_VALUE_UNDECL(last);
    HANDLEBARS_VALUE_UNDECL(first);
    HANDLEBARS_VALUE_UNDECL(key);
    HANDLEBARS_VALUE_UNDECL(index);

    return rv;
}

struct handlebars_value * handlebars_builtin_block_helper_missing(HANDLEBARS_HELPER_ARGS)
{
    struct handlebars_value * context;
    struct handlebars_string * result_str = NULL;
    bool is_zero;

    if( argc < 1 ) {
        goto inverse;
    }

    context = argv[0];
    is_zero = handlebars_value_get_type(context) == HANDLEBARS_VALUE_TYPE_INTEGER && handlebars_value_get_intval(context) == 0;

    if( handlebars_value_get_type(context) == HANDLEBARS_VALUE_TYPE_TRUE ) {
        result_str = handlebars_vm_execute_program(options->vm, options->program, options->scope);
    } else if( handlebars_value_is_empty(context) && !is_zero ) {
inverse:
        result_str = handlebars_vm_execute_program(options->vm, options->inverse, options->scope);
    } else if( handlebars_value_get_type(context) == HANDLEBARS_VALUE_TYPE_ARRAY ) {
        rv = handlebars_vm_call_helper_str(HBS_STRL("each"), argc, argv, options, rv);
        goto done;
    } else {
        // For object, etc
        result_str = handlebars_vm_execute_program(options->vm, options->program, context);
    }

    handlebars_value_str(rv, result_str);

    //handlebars_value_try_delref(context); // @todo double-check

done:
    return rv;
}

struct handlebars_value * handlebars_builtin_helper_missing(HANDLEBARS_HELPER_ARGS)
{
    if( argc != 0 ) {
        handlebars_throw(
            CONTEXT,
            HANDLEBARS_ERROR,
            "Missing helper: \"%.*s\"",
            (int) hbs_str_len(options->name),
            hbs_str_val(options->name)
        );
    }

    return rv;
}

struct handlebars_value * handlebars_builtin_log(HANDLEBARS_HELPER_ARGS)
{
    int i;
    fprintf(stderr, "[INFO] ");
    for (i = 0; i < argc; i++) {
        char *tmp = handlebars_value_dump(argv[i], HBSCTX(options->vm), 0);
        fprintf(stderr, "%s ", tmp);
        handlebars_talloc_free(tmp);
    }
    fprintf(stderr, "\n");
    //fflush(stderr);

    return rv;
}

struct handlebars_value * handlebars_builtin_lookup(HANDLEBARS_HELPER_ARGS)
{
    if( argc < 2 ) {
        handlebars_throw(CONTEXT, HANDLEBARS_ERROR, "lookup requires two parameters");
    }

    struct handlebars_value * context = argv[0];
    struct handlebars_value * field = argv[1];
    enum handlebars_value_type type = handlebars_value_get_type(context);

    if( type == HANDLEBARS_VALUE_TYPE_MAP ) {
        struct handlebars_string * key = handlebars_value_to_string(field, CONTEXT);
        (void) handlebars_value_map_find(context, key, rv);
        handlebars_string_delref(key);
    } else if( type == HANDLEBARS_VALUE_TYPE_ARRAY ) {
        // @todo sscanf?
        if( handlebars_value_get_type(field) == HANDLEBARS_VALUE_TYPE_INTEGER ) {
            rv = handlebars_value_array_find(context, handlebars_value_get_intval(field), rv);
            // handlebars_value_value(rv, result);
        }
    }

    return rv;
}

struct handlebars_value * handlebars_builtin_if(HANDLEBARS_HELPER_ARGS)
{
    struct handlebars_value * conditional = argv[0];
    long program;
    struct handlebars_value * tmp = NULL;
    struct handlebars_string * result_str = NULL;
    HANDLEBARS_VALUE_DECL(rv2);

    if (argc != 1) {
        handlebars_throw(CONTEXT, HANDLEBARS_ERROR, "#if requires exactly one argument");
    }

    if( handlebars_value_is_callable(conditional) ) {
        struct handlebars_options options2 = {0};
        int argc2 = 1;
        //struct handlebars_value * argv2[argc2];
        struct handlebars_value ** argv2 = alloca(sizeof(struct handlebars_value *) * argc2);
        options2.vm = options->vm;
        options2.scope = options->scope;
        argv2[0] = options->scope;
        struct handlebars_value * result = handlebars_value_call(conditional, argc2, argv2, &options2, rv2);
        assert(result != NULL);
        conditional = result;
        handlebars_options_deinit(&options2);
    }

    if( !handlebars_value_is_empty(conditional) ) {
        program = options->program;
    } else if( handlebars_value_get_type(conditional) == HANDLEBARS_VALUE_TYPE_INTEGER &&
            handlebars_value_get_intval(conditional) == 0 &&
            NULL != (tmp = handlebars_value_map_str_find(options->hash, HBS_STRL("includeZero"), rv2)) ) {
        program = options->program;
    } else {
        program = options->inverse;
    }

    result_str = handlebars_vm_execute_program(options->vm, program, options->scope);
    handlebars_value_str(rv, result_str);

    HANDLEBARS_VALUE_UNDECL(rv2);

    return rv;
}

struct handlebars_value * handlebars_builtin_unless(HANDLEBARS_HELPER_ARGS)
{
    struct handlebars_value * conditional;

    if (argc != 1) {
        handlebars_throw(CONTEXT, HANDLEBARS_ERROR, "#unless requires exactly one argument");
    }

    conditional = argv[0];

    handlebars_value_boolean(conditional, conditional ? handlebars_value_is_empty(conditional) : true);

    return handlebars_vm_call_helper_str(HBS_STRL("if"), argc, argv, options, rv);
}

struct handlebars_value * handlebars_builtin_with(HANDLEBARS_HELPER_ARGS)
{
    struct handlebars_string * result_str = NULL;
    struct handlebars_value * context = argv[0];
    HANDLEBARS_VALUE_DECL(block_params);
    HANDLEBARS_VALUE_DECL(rv2);

    if (argc != 1) {
        handlebars_throw(CONTEXT, HANDLEBARS_ERROR, "#with requires exactly one argument");
    }

    if( handlebars_value_is_callable(context) ) {
        struct handlebars_options options2 = {0};
        int argc2 = 1;
        //struct handlebars_value * argv2[argc2];
        struct handlebars_value ** argv2 = alloca(sizeof(struct handlebars_value *) * argc2);
        argv2[0] = options->scope;
        struct handlebars_value * result = handlebars_value_call(context, argc2, argv2, options, rv2);
        context = result;
        handlebars_options_deinit(&options2);
    }

    assert(context != NULL);

    if( handlebars_value_get_type(context) == HANDLEBARS_VALUE_TYPE_NULL ) {
        result_str = handlebars_vm_execute_program(options->vm, options->inverse, context);
    } else {
        handlebars_value_array(block_params, handlebars_stack_ctor(CONTEXT, 2));
        handlebars_value_array_set(block_params, 0, context);

        result_str = handlebars_vm_execute_program_ex(options->vm, options->program, context, options->data, block_params);
    }

    handlebars_value_str(rv, result_str);

    HANDLEBARS_VALUE_UNDECL(rv2);
    HANDLEBARS_VALUE_UNDECL(block_params);

    return rv;
}







static const char * names[] = {
    "blockHelperMissing",
    "helperMissing",
    "each",
    "if",
    "unless",
    "with",
    "log",
    "lookup",
    NULL
};

static handlebars_helper_func builtins[] = {
    &handlebars_builtin_block_helper_missing,
    &handlebars_builtin_helper_missing,
    &handlebars_builtin_each,
    &handlebars_builtin_if,
    &handlebars_builtin_unless,
    &handlebars_builtin_with,
    &handlebars_builtin_log,
    &handlebars_builtin_lookup,
    NULL
};

const char ** handlebars_builtins_names(void)
{
    return names;
}

handlebars_helper_func * handlebars_builtins(void)
{
    return builtins;
}

handlebars_helper_func handlebars_builtins_find(const char * str, unsigned int len)
{
    const struct handlebars_builtin_pair * pair = hbs_builtin_lut_lookup(str, len);
    handlebars_helper_func fn = NULL;
    if( pair ) {
        fn = builtins[pair->pos];
    }
    return fn;
}
