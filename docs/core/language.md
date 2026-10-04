# The language reader

The easyforge script language and shader language share one syntax, and `core`
holds the reader for it: a tokenizer, a syntax tree, and a parser. The `script`
and `graphics` libraries use it, and so can tools such as editors, formatters,
or checkers that want to understand easyforge source.

```cpp
#include <easyforge/core/language/Syntax.h>

using namespace easyforge::language;

SyntaxTree tree = Parse(R"(
function SomeName(parameter) returns string then
    print("Output: {parameter}")
    return "Output: {parameter}"
end
)");

if (!tree)
{
    for (const Problem& problem : tree.Problems)
    {
        Log(Describe(problem, "greet.script"));   // "greet.script:3:5: ..."
    }
}
```

Everything is in the `easyforge::language` namespace, since its names, such as
`Token` and `Statement`, are common words.

## Tokens

`Tokenize(source, problems)` splits text into `Token`s: names, keywords,
numbers, text in quotes, color codes such as `#FF8000`, and symbols. Comments
are left out: `--` runs to the end of the line, and `--[[` runs to the next
`]]`, even inside a line. Each token has its `Spelling`, its `Where` (line and
column, from 1), and whether it `StartsLine`. The last token is always
`EndOfFile`. A UTF-8 byte order mark at the very start, which some editors
write, is skipped.

`IsKeyword(word)` tells whether a word is reserved: `function`, `returns`,
`then`, `end`, `variable`, `constant`, `value`, `if`, `else`, `while`, `for`,
`in`, `to`, `return`, `break`, `continue`, `and`, `or`, `not`, `true`, `false`,
`nothing`, `type`, `import`, `try`, `catch`, `spawn`, `wait`, and `yield`.

## The syntax tree

`Parse(source)` gives a `SyntaxTree`: its `Statements` and its `Problems`. It
tests as true when there are no problems. After a problem the parser skips to
the next line and carries on, so one pass finds every problem it can.

A `Statement` has a `Kind` (`Variable`, `Constant`, `Value`, `Function`, `If`,
`While`, `For`, `Return`, `Break`, `Continue`, `Type`, `Import`, `Try`, `Spawn`,
`Wait`, `YieldControl`, `Assignment`, or `Expression`) and the members that
kind uses, described next to `Statement` in the header. An `Expression` is the
same: a `Kind` (`Number`, `Text`, `ColorCode`, `Boolean`, `Nothing`, `Name`,
`Unary`, `Binary`, `Call`, `Member`, `Index`, `List`, `Table`, or `Function`) and
its members. Written types such as `list of number` are `TypeName`s.

Text with `{...}` parts is split while parsing: `"Score: {score}"` becomes the
pieces `"Score: "` and `""` around the expression `score`. `{{` and `}}` are
literal braces, and `\n`, `\t`, `\"`, and `\\` are escapes.

### How expressions group

From loosest to tightest: `or`; `and`; `not`; comparisons, which cannot be
chained; `+` and `-`; `*`, `/`, and `%`; a leading `-`; `^`, which groups to the
right, so `-2 ^ 2` is -4; and calls, indexes, and `.`. A call or index has to
start on the same line as what it applies to, so a line beginning with `(` is
never read as part of the line before it.

## Limitations

- The reader checks syntax only. Whether names exist and types match is up to
  each language: the shader compiler in `graphics`, and
  [`script`](../script/overview.md).
- Source is read as UTF-8; names may contain any letters beyond ASCII, but
  columns count bytes, not characters.
