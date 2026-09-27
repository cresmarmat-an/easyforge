# script

`script` will be the easyforge scripting language: a small language any C or C++
program can embed, with or without the rest of easyforge. Script files end in
`.script`.

> [!NOTE] Not available yet
> `script` is step 8 of stage 1. This page will describe how to use it once it
> exists.

A taste of the planned language:

```script
-- a comment runs to the end of the line

function SomeName(parameter) returns string then
    print("Output: {parameter}")
    return "Output: {parameter}"
end

SomeName("Hello")
```

What it is planned to do:

- Blocks written as a header ending in `then`, closed by `end`, with keywords in
  whole words: `function`, `variable`, `constant`, `nothing`.
- Optional type annotations, checked before the script runs.
- Values placed into text with braces: `"Score: {score}"`.
- `spawn`, `wait`, and `yield` for work that goes on across frames.
- A C++ interface and a plain C interface to run scripts, call their functions,
  and give them functions of your own, with limits on memory and instructions
  for scripts you do not trust.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#script).
