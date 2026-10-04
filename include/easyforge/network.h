#pragma once

// Everything in the easyforge network library: servers and clients that send
// one-way messages and two-way requests over UDP, reliably or not, with
// simulated loss and delay for testing and finding servers on the local network.

#include <easyforge/core.h>

#include <easyforge/network/Client.h>
#include <easyforge/network/Connection.h>
#include <easyforge/network/Message.h>
#include <easyforge/network/Server.h>
