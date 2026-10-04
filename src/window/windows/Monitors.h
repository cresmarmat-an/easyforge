#pragma once

#include <easyforge/window/Monitor.h>

#include "Win32.h"

namespace easyforge::internal
{
    Monitor DescribeMonitor(HMONITOR handle);

    // In hertz, or 0 when the system does not say.
    float MonitorRefreshRate(HMONITOR handle);
}
