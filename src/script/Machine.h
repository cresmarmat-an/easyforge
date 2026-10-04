#pragma once

// Carries out compiled code: each task has its own registers and calls, so a
// function started with spawn can stop in the middle and carry on later.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Heap.h"

namespace easyforge::internal::scripting
{
    class EngineState;

    struct Frame
    {
        FunctionObject* Function = nullptr;

        // Where the function's registers start in the task's stack.
        std::size_t Base = 0;
        std::size_t Next = 0;

        // Where the caller wants the result.
        std::size_t ResultSlot = 0;
    };

    struct Handler
    {
        // How many calls were running when the try started.
        std::size_t Frames = 0;
        std::uint32_t Catch = 0;
        std::size_t ProblemSlot = 0;
    };

    struct Task
    {
        std::vector<Value> Stack;
        std::vector<Frame> Frames;
        std::vector<Handler> Handlers;

        // Started with spawn, so it may wait.
        bool Spawned = false;
        bool Waiting = false;

        // Seconds left to wait, or below zero to wait for the next update only.
        double WaitLeft = 0.0;

        Value Result;
    };

    enum class Outcome
    {
        Finished,
        Suspended,
        Failed,
    };

    class Machine
    {
    public:
        explicit Machine(EngineState& engine) : Engine(engine) {}

        // Runs the task until it finishes, waits, or fails; a failure's message,
        // with where it happened, is then in Problem.
        Outcome Run(Task& task);

        // For built-in functions: records why they failed, and returns false.
        bool Fail(std::string message);

        // Starts a call of `callee` as the first call of a fresh task.
        bool Begin(Task& task, Value callee, std::span<const Value> arguments);

        std::string Problem;

        // Stops every script at once, past any try: the limits use it.
        bool Fatal = false;

        EngineState& Engine;

    private:
        // Calls the value in `slot` with `count` arguments after it. A script
        // function gets a new frame; anything else runs now and leaves its
        // result in the slot.
        bool CallAt(Task& task, std::size_t slot, std::size_t count);
        bool Invoke(Task& task, std::size_t slot, std::size_t count, TextObject* name);

        // Moves to the innermost try that can catch the failure. False when none can.
        bool Recover(Task& task);

        std::string Where(const Task& task) const;
    };
}
