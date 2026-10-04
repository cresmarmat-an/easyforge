# Requests

```cpp
server.OnRequest("Buy", [&](Connection from, const Message& request) -> Result<Message> {
    std::string item = request["Item"];
    if (gold[from.Index()] < Price(item))
    {
        return Failure("not enough gold");
    }
    gold[from.Index()] -= Price(item);
    return Message { { "Gold", gold[from.Index()] } };
});

client.Request("Buy", { { "Item", "Sword" } }, [](const Reply& reply) {
    if (!reply)
    {
        Log("could not buy: {}", reply.Error);
        return;
    }
    int gold = reply.Message["Gold"];
}, { .Timeout = 5.0f });
```

## Asking

`Request(name, message, callback, settings)` sends a request and calls the
callback once, during a later `Update`, with a `Reply`. A reply tests as true
when it was answered, with the answer in `reply.Message`, and as false when it
failed, with the reason in `reply.Error`:

| Error | |
|---|---|
| The handler's text | The handler refused with `Failure("...")` |
| "nothing answers \"Buy\" here" | The other end has no handler for the name |
| "the request timed out" | No answer within `Timeout` seconds (5 by default) |
| "the connection closed" | The connection ended first |
| "not connected" | There was no connection to ask |

Clients ask with `client.Request`, and a server asks a client the same way
through its connection: `connection.Request(...)`.

An answer that comes after the request timed out is ignored.

## Answering

`OnRequest(name, handler)` sets the handler for a name; a later handler for the
same name replaces the earlier one, and `nullptr` removes it. The handler gets
the connection that asked (on a server) and the request, and returns a
`Message` to answer or a `Failure` to refuse. A lambda that returns both needs
`-> Result<Message>` written out, as above.

Requests and replies travel reliably, and replies leave in the order the
requests arrived.

## Limitations

- A handler answers at once, inside `Update`. To answer later, for example
  after reading a file in the background, the handler can answer that the work
  started, and send a message when it is done.
