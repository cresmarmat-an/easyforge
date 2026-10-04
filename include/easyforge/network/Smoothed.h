#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <deque>

#include <easyforge/core/Color.h>
#include <easyforge/core/Quaternion.h>
#include <easyforge/core/Scalar.h>
#include <easyforge/core/Vector.h>
#include <easyforge/network/Message.h>

namespace easyforge
{
    // A value another program sends again and again, such as where a player
    // is, shown smoothly.
    //
    // Messages cross a network at uneven times: some late, some bunched up,
    // some lost. Drawing each value as it arrives makes things jump. A Smoothed
    // value is shown a short moment in the past instead, blended between the
    // values sent around then, using when each was sent rather than when it
    // arrived. The delay grows by itself to cover how far apart values are sent
    // and how unevenly they arrive, and shrinks again when the network calms.
    //
    //     Smoothed<Vector2> place;
    //
    //     client.OnMessage("Place", [&](const Message& message) {
    //         place.Add(message["Position"], message);
    //     });
    //
    //     window.OnFrame = [&](float deltaSeconds) {
    //         Vector2 shown = place.Get();
    //     };
    //
    // Works with float, double, Vector2, Vector3, Vector4, Color, and
    // Quaternion; SmoothedAngle does the same for an angle. When values stop
    // coming, the last one is held rather than guessed past. A Smoothed value
    // is used from one thread at a time.
    template <typename Value>
        requires std::same_as<Value, float> || std::same_as<Value, double> || std::same_as<Value, Vector2> ||
                 std::same_as<Value, Vector3> || std::same_as<Value, Vector4> || std::same_as<Value, Color> ||
                 std::same_as<Value, Quaternion>
    class Smoothed
    {
    public:
        Smoothed() = default;
        explicit Smoothed(const Value& start) { Reset(start); }

        // A value the other end sent, stamped with when it sent it.
        void Add(const Value& value, const Message& message) { Add(value, message.SentAt(), NetworkTime()); }

        // A value sent at `sentAt` and arriving `now`, both on NetworkTime's
        // clock. A `sentAt` of 0, as on a message made on this computer, counts
        // as sent now.
        void Add(const Value& value, double sentAt, double now)
        {
            if (!std::isfinite(now))
            {
                return;
            }
            if (!(sentAt > 0.0) || !std::isfinite(sentAt))
            {
                sentAt = now;
            }
            // Too late: the moment it belongs to has been shown already.
            if (Started && sentAt < LastShown)
            {
                return;
            }

            auto place = std::upper_bound(Samples.begin(), Samples.end(), sentAt,
                [](double time, const Sample& sample) { return time < sample.Time; });
            if (place != Samples.begin() && std::prev(place)->Time == sentAt)
            {
                std::prev(place)->Item = value;
            }
            else
            {
                bool newest = place == Samples.end();
                double previous = Samples.empty() ? 0.0 : Samples.back().Time;
                Samples.insert(place, Sample { sentAt, value });
                if (newest && Samples.size() >= 2)
                {
                    LearnInterval(sentAt - previous);
                }
            }
            ++Received;

            // How late values arrive: the worst in the current and the previous
            // few seconds, so one late value raises it at once and it falls
            // back only once the network has been calm for a while.
            if (now - LatenessWindowStarted >= 3.0)
            {
                LatenessPeak[1] = LatenessPeak[0];
                LatenessPeak[0] = 0.0;
                LatenessWindowStarted = now;
            }
            LatenessPeak[0] = std::max(LatenessPeak[0], std::max(now - sentAt, 0.0));

            // A second and a half of history, and never more than 128 values.
            while (Samples.size() > 128 || (Samples.size() > 2 && Samples.back().Time - Samples.front().Time > 1.5))
            {
                Samples.pop_front();
            }
        }

        // The value to show now.
        Value Get() { return Get(NetworkTime()); }

        // The value to show at `now` on NetworkTime's clock. Times passed in
        // should not go backwards.
        Value Get(double now)
        {
            if (Samples.empty() || !std::isfinite(now))
            {
                return Samples.empty() ? Value {} : Samples.back().Item;
            }
            double wanted = WantedDelay();
            if (!Started)
            {
                // The first few values show as they come, until there are enough
                // to tell how often values are sent.
                if (Received < 3)
                {
                    return Samples.back().Item;
                }
                Started = true;
                CurrentDelay = wanted;
            }
            else
            {
                // Within a comfortable range the delay stays put, so what is shown
                // moves at exactly the speed it was sent. Outside it, the delay
                // moves by a few hundredths of the time passed, so what is shown
                // runs that much slower or faster instead of jumping.
                double passed = std::clamp(now - LastGet, 0.0, 0.25);
                if (CurrentDelay < wanted)
                {
                    CurrentDelay = std::min(wanted, CurrentDelay + passed * 0.05);
                }
                else if (CurrentDelay > wanted * 1.3 + 0.01)
                {
                    CurrentDelay = std::max(wanted, CurrentDelay - passed * 0.02);
                }
            }
            LastGet = now;

            double time = std::max(now - CurrentDelay, LastShown);
            LastShown = time;

            // Values before the one at or just before the time are done with.
            while (Samples.size() >= 2 && Samples[1].Time <= time)
            {
                Samples.pop_front();
            }
            const Sample& from = Samples.front();
            if (Samples.size() == 1 || time <= from.Time)
            {
                return from.Item;
            }
            const Sample& to = Samples[1];
            auto amount = static_cast<float>((time - from.Time) / (to.Time - from.Time));
            return Blend(from.Item, to.Item, std::clamp(amount, 0.0f, 1.0f));
        }

        // Jumps to `value` at once and forgets the values before it, as when a
        // player is put somewhere new.
        void Reset(const Value& value)
        {
            Samples.clear();
            Samples.push_back(Sample { Started ? LastShown : 0.0, value });
            Received = std::max(Received, 1);
        }

        bool HasValue() const { return !Samples.empty(); }

        // The value sent most recently, without smoothing.
        Value Newest() const { return Samples.empty() ? Value {} : Samples.back().Item; }

        // How far in the past Get shows, in seconds.
        double Delay() const { return Started ? CurrentDelay : WantedDelay(); }

    private:
        struct Sample
        {
            double Time = 0.0;
            Value Item {};
        };

        static Value Blend(const Value& from, const Value& to, float amount)
        {
            if constexpr (std::same_as<Value, Quaternion>)
            {
                return Slerp(from, to, amount);
            }
            else if constexpr (std::same_as<Value, double>)
            {
                return from + (to - from) * static_cast<double>(amount);
            }
            else
            {
                return Lerp(from, to, amount);
            }
        }

        // Follows the time between values sent. A gap of a few intervals is
        // values lost on the way rather than slower sending, unless gaps stay
        // that long.
        void LearnInterval(double gap)
        {
            if (!(gap > 0.0) || gap >= 1.0)
            {
                return;
            }
            if (Interval == 0.0)
            {
                Interval = gap;
                return;
            }
            double spans = std::max(std::round(gap / Interval), 1.0);
            LongGaps = spans > 1.0 ? LongGaps + 1 : 0;
            if (LongGaps >= 4)
            {
                Interval = gap;
                LongGaps = 0;
                return;
            }
            Interval += (gap / spans - Interval) * 0.05;
        }

        // Enough to blend across one lost value, plus how late values come,
        // plus a little, and never more than half a second.
        double WantedDelay() const
        {
            return std::min(Interval * 2.0 + std::max(LatenessPeak[0], LatenessPeak[1]) + 0.004, 0.5);
        }

        std::deque<Sample> Samples;
        double Interval = 0.0;
        int LongGaps = 0;
        int Received = 0;
        double LatenessPeak[2] = { 0.0, 0.0 };
        double LatenessWindowStarted = 0.0;
        double CurrentDelay = 0.0;
        double LastGet = 0.0;
        double LastShown = 0.0;
        bool Started = false;
    };

    // An angle in radians another program sends again and again, shown
    // smoothly like Smoothed, and turning the short way round.
    class SmoothedAngle
    {
    public:
        SmoothedAngle() = default;
        explicit SmoothedAngle(float radians) { Reset(radians); }

        void Add(float radians, const Message& message) { Direction.Add(Toward(radians), message); }
        void Add(float radians, double sentAt, double now) { Direction.Add(Toward(radians), sentAt, now); }

        // From -pi to pi.
        float Get() { return Angle(Direction.Get()); }
        float Get(double now) { return Angle(Direction.Get(now)); }

        void Reset(float radians) { Direction.Reset(Toward(radians)); }
        bool HasValue() const { return Direction.HasValue(); }
        float Newest() const { return Angle(Direction.Newest()); }
        double Delay() const { return Direction.Delay(); }

    private:
        static Vector2 Toward(float radians) { return { std::cos(radians), std::sin(radians) }; }
        static float Angle(Vector2 direction) { return std::atan2(direction.Y, direction.X); }

        Smoothed<Vector2> Direction;
    };
}
