#include <easyforge/network/Connection.h>

#include "Endpoint.h"

namespace easyforge
{
    bool Connection::IsConnected() const
    {
        std::shared_ptr<internal::networking::Endpoint> endpoint = Endpoint.lock();
        return endpoint && endpoint->IsConnected(Slot, Generation);
    }

    int Connection::Index() const
    {
        return Slot;
    }

    std::string Connection::Address() const
    {
        std::shared_ptr<internal::networking::Endpoint> endpoint = Endpoint.lock();
        return endpoint ? endpoint->AddressOf(Slot, Generation) : std::string();
    }

    float Connection::RoundTrip() const
    {
        std::shared_ptr<internal::networking::Endpoint> endpoint = Endpoint.lock();
        return endpoint ? endpoint->RoundTripOf(Slot, Generation) : 0.0f;
    }

    void Connection::Send(std::string_view name, const Message& message, Delivery delivery) const
    {
        if (std::shared_ptr<internal::networking::Endpoint> endpoint = Endpoint.lock())
        {
            endpoint->Send(Slot, Generation, name, message, delivery);
        }
    }

    void Connection::Request(std::string_view name, const Message& message, std::function<void(const Reply&)> callback,
        const RequestSettings& settings) const
    {
        if (std::shared_ptr<internal::networking::Endpoint> endpoint = Endpoint.lock())
        {
            endpoint->Request(Slot, Generation, name, message, std::move(callback), settings);
        }
    }

    void Connection::Disconnect() const
    {
        if (std::shared_ptr<internal::networking::Endpoint> endpoint = Endpoint.lock())
        {
            endpoint->Disconnect(Slot, Generation);
        }
    }

    bool Connection::operator==(const Connection& other) const
    {
        bool sameEndpoint = !Endpoint.owner_before(other.Endpoint) && !other.Endpoint.owner_before(Endpoint);
        return sameEndpoint && Slot == other.Slot && Generation == other.Generation;
    }
}
