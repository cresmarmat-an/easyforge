/* Uses the script engine's C interface from C. */

#include <easyforge/script/CInterface.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition)                                                      \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            printf("%s:%d: expected %s\n", __FILE__, __LINE__, #condition);   \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

static void Shout(easyforge_script_arguments* call, void* data)
{
    char buffer[64];
    int* calls = (int*)data;
    ++*calls;
    if (easyforge_script_argument_kind(call, 0) != EASYFORGE_SCRIPT_TEXT)
    {
        easyforge_script_fail(call, "Shout needs a string");
        return;
    }
    snprintf(buffer, sizeof buffer, "%s!", easyforge_script_argument_text(call, 0));
    easyforge_script_return_text(call, buffer);
}

int main(void)
{
    int calls = 0;
    easyforge_script* script = easyforge_script_new();
    easyforge_script_define(script, "Shout", Shout, &calls);
    easyforge_script_define_number(script, "Limit", 3);

    CHECK(easyforge_script_run(script,
        "function Twice(amount) returns number then\n"
        "    return amount * Limit - amount\n"
        "end\n"
        "return Shout(\"hey\")\n",
        "c.script"));
    CHECK(easyforge_script_result_kind(script) == EASYFORGE_SCRIPT_TEXT);
    CHECK(strcmp(easyforge_script_result_text(script), "hey!") == 0);
    CHECK(calls == 1);

    easyforge_script_push_number(script, 21);
    CHECK(easyforge_script_call(script, "Twice"));
    CHECK(easyforge_script_result_kind(script) == EASYFORGE_SCRIPT_NUMBER);
    CHECK(easyforge_script_result_number(script) == 42);

    CHECK(!easyforge_script_run(script, "Shout(1)\n", "c.script"));
    CHECK(strstr(easyforge_script_error(script), "Shout needs a string") != NULL);
    CHECK(!easyforge_script_call(script, "Missing"));

    easyforge_script_free(script);
    puts(failures == 0 ? "C interface: every check passed" : "C interface: some checks failed");
    return failures == 0 ? 0 : 1;
}