#include <easyforge/network/Server.h>

#include "Endpoint.h"

namespace easyforge
{
    using internal::networking::Endpoint;

    Server::Server() : Server(std::shared_ptr<Endpoint>())
    {
    }

    Server::Server(std::shared_ptr<Endpoint> state)
        : OnConnected(state.get(),
              [](const void* owner) {
                  return owner ? static_cast<const Endpoint*>(owner)->Read(&Endpoint::ServerConnected)
                               : std::function<void(Connection)>();
              },
              [](void* owner, const std::function<void(Connection)>& value) {
                  if (owner)
                  {
                      static_cast<Endpoint*>(owner)->Write(&Endpoint::ServerConnected, value);
                  }
              }),
          OnDisconnected(state.get(),
              [](const void* owner) {
                  return owner ? static_cast<const Endpoint*>(owner)->Read(&Endpoint::ServerDisconnected)
                               : std::function<void(Connection, std::string)>();
              },
              [](void* owner, const std::function<void(Connection, std::string)>& value) {
                  if (owner)
                  {
                      static_cast<Endpoint*>(owner)->Write(&Endpoint::ServerDisconnected, value);
                  }
              }),
          State(std::move(state))
    {
    }

    Server Server::New(const ServerSettings& settings)
    {
        return Server(Endpoint::StartServer(settings));
    }

    Server::Server(const Server& other) : Server(other.State)
    {
    }

    Server& Server::operator=(const Server& other)
    {
        if (this != &other)
        {
            State = other.State;
            RebindProperties();
        }
        return *this;
    }

    Server::~Server() = default;

    void Server::RebindProperties()
    {
        OnConnected.Rebind(State.get());
        OnDisconnected.Rebind(State.get());
    }

    Server::operator bool() const
    {
        return State && State->Error().empty();
    }

    std::string Server::Error() const
    {
        return State ? State->Error() : std::string("no server was started");
    }

    void Server::OnMessage(std::string_view name, std::function<void(Connection, const Message&)> handler) const
    {
        if (State)
        {
            State->SetMessageHandler(name, std::move(handler));
        }
    }

    void Server::OnRequest(std::string_view name, std::function<Result<Message>(Connection, const Message&)> handler) const
    {
        if (State)
        {
            State->SetRequestHandler(name, std::move(handler));
        }
    }

    void Server::SendToAll(std::string_view name, const Message& message, Delivery delivery) const
    {
        if (State)
        {
            State->SendToAll(name, message, delivery);
        }
    }

    std::vector<Connection> Server::Connections() const
    {
        return State ? State->Connections() : std::vector<Connection>();
    }

    std::uint16_t Server::Port() const
    {
        return State ? State->Port() : 0;
    }

    void Server::Update() const
    {
        if (State)
        {
            State->Update();
        }
    }

    void Server::Stop() const
    {
        if (State)
        {
            State->Stop();
        }
    }

    std::size_t Server::AfterUpdate(std::function<void()> handler) const
    {
        return State ? State->AddAfterUpdate(std::move(handler)) : 0;
    }

    void Server::RemoveAfterUpdate(std::size_t handler) const
    {
        if (State)
        {
            State->RemoveAfterUpdate(handler);
        }
    }
}
