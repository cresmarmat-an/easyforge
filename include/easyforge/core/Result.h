#pragma once

#include <string>
#include <utility>
#include <variant>

namespace easyforge
{
    // Why something failed, as text a person can read.
    class Failure
    {
    public:
        explicit Failure(std::string message) : Text(std::move(message)) {}

        const std::string& Message() const { return Text; }

    private:
        std::string Text;
    };

    namespace internal
    {
        [[noreturn]] void StopOnFailedResult(const std::string& error);

        inline const std::string& NoError()
        {
            static const std::string empty;
            return empty;
        }
    }

    // Either a value, or the reason there is none:
    //
    //     Result<int> count = CountLines("notes.txt");
    //     if (!count)
    //     {
    //         Log(count.Error());
    //     }
    //
    // Reading the value of a failed result is a mistake in the program: it logs
    // the error and stops the program.
    template <typename Type = void>
    class Result
    {
    public:
        Result(Type value) : Stored(std::in_place_index<0>, std::move(value)) {}
        Result(Failure failure) : Stored(std::in_place_index<1>, std::move(failure)) {}

        bool Succeeded() const { return Stored.index() == 0; }
        explicit operator bool() const { return Succeeded(); }

        // Empty when it succeeded.
        const std::string& Error() const
        {
            if (const Failure* failure = std::get_if<1>(&Stored))
            {
                return failure->Message();
            }
            return internal::NoError();
        }

        Type& Get() & { return *CheckedValue(); }
        const Type& Get() const& { return *CheckedValue(); }
        Type&& Get() && { return std::move(*CheckedValue()); }

        // The value, or `fallback` when it failed.
        Type GetOr(Type fallback) const&
        {
            return Succeeded() ? *std::get_if<0>(&Stored) : std::move(fallback);
        }

        Type& operator*() & { return Get(); }
        const Type& operator*() const& { return Get(); }
        Type* operator->() { return CheckedValue(); }
        const Type* operator->() const { return CheckedValue(); }

    private:
        Type* CheckedValue()
        {
            if (!Succeeded())
            {
                internal::StopOnFailedResult(Error());
            }
            return std::get_if<0>(&Stored);
        }

        const Type* CheckedValue() const
        {
            if (!Succeeded())
            {
                internal::StopOnFailedResult(Error());
            }
            return std::get_if<0>(&Stored);
        }

        std::variant<Type, Failure> Stored;
    };

    // A result with no value: it either succeeded or has a reason it did not.
    //
    //     Result<> saved = table.Save("save.tree");
    template <>
    class Result<void>
    {
    public:
        Result() = default;
        Result(Failure failure) : Problem(std::move(failure)), Failed(true) {}

        bool Succeeded() const { return !Failed; }
        explicit operator bool() const { return Succeeded(); }

        const std::string& Error() const { return Failed ? Problem.Message() : internal::NoError(); }

    private:
        Failure Problem { std::string() };
        bool Failed = false;
    };
}
