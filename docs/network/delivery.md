# Delivery

```cpp
client.Send("Chat", { { "Text", "hello" } });                                    // reliable, in order
client.Send("Position", { { "X", x }, { "Y", y } }, Delivery::Unreliable);      // may be lost
client.Send("Achievement", { { "Name", "First win" } }, Delivery::ReliableUnordered);
```

## Three ways

| Delivery | Arrives | Use for |
|---|---|---|
| `Reliable` (the default) | Once, in the order sent | Chat, commands, anything that must not be lost |
| `Unreliable` | Maybe; an older message never follows a newer one of the same name | Positions and other state sent many times a second |
| `ReliableUnordered` | Once, possibly before messages sent earlier | Events that matter but do not depend on each other |

Order holds between the reliable messages of one connection, whatever their
names. Unreliable messages are compared by name: a late "Position" is dropped
once a newer "Position" has arrived, while a "Score" sent in between is not
affected.

Reliable messages wait for each other: one that was lost holds back the ones
after it until it is sent again and arrives. Unreliable messages wait for
nothing, which is why state sent every frame should be unreliable.

## Large messages

A message larger than about 1000 bytes is split into pieces that travel
reliably and are put together at the other end. An unreliable message that
large is sent as `ReliableUnordered`. A message is at most 4 MB; a larger one
is not sent, and a warning is logged.

## The send rate

Each connection sends at most 1 MB a second, and what does not fit waits for
the next update. Unreliable messages that do not fit are dropped, as they may
be. Packets are kept small enough that networks rarely have to split them.

## How it works

Every packet carries a number and says which of the other end's last 33
packets arrived. A reliable message is sent again when the packet carrying it
is not acknowledged in about one and a half round trips. The receiver drops
repeats and holds early arrivals until the gaps fill. A packet goes out at
least four times a second even with nothing to say, so silence means the other
end is gone.

## Testing on a poor network

```cpp
NetworkConditions poor { .Loss = 0.1f, .Latency = 0.05f, .Jitter = 0.02f };

Server server = Server::New({ .Port = 7777, .Conditions = poor });
Client client = Client::New({ .Address = "127.0.0.1", .Port = 7777, .Conditions = poor });
```

`Conditions` puts trouble into everything an end sends: `Loss` drops that share
of packets, `Latency` holds each one back for that many seconds, and `Jitter`
adds or takes up to that many more at random, which also puts packets out of
order. Give both ends the same conditions to test a poor network in both
directions. The last word on a closing connection is sent without them.

Held-back packets leave during `Update`, so latency shorter than the time
between updates is rounded up to it.

## Limitations

- There is no congestion control beyond the fixed send rate.
- Unreliable messages are only compared by name, not by connection order with
  reliable ones.
