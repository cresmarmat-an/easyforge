# Values

Every property holds one `DataValue`: nothing, or one of eight kinds of value.

```cpp
DataValue health = 100;
DataValue name = "Ari";
DataValue position = Vector2 { 10, 20 };

int remaining = health.As<int>();
std::string text = std::format("{} has {} health", name, health);   // "Ari has 100 health"
```

| Kind | `DataType` | Made from | Written in a `.tree` file |
|---|---|---|---|
| Nothing | `Nothing` | `DataValue()` | `nothing` |
| True or false | `Boolean` | `bool` | `true`, `false` |
| Whole number | `Integer` | any integer type, kept as 64 bits | `100`, `-7` |
| Number | `Number` | `float` or `double`, kept as `double` | `4.5`, `3.0`, `1e+20` |
| Text | `Text` | `const char*`, `std::string`, `std::string_view` | `"Ari"` |
| Vector | `Vector2`, `Vector3`, `Vector4` | the `core` vectors | `10, 20`, `1, 2, 3` |
| Color | `Color` | `Color` | `#FF8000`, `#15151A80` |

`value.Type()` gives the kind, and `DataTypeName(type)` its name as people read
it, such as `"vector2"`. `value.IsNothing()` is true for nothing. A `char` is not
accepted, so a single letter is not mistaken for a number; write text in double
quotes instead.

## Reading a value as a type

`value.As<Type>()` reads the value as any of `bool`, the integer types, `float`,
`double`, `std::string`, `Vector2`, `Vector3`, `Vector4`, `Color`, or
`DataValue`. There are also named readers: `AsBoolean`, `AsInteger`, `AsNumber`,
`AsText`, `AsVector2`, `AsVector3`, `AsVector4`, and `AsColor`.

- Whole numbers and numbers read as each other. A number read as a whole number
  is rounded to the nearest one.
- Anything else read as the wrong kind gives that kind's empty value: `0`,
  `false`, `""`, a zero vector, or opaque black. Text is never
  parsed into a number.

## As text

`value.ToText()` writes the value the way a `.tree` file does, and
`DataValue::FromText(text, &valid)` reads it back, setting `valid` to false for
text that is not a value.

- A number always has a decimal point or an exponent, so `3.0` stays a number
  and `3` is a whole number.
- Numbers are written with the fewest digits that read back as exactly the same
  number.
- Text is written in double quotes, with `\"`, `\\`, `\n`, `\t`, and `\r` for
  the characters that need them.
- A vector is two to four numbers separated by commas.

`std::format` prints a value as `ToText` does, except that text is printed
without its quotes, so a value can go straight into a sentence.

Two values are equal when they are the same kind and hold the same value:
`DataValue(1) == DataValue(1.0)` is false.
