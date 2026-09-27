# network

`network` will connect programs, with two ways to talk that either end can use:

- **One way**: send a message and expect nothing back, for positions, chat, and
  events.
- **Two way**: send a request and get a reply, or a failure if it times out or
  the connection drops, for logging in, asking for a score, or buying an item.

> [!NOTE] Not available yet
> `network` is step 11 of stage 1. This page will describe how to use it once it
> exists.

What it is planned to do:

- `Server::New({ .Port = 7777 })` and `Client::New({ .Address = ..., .Port = ... })`.
- Reliable, unreliable, and reliable-but-unordered delivery over UDP, with the
  handshake, acknowledgements, resending, and ordering written from scratch.
- Finding servers on the local network without typing an address.
- A setting that drops and delays packets on purpose, for testing.
- WebSocket for programs running in a browser, which cannot send UDP.

Encryption is not part of the first version; it will come through the TLS the
operating system provides. Until then, use `network` on local networks and with
servers you trust.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#network).
