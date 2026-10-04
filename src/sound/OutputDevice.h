#pragma once

#include <memory>
#include <string>

namespace easyforge::internal
{
    // What an output device asks of the mixer, on the device's own thread.
    class DeviceClient
    {
    public:
        virtual ~DeviceClient() = default;

        // The device is ready at this format, or ready again after changing.
        // Comes before the first DeviceRender at the format.
        virtual void DeviceFormat(int sampleRate, int channelCount, const std::string& name) = 0;

        // Fills `frames` frames, channels interleaved.
        virtual void DeviceRender(float* output, int frames) = 0;
    };

    struct DeviceSettings
    {
        // Zero takes the device's own rate.
        int SampleRate = 0;
        float Latency = 0.04f;
    };

    // The system's default output device, playing what its client renders on a
    // thread of its own. When the default device changes or goes away it opens
    // the new one; with none at all it keeps the client running in silence.
    // Destroying it stops the thread.
    class OutputDevice
    {
    public:
        virtual ~OutputDevice() = default;
    };

    // Opens the default output device and starts it, or returns nothing and
    // says why in `error`.
    std::unique_ptr<OutputDevice> StartOutputDevice(DeviceClient& client, const DeviceSettings& settings, std::string& error);
}
