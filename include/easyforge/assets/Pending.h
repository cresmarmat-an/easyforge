#pragma once

#include <string>
#include <utility>

#include <easyforge/core/Jobs.h>

namespace easyforge
{
    // Something being loaded in the background, returned by the LoadInBackground
    // functions. Check Ready() each frame, or call Get() to wait for it.
    //
    //     Pending<ModelData> level = ModelData::LoadInBackground("level.obj");
    //     if (level.Ready())
    //     {
    //         const ModelData& model = level.Get();
    //     }
    template <typename Type>
    class Pending
    {
    public:
        // Nothing is being loaded; Ready() is true and Get() gives an empty value.
        Pending() = default;

        explicit Pending(Job<Type> job) : Work(std::move(job)) {}

        bool Ready() const { return Work.IsDone(); }

        void Wait() const { Work.Wait(); }

        // Waits if needed, then gives what was loaded. Test it like the loaded type:
        // `if (!pending.Get())` is true when loading failed.
        const Type& Get() const
        {
            if (!Work.IsStarted())
            {
                static const Type empty;
                return empty;
            }
            return Work.Get();
        }

    private:
        Job<Type> Work;
    };
}
