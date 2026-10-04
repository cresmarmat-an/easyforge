# Finding servers

```cpp
std::vector<FoundServer> servers = FindServers({ .Port = 7777, .Seconds = 0.5f });

for (const FoundServer& found : servers)
{
    Log("{} at {}:{}, {} of {} players", found.Name, found.Address, found.Port, found.Clients, found.MaximumClients);
}

if (!servers.empty())
{
    Client client = Client::New({ .Address = servers[0].Address, .Port = servers[0].Port });
}
```

`FindServers` asks every computer on the local network, this one included,
whether a server listens on the port, and waits `Seconds` for the answers. It
returns the servers that answered, with the `Name` from their settings, their
address and port, and how many clients they have and allow.

With `.ThisComputerOnly = true` it asks only this computer, which finds servers
started with `ThisComputerOnly` too.

A server answers while it updates, so a server in the same program that is not
threaded cannot answer while `FindServers` waits; in practice the server runs in
another program or with `.Threaded = true`.

## Limitations

- `FindServers` waits for the whole time it is given.
- It asks by broadcast, which reaches the local network only, not the internet
  or networks beyond a router.
- On a computer with several networks, the broadcast may reach only the one
  Windows prefers.
