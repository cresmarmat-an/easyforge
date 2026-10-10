# Security

Every connection is encrypted and authenticated, with nothing to switch on.
Whoever is on the network between a server and a client sees packets go by,
but cannot read them, change them, send one again, or make one up: any packet
that was touched on the way is dropped. The cryptography is the operating
system's own (CNG on Windows), not easyforge's.

## Proving who the server is

Encryption keeps out whoever is between the two ends, but on its own a client
cannot tell the server it meant from a program that answers at the same
address in its place. A server with an identity can prove who it is:

```cpp
#include <easyforge/network.h>

using namespace easyforge;

// Once, when setting the server up:
NetworkIdentity identity = NetworkIdentity::New();
identity.Save("server.identity");
std::string key = identity.PublicKey();   // 128 hexadecimal digits

// Every time the server starts:
Server server = Server::New({ .Port = 7777, .Identity = NetworkIdentity::Load("server.identity") });

// In the client, with the key written into the program:
Client client = Client::New({ .Address = "play.example.com", .Port = 7777, .ServerKey = key });
```

A client given a `ServerKey` connects only to a server that proves it holds that
identity; otherwise it disconnects with "the server could not prove it is the
one ServerKey names". A client without one accepts whichever server answers,
which is fine on a local network.

| Written | Result |
|---|---|
| `NetworkIdentity::New()` | A new, random identity |
| `NetworkIdentity::Load(path)` | One saved before. Tests as false, with `Error()`, when the file is missing or damaged; a server given it does not start |
| `identity.Save(path)` | Writes it whole, private half included. Keep that file on the server only |
| `identity.PublicKey()` | The text to give clients as `ServerKey` |

## Servers on the open internet

A server reachable from anywhere gets packets from anyone, including packets
with a forged sender. The handshake keeps them from doing harm:

- A connect request must be as large as a full packet, and the server's first
  answer is far smaller, so a forged sender cannot make a server send someone
  else more than it was sent. Requests to find servers on the local network are
  padded the same way.
- The server keeps nothing for a connect request. It answers with a cookie that
  only it can make, tied to the address the request came from, and only a
  client that sends the cookie back gets a place. Forged addresses never get
  that far, so they cannot fill the server's places.
- Each address may try to connect about ten times a second, with bursts of
  twenty, since agreeing on keys costs the server time.
- A connection that sends more unfinished messages than it may hold, 16 MB, is
  closed.

## How it works

The ideas are the ones QUIC and DTLS use.

1. The client sends a connect request, padded to 1200 bytes.
2. The server answers with a cookie: when it runs out, ten seconds later, and an
   HMAC-SHA256 of that and the client's address, made with a secret the server
   chose when it started.
3. The client sends the cookie back with a public key, made for this
   connection only. While no answer comes, it tries again, and every fourth
   try also asks for a new cookie, in case the server no longer takes the old
   one.
4. The server checks the cookie, makes a public key of its own, and both ends
   agree on a shared secret (ECDH on the P-256 curve). From it, HKDF-SHA256
   derives one key for each direction. A server with an identity also signs
   both public keys and both connection numbers (ECDSA on P-256), which the
   client checks against `ServerKey`.
5. Every packet after that is sealed with AES-256-GCM. Its counter, which
   never repeats, is the nonce; the counter and the connection number are
   authenticated with it. A packet that does not open, or whose counter was
   already seen, is dropped.

New keys are made for every connection, so a key found later says nothing
about earlier connections.

Sealing or opening a full packet takes about a microsecond on a desktop
processor, through the processor's own AES instructions. A connection moves
as many messages a second sealed as it would plain.

## Limitations

- Keys are not changed during a connection; one connection may send up to
  2^64 packets, which no connection lives long enough to reach.
- A client cannot prove who it is to the server through `network`; servers
  that need to know their users ask for a name and password, or a token, in a
  request once connected.
- Finding servers on the local network is not encrypted: what it shows, the
  server's name and number of clients, is meant for anyone nearby.
