# Sharing tables

```cpp
#include <easyforge/bridges/data_network.h>

// on the server
Table game = Table::New();
game.Add("Players");
Share(game, server, Sharing::TwoWay);

// on each client
Table copy = Table::New();
TableShare shared = Share(copy, client);

// later, on a client: its own node, which everyone sees
Node me = copy.Find("Players").Add("Ari", { { "Health", 100 } });
me["Position"] = Vector2 { 3, 4 };
```

The bridge `<easyforge/bridges/data_network.h>` keeps a [data](../data/overview.md)
table the same on a server and its clients. It is header-only, so it compiles
only in programs that link both `easyforge::data` and `easyforge::network`.

## How sharing goes

`Share(table, server, sharing, name)` shares the server's table under a name,
"table" unless given. `Share(table, client, name)` on a client empties the
client's table and fills it with the server's, as soon as the server sends it;
`IsReady()` says when that has happened. From then on every change to the
server's table reaches every client during `Update`: added, removed, renamed,
and moved nodes, property values, and type definitions.

The two ends may start in any order: a server that starts sharing later asks
the clients already connected to send for the table again.

## One way and two ways

With `Sharing::OneWay`, clients only receive. A change a client makes to its
copy stays on that client until the server changes the same thing.

With `Sharing::TwoWay`, each node belongs to whoever added it. A client's
changes to its own nodes reach the server and the other clients; it may add
nodes anywhere, and they are its own. A change to a node it does not own is
undone: the server sends back how the node really is. The server may change
any node, and types are defined by the server only.

On the server, `Owner(node)` gives the client that added a node, or no
connection for the server's own nodes, and `NodesOwnedBy(client)` lists a
client's nodes. When a client leaves, its nodes stay and become the server's
after the next update, so a game removes them in `OnDisconnected` if it wants
them gone:

```cpp
TableShare shared = Share(game, server, Sharing::TwoWay);

server.OnDisconnected = [&](Connection client, std::string) {
    for (Node node : shared.NodesOwnedBy(client))
    {
        node.Remove();
    }
};
```

## Stopping

Sharing goes on for as long as the server or client runs, whether or not the
`TableShare` is kept. `Stop()` ends it on that end, and the table stays as it
is. Several tables can be shared over the same connection under different
names.

## How it works

Each end reads its table's [change list](../data/changes-and-undo.md) once an
update and sends the changes as records: whole new nodes, removals, and single
property values. Nodes are numbered by the server's node identifiers; a client
numbers the nodes it adds below zero until the server tells it their numbers.
The server checks each client record against the node's owner. Changes the
bridge itself applies are not sent back. If more changes happen between two
updates than the table remembers, the server sends every client the whole table
again.

## Limitations

- The bridge reads and changes the table during `Update`, so share tables only
  with servers and clients that are not threaded, or the table would be used by
  two threads at once.
- When two ends add children to the same node at the same moment, the children
  can end up in a different order at each end.
- All the changes of one update travel in one message, so at most 4 MB of them.
- Edits a client undoes are sent as ordinary changes; undo history is not
  shared.
