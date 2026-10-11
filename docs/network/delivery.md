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
large is sent as `ReliableUnordered`. A message is at most the sending end's
`MessageLimit`, 4 MB unless its settings say otherwise; a larger one is not
sent, and a warning is logged. An end that is sent a message larger than its
own limit closes the connection, so both ends should use the same one.

## How fast a connection sends

Each connection finds out how fast the path to the other end can carry data,
and sends that fast and no faster, the way TCP and QUIC do. It keeps a window
of bytes that may be on their way at once. The window starts at ten packets,
doubles with every round trip while everything arrives, then grows by about a
packet each round trip, and halves when a packet is lost, since a lost packet
usually means the path is full. Packets in the window go out spread over the
round trip rather than all at once.

So a game that sends a few small messages a frame never waits, a large
transfer over a fast network uses it, and a slow or crowded link is not
flooded. What does not fit in the window waits for the next update.
Unreliable messages that do not fit are dropped, as they may be. Packets are
kept small enough that networks rarely have to split them.

## How it works

Every packet carries a number and says which of the other end's last 33
packets arrived. The receiver answers every second packet with messages at
once, and any other within 8 milliseconds, often in a packet it was sending
anyway; a packet that arrives out of order is answered at once, so the sender
hears of the gap before the next one. A packet counts as lost once three packets sent after it have arrived,
or once it is later than a round trip after a later one arrived, and the
reliable messages it carried are sent again at once. Any reliable message not
acknowledged in about one and a half round trips is sent again too. The
receiver drops repeats and holds early arrivals until the gaps fill. A packet
that carries nothing but acknowledgements is not acknowledged in turn, and a
quiet connection
sends a keep-alive four times a second, which the other end does answer, so
silence means the other end is gone and the round trip stays measured.

Every packet also carries the sender's clock, in milliseconds since the
connection began. The receiver keeps the smallest difference it has seen
between that clock and its own over the last few seconds, which belongs to the
quickest packet, and stamps each message with when it would have arrived that
quickly: `Message::SentAt`.

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

`Conditions` is also a property of servers and clients, so a program can switch
the trouble on and off while it runs:

```cpp
server.Conditions = poor;
client.Conditions = NetworkConditions {};   // a good network again
```

Held-back packets leave during `Update`, so latency shorter than the time
between updates is rounded up to it.

## Limitations

- A packet lost to noise rather than a full path, such as on a poor wireless
  link, halves the window too, as it does with TCP.
- Unreliable messages are only compared by name, not by connection order with
  reliable ones.
