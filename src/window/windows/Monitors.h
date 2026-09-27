#pragma once

#include <easyforge/window/Monitor.h>

#include "Win32.h"

namespace easyforge::internal
{
    Monitor DescribeMonitor(HMONITOR handle);
}
