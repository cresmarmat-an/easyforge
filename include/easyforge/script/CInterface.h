#ifndef EASYFORGE_SCRIPT_C_INTERFACE_H
#define EASYFORGE_SCRIPT_C_INTERFACE_H

/*
 * The script engine for C, and for other languages that can call C.
 *
 *     easyforge_script* script = easyforge_script_new();
 *     if (!easyforge_script_run_file(script, "greet.script"))
 *     {
 *         puts(easyforge_script_error(script));
 *     }
 *     easyforge_script_push_text(script, "Hello");
 *     if (easyforge_script_call(script, "SomeName"))
 *     {
 *         puts(easyforge_script_result_text(script));
 *     }
 *     easyforge_script_free(script);
 *
 * Functions that can fail return 1 when they worked and 0 when they did not;
 * easyforge_script_error then says why. Text returned by these functions stays
 * valid until the next call on the same engine.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct easyforge_script easyforge_script;

    /* The arguments of a call into a function of your own. */
    typedef struct easyforge_script_arguments easyforge_script_arguments;

    /* The kinds of value results and arguments can be. Lists, tables, functions,
       and objects are all OTHER; read them as text. */
    enum easyforge_script_kind
    {
        EASYFORGE_SCRIPT_NOTHING = 0,
        EASYFORGE_SCRIPT_BOOLEAN = 1,
        EASYFORGE_SCRIPT_NUMBER = 2,
        EASYFORGE_SCRIPT_TEXT = 3,
        EASYFORGE_SCRIPT_OTHER = 4
    };

    /* An engine without limits, or with a memory limit in bytes and an
       instruction limit for each run or call; zero is no limit. */
    easyforge_script* easyforge_script_new(void);
    easyforge_script* easyforge_script_new_limited(size_t memory_limit, unsigned long long instruction_limit);
    void easyforge_script_free(easyforge_script* script);

    int easyforge_script_run(easyforge_script* script, const char* source, const char* name);
    int easyforge_script_run_file(easyforge_script* script, const char* path);

    /* Moves functions started with spawn along; call it once a frame. */
    int easyforge_script_update(easyforge_script* script, float delta_seconds);

    /* Why the last call that failed did. */
    const char* easyforge_script_error(const easyforge_script* script);

    /* Calling a script's function: push each argument, then call. */
    void easyforge_script_push_nothing(easyforge_script* script);
    void easyforge_script_push_boolean(easyforge_script* script, int value);
    void easyforge_script_push_number(easyforge_script* script, double value);
    void easyforge_script_push_text(easyforge_script* script, const char* value);
    int easyforge_script_call(easyforge_script* script, const char* function);

    /* The result of the last run or call. */
    int easyforge_script_result_kind(const easyforge_script* script);
    int easyforge_script_result_boolean(const easyforge_script* script);
    double easyforge_script_result_number(const easyforge_script* script);

    /* Any result as text, as print shows it. */
    const char* easyforge_script_result_text(easyforge_script* script);

    /* A function of your own that scripts can call. `data` is given back to it. */
    typedef void (*easyforge_script_function)(easyforge_script_arguments* call, void* data);
    void easyforge_script_define(easyforge_script* script, const char* name, easyforge_script_function function, void* data);
    void easyforge_script_define_number(easyforge_script* script, const char* name, double value);
    void easyforge_script_define_text(easyforge_script* script, const char* name, const char* value);

    /* Inside such a function: the arguments, counting from 0. */
    int easyforge_script_argument_count(const easyforge_script_arguments* call);
    int easyforge_script_argument_kind(const easyforge_script_arguments* call, int index);
    int easyforge_script_argument_boolean(const easyforge_script_arguments* call, int index);
    double easyforge_script_argument_number(const easyforge_script_arguments* call, int index);
    const char* easyforge_script_argument_text(easyforge_script_arguments* call, int index);

    /* What the function gives back, or why it fails. Without either, it gives
       back nothing. */
    void easyforge_script_return_boolean(easyforge_script_arguments* call, int value);
    void easyforge_script_return_number(easyforge_script_arguments* call, double value);
    void easyforge_script_return_text(easyforge_script_arguments* call, const char* value);
    void easyforge_script_fail(easyforge_script_arguments* call, const char* message);

#ifdef __cplusplus
}
#endif

#endif
