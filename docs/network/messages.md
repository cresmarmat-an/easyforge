# Messages

```cpp
client.Send("Move", {
    { "Position", Vector2 { 10, 20 } },
    { "Running", true },
    { "Name", "Ari" },
});

server.OnMessage("Move", [](Connection from, const Message& message) {
    Vector2 position = message["Position"];
    bool running = message["Running"];
    std::string name = message["Name"];
});
```

Every message has a name, which picks the handler that receives it, and a
`Message`: named values in the order they were given. A message with no values
is written `{}` or left out: `client.Send("Ready")`.

## Values

A `MessageValue` holds one of:

| Kind | From |
|---|---|
| `Nothing` | `MessageValue()`, or a name the message does not have |
| `Boolean` | `bool` |
| `Integer` | any whole number type, kept as 64 bits |
| `Number` | `float` or `double`, kept as a `double` |
| `Text` | `std::string`, `std::string_view`, or a string literal |
| `Bytes` | `std::vector<std::uint8_t>` |
| `Vector2`, `Vector3`, `Vector4`, `Color` | the `core` types |

Reading gives the type it is read into:

```cpp
int score = message["Score"];
float speed = message["Speed"];
auto health = message["Health"].As<int>();
std::int64_t big = message["Big"].AsInteger();
```

Whole numbers and numbers read as each other. Anything else read as the wrong
kind gives that type's empty value, such as 0, "", or a zero vector, and a name
the message does not have reads as nothing. `Kind()` and `IsNothing()` tell
what a value really is.

## Writing and reading

```cpp
Message reply;
reply["Score"] = 1250;
reply.Set("Rank", 3);

if (reply.Has("Score")) { /* ... */ }
reply.Remove("Rank");
std::vector<std::string> names = reply.Names();   // in order
```

A name given twice keeps the later value. Writing with `[]` adds the name when
it is new, and so does reading with `[]` from a message that is not `const`, as
`std::map` does. The messages handlers receive are `const`, so this matters only
for messages a program builds itself; `Get(name)` reads without adding.

## As bytes

`Encode()` turns a message into bytes and `Message::Decode(bytes)` turns them
back, giving nothing for bytes that are not a message. Both ends use this
format, so it also suits saving a message to a file.

## Limitations

- Names longer than 255 bytes are cut short, and a message holds at most 65535
  values.
- Messages cannot contain other messages; encode one into `Bytes` for that.
- Looking up a name goes through the values in order, which suits the few
  values a message usually has.
