# Testing

`core` includes the small test framework that easyforge's own tests use. You can
use it for your program's tests too.

```cpp
// VectorTests.cpp
#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

EASYFORGE_TEST(LengthOfThreeFour)
{
    EASYFORGE_EXPECT_EQUAL(Length(Vector2 { 3.0f, 4.0f }), 5.0f);
}

EASYFORGE_TEST(NormalizeKeepsDirection)
{
    Vector3 direction = Normalize(Vector3 { 0.0f, 0.0f, 9.0f });
    EASYFORGE_EXPECT_NEAR(direction, (Vector3 { 0.0f, 0.0f, 1.0f }), 0.00001f);
}
```

```cpp
// Main.cpp
#include <easyforge/core/Testing.h>

int main(int argumentCount, char** arguments)
{
    return easyforge::RunTests(argumentCount, arguments);
}
```

```cmake
add_executable(my_tests Main.cpp VectorTests.cpp)
target_link_libraries(my_tests PRIVATE easyforge::core)

enable_testing()
add_test(NAME my_tests COMMAND my_tests)
```

## Writing tests

| Written | What it does |
|---|---|
| `EASYFORGE_TEST(Name) { ... }` | Declares a test. The name must be unique in its file and usable in a C++ name |
| `EASYFORGE_EXPECT(condition)` | Records a failure when the condition is false, and carries on |
| `EASYFORGE_EXPECT_EQUAL(actual, expected)` | Records a failure when the two are not equal, showing both values |
| `EASYFORGE_EXPECT_NEAR(actual, expected, tolerance)` | Records a failure unless `NearlyEqual` is true. Works with numbers, vectors, quaternions, matrices, and colors |
| `EASYFORGE_REQUIRE(condition)` | Records a failure and ends the test when the condition is false |

Use `EASYFORGE_REQUIRE` when the rest of the test cannot run, for example before
reading the value of a [Result](results.md) or an element of a list whose size
you are not sure of.

When a macro argument contains a comma outside parentheses, such as a vector
written with braces, wrap it in parentheses:
`EASYFORGE_EXPECT_EQUAL(position, (Vector2 { 1.0f, 2.0f }))`.

Failure messages show values that `std::format` can print, which includes
numbers, text, and easyforge's vectors, quaternions, rectangles, and colors.

## Running tests

`RunTests` runs every test and returns 0 when all of them pass, so `main` can
return it. It prints one line per test and a summary:

```
[pass] LengthOfThreeFour
[pass] NormalizeKeepsDirection

2 passed, 0 failed
```

A failed check is listed under its test with the file, the line, and both
values. If `Normalize` were broken, the output would read:

```
[fail] NormalizeKeepsDirection
       VectorTests.cpp(15): expected direction to be near (Vector3 { 0.0f, 0.0f, 1.0f }), but it was (0, 0, 2) instead of (0, 0, 1)
```

| Argument | Effect |
|---|---|
| a word, such as `Vector` | Runs only tests whose names contain it. Several words run tests matching any of them |
| `--list` | Prints the names of the tests instead of running them |

Running with a filter that matches no test counts as a failure, so a typo in a
test name cannot pass silently.

## Limitations

- There are no fixtures or setup functions; use ordinary functions and local
  variables.
- Tests run one after another on one thread. Tests in one file run in the order
  they are written; the order of the files is up to the linker.
- A crash stops the whole run, and a test that never returns stops it too.
- An exception that escapes a test is caught and reported as a failure.
