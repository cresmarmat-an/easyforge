#include "Icons.h"

#include <cstring>
#include <string>
#include <variant>
#include <vector>

namespace easyforge::internal
{
    namespace
    {
        // Icons and cursors take 32-bit bitmaps with straight alpha, in blue,
        // green, red, alpha order, plus a mask that is ignored when alpha is present.
        HICON CreateFromPixels(const ImageData& image, bool icon, int hotSpotX, int hotSpotY)
        {
            BITMAPV5HEADER header {};
            header.bV5Size = sizeof(header);
            header.bV5Width = image.Width;
            header.bV5Height = -image.Height;
            header.bV5Planes = 1;
            header.bV5BitCount = 32;
            header.bV5Compression = BI_BITFIELDS;
            header.bV5RedMask = 0x00FF0000;
            header.bV5GreenMask = 0x0000FF00;
            header.bV5BlueMask = 0x000000FF;
            header.bV5AlphaMask = 0xFF000000;

            HDC screen = GetDC(nullptr);
            void* bits = nullptr;
            HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits,
                nullptr, 0);
            ReleaseDC(nullptr, screen);
            if (!color)
            {
                return nullptr;
            }

            auto* output = static_cast<std::uint8_t*>(bits);
            for (std::size_t pixel = 0; pixel < image.Pixels.size(); pixel += 4)
            {
                output[pixel + 0] = image.Pixels[pixel + 2];
                output[pixel + 1] = image.Pixels[pixel + 1];
                output[pixel + 2] = image.Pixels[pixel + 0];
                output[pixel + 3] = image.Pixels[pixel + 3];
            }

            std::vector<std::uint8_t> maskBits(static_cast<std::size_t>((image.Width + 15) / 16 * 2) * image.Height, 0);
            HBITMAP mask = CreateBitmap(image.Width, image.Height, 1, 1, maskBits.data());

            ICONINFO information {};
            information.fIcon = icon ? TRUE : FALSE;
            information.xHotspot = static_cast<DWORD>(hotSpotX);
            information.yHotspot = static_cast<DWORD>(hotSpotY);
            information.hbmMask = mask;
            information.hbmColor = color;
            HICON result = CreateIconIndirect(&information);

            DeleteObject(color);
            DeleteObject(mask);
            return result;
        }

        // The first icon group's name: a number or a string.
        struct ProgramIconName
        {
            bool Searched = false;
            bool Found = false;
            std::variant<WORD, std::wstring> Name;
        };

        BOOL CALLBACK RememberFirstIcon(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR parameter)
        {
            auto* found = reinterpret_cast<ProgramIconName*>(parameter);
            found->Found = true;
            if (IS_INTRESOURCE(name))
            {
                found->Name = static_cast<WORD>(reinterpret_cast<ULONG_PTR>(name));
            }
            else
            {
                found->Name = std::wstring(name);
            }
            return FALSE;
        }
    }

    HICON CreateIconFromImage(const ImageData& image, int size)
    {
        if (!image || size <= 0)
        {
            return nullptr;
        }
        // Keep the shape: fit the longer side, center the shorter one.
        ImageData square(size, size);
        int width = image.Width >= image.Height ? size : Max(1, image.Width * size / image.Height);
        int height = image.Height >= image.Width ? size : Max(1, image.Height * size / image.Width);
        ImageData scaled = image.Resized(width, height);
        int left = (size - width) / 2;
        int top = (size - height) / 2;
        for (int y = 0; y < height; ++y)
        {
            std::memcpy(square.Pixels.data() + static_cast<std::size_t>(top + y) * square.Stride() + left * 4,
                scaled.Pixels.data() + static_cast<std::size_t>(y) * scaled.Stride(), scaled.Stride());
        }
        return CreateFromPixels(square, true, 0, 0);
    }

    HCURSOR CreateCursorFromImage(const ImageData& image, int hotSpotX, int hotSpotY)
    {
        if (!image)
        {
            return nullptr;
        }
        return CreateFromPixels(image, false, Clamp(hotSpotX, 0, image.Width - 1), Clamp(hotSpotY, 0, image.Height - 1));
    }

    HICON LoadProgramIcon(int size)
    {
        static ProgramIconName program;
        HMODULE module = GetModuleHandleW(nullptr);
        if (!program.Searched)
        {
            program.Searched = true;
            EnumResourceNamesW(module, RT_GROUP_ICON, RememberFirstIcon, reinterpret_cast<LONG_PTR>(&program));
        }
        if (!program.Found)
        {
            return nullptr;
        }
        LPCWSTR name = std::holds_alternative<WORD>(program.Name) ? MAKEINTRESOURCEW(std::get<WORD>(program.Name))
                                                                   : std::get<std::wstring>(program.Name).c_str();
        return static_cast<HICON>(LoadImageW(module, name, IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    }
}
