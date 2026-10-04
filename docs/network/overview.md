# network

`network` connects programs. A server accepts clients, and either end can talk
to the other in two ways:

- **One way**: send a message and expect nothing back, for positions, chat, and
  events.
- **Two way**: send a request and get a reply, or a failure if it is refused,
  times out, or the connection drops, for logging in, asking for a score, or
  buying an item.

```cpp
#include <chrono>
#include <thread>

#include <easyforge/network.h>

using namespace easyforge;

int main()
{
    // The server.
    Server server = Server::New({ .Port = 7777, .MaximumClients = 16 });
    if (!server)
    {
        Log(server.Error());
        return 1;
    }

    server.OnMessage("Chat", [&](Connection from, const Message& message) {
        Log("{} says {}", from.Address(), message["Text"].AsText());
        server.SendToAll("Chat", message);                                    // one way
    });

    server.OnRequest("GetScore", [](Connection, const Message&) {
        return Message { { "Score", 1250 } };                                 // two way
    });

    // A client, here in the same program.
    Client client = Client::New({ .Address = "127.0.0.1", .Port = 7777 });

    client.OnMessage("Chat", [](const Message& message) {
        std::string text = message["Text"];
        Log("someone said {}", text);
    });

    client.Send("Chat", { { "Text", "hello" } });
    client.Request("GetScore", {}, [](const Reply& reply) {
        if (reply)
        {
            int score = reply.Message["Score"];
            Log("score: {}", score);
        }
    });

    // Each frame, both ends update.
    for (int frame = 0; frame < 300; ++frame)
    {
        server.Update();
        client.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
```

Link `easyforge::network`. It needs only `core`.

## Pages

- [Servers and clients](servers-and-clients.md): starting, connecting,
  connections, disconnecting and why, updating, and threads.
- [Messages](messages.md): the named values a message carries, and how to read
  and write them.
- [Delivery](delivery.md): reliable, unreliable, and unordered messages, large
  messages, the send rate, and testing on a poor network on purpose.
- [Requests](requests.md): asking, answering, refusing, and timeouts.
- [Finding servers](finding-servers.md): servers on the local network, found
  without typing an address.
- [Sharing tables](sharing-tables.md): keeping a `data` table the same on a
  server and its clients, one way or two ways.

Examples:
[09](https://github.com/cresmarmat-an/easyforge-examples/tree/main/09-chat)
is a chat with one-way messages,
[10](https://github.com/cresmarmat-an/easyforge-examples/tree/main/10-lobby)
a lobby built on requests, and
[11](https://github.com/cresmarmat-an/easyforge-examples/tree/main/11-shared-table)
a table shared between a server and its clients. Each runs a server and its
clients in one window, with a switch that drops a tenth of the packets.

## How it works

Everything travels over UDP. The parts that make it dependable are written for
easyforge: a handshake, numbered packets that acknowledge each other,
resending what was lost, putting messages back in order, splitting large
messages, keep-alives and timeouts, a limit on how fast each connection sends,
request numbers so each reply finds its callback, and finding servers by asking
the whole local network at once.

## Limitations

- **No encryption.** Anything sent can be read and changed by whoever is on the
  network between the two ends. Use `network` on local networks and with
  servers you trust. Encryption will come through the TLS the operating system
  provides.
- Windows only for now. Linux, Apple platforms, and Android arrive with their
  stages. In a browser (stage 3) the connection will use WebSocket, since
  browsers cannot send UDP; there, unreliable messages will arrive reliably.
- IPv4 only.
- A message is at most 4 MB, and each connection sends at most 1 MB a second.
- Nothing finds a way through routers (no NAT traversal): a server on the
  internet needs its port forwarded.
