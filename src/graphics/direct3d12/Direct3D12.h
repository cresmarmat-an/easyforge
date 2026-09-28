#pragma once

// Direct3D 12 headers for easyforge's own code, without the min and max macros.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dcomp.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <format>
#include <string>

namespace easyforge::internal::direct3d12
{
    using Microsoft::WRL::ComPtr;

    // "the call failed (0x887A0005)", for error messages.
    inline std::string Describe(HRESULT result)
    {
        return std::format("0x{:08X}", static_cast<unsigned long>(result));
    }
}
