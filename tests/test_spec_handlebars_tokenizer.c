
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <check.h>
#include <stdio.h>
#include <talloc.h>

#if defined(HAVE_JSON_C_JSON_H)
#include "json-c/json.h"
#include "json-c/json_object.h"
#include "json-c/json_tokener.h"
#elif defined(HAVE_JSON_JSON_H)
#include "json/json.h"
#include "json/json_object.h"
#include "json/json_tokener.h"
#endif

#include "handlebars.h"
#include "handlebars_context.h"
#include "handlebars_token.h"
#include "handlebars_token_list.h"
#include "handlebars_token_printer.h"
#include "handlebars_utils.h"
#include "handlebars.tab.h"
#include "handlebars.lex.h"
#include "utils.h"

struct tokenizer_test_tokens {
    char * name;
    char * text;
};

struct tokenizer_test {
    char * description;
    char * it;
    char * tmpl;
    struct handlebars_token_list * expected;
    size_t expected_len;
    TALLOC_CTX * mem_ctx;
};

static struct tokenizer_test * tests;
static size_t tests_len = 0;
static size_t tests_size = 0;

static void loadSpecTestExpected(struct tokenizer_test * test, json_object * object)
{
    struct json_object * array_item = NULL;
    int array_len = 0;
    json_object * cur = NULL;
    struct tokenizer_test_tokens * expected_token = NULL;
    int token_int = -1;
    const char * name = NULL;
    const char * text = NULL;
    struct handlebars_token * token = NULL;
    
    // Get number of tokens cases
    array_len = json_object_array_length(object);
    
    // Allocate token array
    // test->expected_len = 0;
    // test->expected_size = array_len + 1;
    // test->expected = calloc(test->expected_size, sizeof(struct tokenizer_test_tokens));
    
    // Allocate token list
    test->expected = handlebars_talloc(NULL, struct handlebars_token_list);
    
    // Iterate over array
    for( int i = 0; i < array_len; i++ ) {
        array_item = json_object_array_get_idx(object, i);
        if( json_object_get_type(array_item) != json_type_object ) {
            fprintf(stderr, "Warning: expected token was not an object\n");
            continue;
        }
        
        // Get name
        cur = json_object_object_get(array_item, "name");
        if( cur && json_object_get_type(cur) == json_type_string ) {
            name = json_object_get_string(cur);
        } else {
            fprintf(stderr, "Warning: expected token name was not a string\n");
            continue;
        }
        
        // Get text
        cur = json_object_object_get(array_item, "text");
        if( cur && json_object_get_type(cur) == json_type_string ) {
            text = json_object_get_string(cur);
        } else {
            fprintf(stderr, "Warning: expected token text was not a string\n");
            continue;
        }
        
        // Convert name to integer T_T
        token_int = handlebars_token_reverse_readable_type(name);
        if( token_int == -1 ) {
            fprintf(stderr, "Warning: failed reverse lookup to int on token name\n");
            continue;
        }
        
        // Make token object
        token = handlebars_token_ctor(token_int, text, strlen(text), test->expected);
        if( token == NULL ) {
            fprintf(stderr, "Warning: failed to allocate token struct\n");
            continue;
        }
        
        // Append
        handlebars_token_list_append(test->expected, token);
        test->expected_len++;
    }
}

static void loadSpecTest(json_object * object)
{
    json_object * cur = NULL;
    
    // Get test
    struct tokenizer_test * test = &(tests[tests_len++]);
    
    // Get description
    cur = json_object_object_get(object, "description");
    if( cur && json_object_get_type(cur) == json_type_string ) {
        test->description = strdup(json_object_get_string(cur));
    }
    
    // Get it
    cur = json_object_object_get(object, "it");
    if( cur && json_object_get_type(cur) == json_type_string ) {
        test->it = strdup(json_object_get_string(cur));
    }
    
    // Get template
    cur = json_object_object_get(object, "template");
    if( cur && json_object_get_type(cur) == json_type_string ) {
        test->tmpl = strdup(json_object_get_string(cur));
    }
    
    // Get expected
    cur = json_object_object_get(object, "expected");
    if( cur && json_object_get_type(cur) == json_type_array ) {
        loadSpecTestExpected(test, cur);
    } else {
        fprintf(stderr, "Warning: Expected was not an array\n");
    }
}

static void loadSpec(char * filename) {
    int error = 0;
    char * data = NULL;
    size_t data_len = 0;
    struct json_object * result = NULL;
    struct json_object * array_item = NULL;
    int array_len = 0;
    
    // Read JSON file
    error = file_get_contents((const char *) filename, &data, &data_len);
    if( error != 0 ) {
        fprintf(stderr, "Failed to read spec file: %s, code: %d\n", filename, error);
        exit(1);
    }
    
    // Parse JSON
    result = json_tokener_parse(data);
    // @todo: parsing errors seem to cause segfaults....
    if( result == NULL ) {
        fprintf(stderr, "Failed so parse JSON\n");
        exit(1);
    }
    
    // Root object should be array
    if( json_object_get_type(result) != json_type_array ) {
        fprintf(stderr, "Root JSON value was not array\n");
        exit(1);
    }
    
    // Get number of test cases
    array_len = json_object_array_length(result);
    
    // Allocate tests array
    tests_size = array_len + 1;
    tests = calloc(tests_size, sizeof(struct tokenizer_test));
    
    // Iterate over array
    for( int i = 0; i < array_len; i++ ) {
        array_item = json_object_array_get_idx(result, i);
        if( json_object_get_type(array_item) != json_type_object ) {
            fprintf(stderr, "Warning: test case was not an object\n");
            continue;
        }
        loadSpecTest(array_item);
    }
}

START_TEST(handlebars_spec_tokenizer)
{
    struct tokenizer_test * test = &tests[_i];
    struct handlebars_context * ctx = handlebars_context_ctor();
    
    ctx->tmpl = test->tmpl;
    
    // Prepare token list
    struct handlebars_token_list * actual = handlebars_token_list_ctor(ctx);
    size_t actual_len = 0;
    struct handlebars_token * token = NULL;
    
    // Run
    YYSTYPE yylval_param;
    YYLTYPE yylloc_param;
    int token_int = 0;
    do {
        token_int = handlebars_yy_lex(&yylval_param, &yylloc_param, ctx->scanner);
        if( token_int == 0 ) break;
        YYSTYPE * lval = handlebars_yy_get_lval(ctx->scanner);
        
        // Make token object
        char * text = (lval->text == NULL ? "" : lval->text);
        token = handlebars_token_ctor(token_int, text, strlen(text), actual);
        
        // Append
        handlebars_token_list_append(actual, token);
        actual_len++;
    } while( token );
    
    // Convert to string
    char * expected_str = handlebars_token_list_print(test->expected);
    char * actual_str = handlebars_token_list_print(actual);
    
    ck_assert_str_eq(expected_str, actual_str);
    
    handlebars_context_dtor(ctx);
}
END_TEST

Suite * parser_suite(void)
{
    const char * title = "Handlebars Tokenizer Spec";
    Suite * s = suite_create(title);
    
    TCase * tc_handlebars_spec_tokenizer = tcase_create(title);
    // tcase_add_checked_fixture(tc_ ## name, setup, teardown);
    tcase_add_loop_test(tc_handlebars_spec_tokenizer, handlebars_spec_tokenizer, 0, tests_len - 1);
    suite_add_tcase(s, tc_handlebars_spec_tokenizer);
    
    return s;
}

int main(void)
{
    int number_failed;
    Suite * s;
    SRunner * sr;
    char * spec_filename;
    
    // Load the spec
    spec_filename = getenv("handlebars_tokenizer_spec");
    loadSpec(spec_filename);
    fprintf(stderr, "Loaded %lu test cases\n", tests_len);
    
    s = parser_suite();
    sr = srunner_create(s);
//#if defined(_WIN64) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN32__)
    srunner_set_fork_status(sr, CK_NOFORK);
//#endif
    //runner_set_log(sr, "test_spec_handlebars_tokenizer.log");
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}