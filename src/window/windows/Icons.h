#pragma once

#include <easyforge/assets/ImageData.h>

#include "Win32.h"

namespace easyforge::internal
{
    // An icon of `size` by `size` pixels from the image, scaled to fit. The
    // caller destroys it with DestroyIcon. Null when Windows refuses it.
    HICON CreateIconFromImage(const ImageData& image, int size);

    // A cursor from the image at its own size, clicking at `hotSpotX`, `hotSpotY`.
    // The caller destroys it with DestroyCursor.
    HCURSOR CreateCursorFromImage(const ImageData& image, int hotSpotX, int hotSpotY);

    // The first icon built into the program, at the given size, or null when the
    // program has none. The caller destroys it with DestroyIcon.
    HICON LoadProgramIcon(int size);
}
