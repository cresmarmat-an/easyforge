# File paths

Every easyforge function that takes or gives a file path uses UTF-8 text, on
every platform, so files and folders named in any language work:

```cpp
Table save = Table::Load("Spielstände/Café.tree");
scripts.RunFile("脚本/main.script");
```

When a program opens files itself, `PathFromText` and `TextFromPath` in
`<easyforge/core/Paths.h>` turn that text into a `std::filesystem::path` and
back:

```cpp
#include <easyforge/core/Paths.h>

std::ifstream file(PathFromText("Spielstände/Café.tree"), std::ios::binary);
std::string shown = TextFromPath(std::filesystem::current_path());
```

| Written | Result |
|---|---|
| `PathFromText(text)` | A `std::filesystem::path` from UTF-8 text |
| `TextFromPath(path)` | UTF-8 text with forward slashes between the parts |

On Windows, a `std::filesystem::path` made straight from a `std::string`, and a
file stream opened with one, read the text in the system's code page, so a
name such as `Café` or `文字` names a different file or none. These two
functions avoid that.

Neither function throws. Bytes that are not valid UTF-8, such as a name cut in
the middle of a character, become the replacement character `�`, and so do
parts of a Windows name that are not valid UTF-16. The standard library's own
conversions throw in both cases.

## Limitations

- Paths longer than 260 characters on Windows work only where the system has
  long paths turned on.
