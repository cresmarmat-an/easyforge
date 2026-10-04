// Platforms without an output device yet. Offline mixers still work.

#include "../OutputDevice.h"

namespace easyforge::internal
{
    std::unique_ptr<OutputDevice> StartOutputDevice(DeviceClient&, const DeviceSettings&, std::string& error)
    {
        error = "easyforge sound cannot play to a device on this system yet; offline mixers work";
        return nullptr;
    }
}
