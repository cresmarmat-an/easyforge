#pragma once

#include <string>
#include <utility>
#include <variant>

#include <easyforge/core/Export.h>

namespace easyforge
{
    // Why something failed, as text a person can read.
    class Failure
    {
    public:
        explicit Failure(std::string message) : Text(std::move(message)) {}

        const std::string& Message() const noexcept { return Text; }

    private:
        std::string Text;
    };

    namespace internal
    {
        [[noreturn]] EASYFORGE_CORE_EXPORT void StopOnFailedResult(const std::string& error);
        EASYFORGE_CORE_EXPORT void LogFailure(const std::string& error);

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
    //
    // A result that is never looked at hides a failure, so the compiler warns
    // about one left unused. Where logging the error is all a program would do:
    //
    //     controls.SaveBindings("bindings.txt").LogIfFailed();
    template <typename Type = void>
    class [[nodiscard]] Result
    {
    public:
        Result(Type value) : Stored(std::in_place_index<0>, std::move(value)) {}
        Result(Failure failure) : Stored(std::in_place_index<1>, std::move(failure)) {}

        bool Succeeded() const noexcept { return Stored.index() == 0; }
        explicit operator bool() const noexcept { return Succeeded(); }

        // Empty when it succeeded.
        const std::string& Error() const noexcept
        {
            if (const Failure* failure = std::get_if<1>(&Stored))
            {
                return failure->Message();
            }
            return internal::NoError();
        }

        // Logs the error as an error when it failed. Gives whether it succeeded.
        bool LogIfFailed() const
        {
            if (!Succeeded())
            {
                internal::LogFailure(Error());
            }
            return Succeeded();
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
    class [[nodiscard]] Result<void>
    {
    public:
        Result() = default;
        Result(Failure failure) : Problem(std::move(failure)), Failed(true) {}

        bool Succeeded() const noexcept { return !Failed; }
        explicit operator bool() const noexcept { return Succeeded(); }

        const std::string& Error() const noexcept { return Failed ? Problem.Message() : internal::NoError(); }

        // Logs the error as an error when it failed. Gives whether it succeeded.
        bool LogIfFailed() const
        {
            if (Failed)
            {
                internal::LogFailure(Problem.Message());
            }
            return !Failed;
        }

    private:
        Failure Problem { std::string() };
        bool Failed = false;
    };
}
