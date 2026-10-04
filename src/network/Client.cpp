#include <easyforge/network/Client.h>

#include "Endpoint.h"

namespace easyforge
{
    using internal::networking::Endpoint;

    Client::Client() : Client(std::shared_ptr<Endpoint>())
    {
    }

    Client::Client(std::shared_ptr<Endpoint> state)
        : Conditions(state.get(),
              [](const void* owner) {
                  return owner ? static_cast<const Endpoint*>(owner)->Read(&Endpoint::Conditions) : NetworkConditions {};
              },
              [](void* owner, const NetworkConditions& value) {
                  if (owner)
                  {
                      static_cast<Endpoint*>(owner)->Write(&Endpoint::Conditions, value);
                  }
              }),
          OnConnected(state.get(),
              [](const void* owner) {
                  return owner ? static_cast<const Endpoint*>(owner)->Read(&Endpoint::ClientConnected) : std::function<void()>();
              },
              [](void* owner, const std::function<void()>& value) {
                  if (owner)
                  {
                      static_cast<Endpoint*>(owner)->Write(&Endpoint::ClientConnected, value);
                  }
              }),
          OnDisconnected(state.get(),
              [](const void* owner) {
                  return owner ? static_cast<const Endpoint*>(owner)->Read(&Endpoint::ClientDisconnected)
                               : std::function<void(std::string)>();
              },
              [](void* owner, const std::function<void(std::string)>& value) {
                  if (owner)
                  {
                      static_cast<Endpoint*>(owner)->Write(&Endpoint::ClientDisconnected, value);
                  }
              }),
          State(std::move(state))
    {
    }

    Client Client::New(const ClientSettings& settings)
    {
        return Client(Endpoint::StartClient(settings));
    }

    Client::Client(const Client& other) : Client(other.State)
    {
    }

    Client& Client::operator=(const Client& other)
    {
        if (this != &other)
        {
            State = other.State;
            RebindProperties();
        }
        return *this;
    }

    Client::~Client() = default;

    void Client::RebindProperties()
    {
        Conditions.Rebind(State.get());
        OnConnected.Rebind(State.get());
        OnDisconnected.Rebind(State.get());
    }

    Client::operator bool() const
    {
        return State && State->Error().empty();
    }

    std::string Client::Error() const
    {
        return State ? State->Error() : std::string("no client was started");
    }

    bool Client::IsConnected() const
    {
        return State && State->ClientIsConnected();
    }

    Connection Client::Server() const
    {
        return State ? State->ServerConnection() : Connection();
    }

    void Client::OnMessage(std::string_view name, std::function<void(const Message&)> handler) const
    {
        if (!State)
        {
            return;
        }
        internal::networking::MessageHandler wrapped;
        if (handler)
        {
            wrapped = [handler = std::move(handler)](Connection, const Message& message) { handler(message); };
        }
        State->SetMessageHandler(name, std::move(wrapped));
    }

    void Client::OnRequest(std::string_view name, std::function<Result<Message>(const Message&)> handler) const
    {
        if (!State)
        {
            return;
        }
        internal::networking::RequestHandler wrapped;
        if (handler)
        {
            wrapped = [handler = std::move(handler)](Connection, const Message& message) { return handler(message); };
        }
        State->SetRequestHandler(name, std::move(wrapped));
    }

    void Client::Send(std::string_view name, const Message& message, Delivery delivery) const
    {
        if (State)
        {
            State->Send(0, State->ClientGeneration(), name, message, delivery);
        }
    }

    void Client::Request(std::string_view name, const Message& message, std::function<void(const Reply&)> callback,
        const RequestSettings& settings) const
    {
        if (State)
        {
            State->Request(0, State->ClientGeneration(), name, message, std::move(callback), settings);
        }
    }

    float Client::RoundTrip() const
    {
        return State ? State->RoundTripOf(0, State->ClientGeneration()) : 0.0f;
    }

    void Client::Update() const
    {
        if (State)
        {
            State->Update();
        }
    }

    void Client::Disconnect() const
    {
        if (State)
        {
            State->Stop();
        }
    }

    std::size_t Client::AfterUpdate(std::function<void()> handler) const
    {
        return State ? State->AddAfterUpdate(std::move(handler)) : 0;
    }

    void Client::RemoveAfterUpdate(std::size_t handler) const
    {
        if (State)
        {
            State->RemoveAfterUpdate(handler);
        }
    }
}
