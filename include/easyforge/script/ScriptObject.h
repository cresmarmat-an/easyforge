#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <easyforge/script/ScriptValue.h>

namespace easyforge
{
    // An object of your own that scripts can use: they read its members with a
    // dot, assign to them, and call members that are functions.
    //
    //     class Player : public ScriptObject
    //     {
    //     public:
    //         std::string TypeName() const override { return "Player"; }
    //         ScriptValue Get(std::string_view name) override
    //         {
    //             return name == "Health" ? ScriptValue(Health) : ScriptValue();
    //         }
    //         bool Set(std::string_view name, const ScriptValue& value) override
    //         {
    //             if (name != "Health") return false;
    //             Health = value.As<int>();
    //             return true;
    //         }
    //         int Health = 100;
    //     };
    //
    //     scripts.Define("player", std::make_shared<Player>());
    //
    // A member that is a function is returned from Get as a function value,
    // made with ScriptValue::Function, and the script calls it as
    // player.Heal(10). Objects are called from the thread that runs the engine.
    class ScriptObject
    {
    public:
        virtual ~ScriptObject() = default;

        // The name scripts see for the object's type, in messages and from Type().
        virtual std::string TypeName() const { return "object"; }

        // A member's value, or nothing when the object has no such member.
        virtual ScriptValue Get(std::string_view name) = 0;

        // Assigns to a member. Returning false tells the script the member cannot
        // be assigned.
        virtual bool Set(std::string_view, const ScriptValue&) { return false; }

        // The members, for showing the object; may be left empty.
        virtual std::vector<std::string> MemberNames() const { return {}; }
    };
}
