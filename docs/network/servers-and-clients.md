# Servers and clients

```cpp
Server server = Server::New({
    .Port = 7777,
    .MaximumClients = 16,
    .Name = "Ari's game",
});

server.OnConnected = [](Connection client) {
    Log("{} joined as player {}", client.Address(), client.Index());
};
server.OnDisconnected = [](Connection client, std::string why) {
    Log("player {} left: {}", client.Index(), why);
};

Client client = Client::New({ .Address = "192.168.1.20", .Port = 7777 });
client.OnConnected = [] { Log("connected"); };
client.OnDisconnected = [](std::string why) { Log("disconnected: {}", why); };

window.OnFrame = [&](float) {
    server.Update();
    client.Update();
};
```

## Servers

`Server::New` starts listening on a UDP port. Its settings, in the order they
are written:

| Setting | Default | |
|---|---|---|
| `Port` | 7777 | 0 takes any free port, which `Port()` then gives |
| `MaximumClients` | 16 | More are refused, and are told "the server is full" |
| `Name` | "easyforge" | Shown to programs [finding servers](finding-servers.md) |
| `ThisComputerOnly` | false | Accepts only programs on this computer |
| `Threaded` | false | See [Threads](#threads) |
| `Conditions` | none | Packet loss and delay on purpose, see [Delivery](delivery.md#testing-on-a-poor-network); also a property that can change while the server runs |
| `Timeout` | 5 | Seconds of silence before a client is dropped |

When the port cannot be used, for example because another program has it, the
server tests as false and `Error()` says why.

The first time a program listens for the network, Windows asks the person at
the computer whether to allow it. A server with `ThisComputerOnly` listens only
to this computer, so Windows does not ask; it suits tests, and programs that run
a server and its clients together.

`SendToAll(name, message, delivery)` sends to every client, and `Connections()`
lists them. `Stop()` closes every connection, telling each client, and stops
listening. A server also stops when the last copy of its handle goes away.

## Clients

`Client::New` starts connecting to `Address` (a name such as "localhost", or a
numeric address) and `Port`. Settings: `Address`, `Port`, `Threaded`,
`Conditions`, `Timeout`, and `ConnectTimeout`, the seconds to wait for the
server to answer, 5 by default.

A client tests as false when the address cannot be looked up or no socket can
be opened, and `Error()` says which. Whether the
server answers is told by `OnConnected`, or by `OnDisconnected` with a reason.
Messages and requests sent before the connection is made wait for it.
`IsConnected()` says whether it is connected now, and `Disconnect()` ends the
connection, telling the server. A client connects once; to connect again, make a
new one.

## Connections

A `Connection` is the other end: a client as the server sees it, or the server
as a client sees it, through `client.Server()`. Through it a program can
`Send`, `Request`, and `Disconnect`, and read:

- `Index()`: on a server, a number from 0 below `MaximumClients`, the same for
  as long as the client stays connected and free for another client afterwards.
  It suits arrays of players. On a client, 0.
- `Address()`: the other end's address and port, such as "192.168.1.20:50312".
- `RoundTrip()`: seconds for a packet to get there and back, smoothed.
- `IsConnected()`, which the connection also tests as.

A connection is a handle, cheap to copy and keep. Once the connection closes it
tests as false and does nothing, and a later client in the same slot never takes
over an old handle.

## Why a connection ended

`OnDisconnected` gives the reason as text.

| On the server | |
|---|---|
| "disconnected" | The client left, or the server closed the connection |
| "timed out" | Nothing arrived for `Timeout` seconds |
| "the server stopped" | `Stop()` was called |

| On the client | |
|---|---|
| "disconnected" | `Disconnect()` was called |
| "timed out" | Nothing arrived for `Timeout` seconds |
| "the server stopped" | The server called `Stop()` or ended |
| "the server closed the connection" | The server called `Disconnect()` on this client |
| "the server is full" | The server already had `MaximumClients` |
| "the server did not answer" | Nothing answered within `ConnectTimeout` |

An end's own `Stop()`, `Disconnect()`, and `connection.Disconnect()` run its
`OnDisconnected` handler before they return. Everything else is reported during
`Update`.

## Updating

`Update()` receives what arrived, runs the handlers for it, and sends what is
waiting, including what the handlers sent. Call it once a frame for each server
and client. Handlers run inside `Update`, on the thread that called it, so they
can change the program's state without locks.

## Threads

With `.Threaded = true`, a thread of the server's or client's own updates it
every few milliseconds, and `Update()` does nothing. Messages are answered even
while the program is busy, but the handlers then run on that thread, so state
they share with the rest of the program needs a lock or an atomic.

Every function of `Server`, `Client`, and `Connection` can be called from any
thread.

## Handlers that hold their server

A handler that holds a copy of its own server or client keeps it alive, since
the handler belongs to it. Such a server ends when `Stop()` is called, which
drops its handlers. Capturing by reference, `[&]`, avoids the question.

## Limitations

- A client connects to one server; a program may run several clients.
- Addresses are looked up while `Client::New` waits, which for a name on the
  internet can take a moment.
