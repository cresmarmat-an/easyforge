#pragma once

#include <concepts>
#include <utility>

namespace easyforge
{
    // A setting of an object that can be read and assigned like a variable:
    //
    //     window.Title = "Notes";
    //     std::string title = window.Title;
    //
    // Reads and writes go to the object that owns the property, which can react
    // to them (a window changes its title bar). A property is not a copy of the
    // value, so `auto title = window.Title;` does not compile; name the type or
    // call Get().
    //
    // Assigning works on a const property too. Objects in easyforge are handles,
    // and a const handle still refers to an object that can change.
    template <typename Value>
    class Property
    {
    public:
        using Reader = Value (*)(const void* owner);
        using Writer = void (*)(void* owner, const Value& value);

        constexpr Property(void* owner, Reader reader, Writer writer) noexcept
            : Owner(owner), Reading(reader), Writing(writer)
        {
        }

        Property(const Property&) = delete;
        Property(Property&&) = delete;

        const Property& operator=(const Value& value) const
        {
            Writing(Owner, value);
            return *this;
        }

        // Copies the other property's value, not which object it belongs to.
        const Property& operator=(const Property& other) const
        {
            return *this = other.Get();
        }

        Value Get() const { return Reading(Owner); }

        operator Value() const { return Get(); }

        // Reads a member of the value: `window.Size->X`.
        class Snapshot
        {
        public:
            explicit Snapshot(Value value) : Held(std::move(value)) {}
            const Value* operator->() const { return &Held; }

        private:
            Value Held;
        };

        Snapshot operator->() const { return Snapshot(Get()); }

        template <typename Other>
        const Property& operator+=(const Other& other) const
            requires requires(const Value& value, const Other& amount) { value + amount; }
        {
            return *this = static_cast<Value>(Get() + other);
        }

        template <typename Other>
        const Property& operator-=(const Other& other) const
            requires requires(const Value& value, const Other& amount) { value - amount; }
        {
            return *this = static_cast<Value>(Get() - other);
        }

        template <typename Other>
        const Property& operator*=(const Other& other) const
            requires requires(const Value& value, const Other& amount) { value * amount; }
        {
            return *this = static_cast<Value>(Get() * other);
        }

        template <typename Other>
        const Property& operator/=(const Other& other) const
            requires requires(const Value& value, const Other& amount) { value / amount; }
        {
            return *this = static_cast<Value>(Get() / other);
        }

        friend bool operator==(const Property& property, const Value& value)
            requires std::equality_comparable<Value>
        {
            return property.Get() == value;
        }

        // Points the property at a different owner. Used by handle classes when
        // they are copied; programs using easyforge do not need it.
        void Rebind(void* owner) noexcept { Owner = owner; }

    private:
        void* Owner;
        Reader Reading;
        Writer Writing;
    };
}
