#include "Keyboard.h"

#include <array>

#include "Win32.h"

namespace easyforge::internal
{
    namespace
    {
        // Scan codes from set 1, the codes Windows reports, without the prefix.
        constexpr std::array<Key, 128> PlainKeys = [] {
            std::array<Key, 128> keys {};
            keys[0x01] = Key::Escape;
            keys[0x02] = Key::Digit1;
            keys[0x03] = Key::Digit2;
            keys[0x04] = Key::Digit3;
            keys[0x05] = Key::Digit4;
            keys[0x06] = Key::Digit5;
            keys[0x07] = Key::Digit6;
            keys[0x08] = Key::Digit7;
            keys[0x09] = Key::Digit8;
            keys[0x0A] = Key::Digit9;
            keys[0x0B] = Key::Digit0;
            keys[0x0C] = Key::Minus;
            keys[0x0D] = Key::Equals;
            keys[0x0E] = Key::Backspace;
            keys[0x0F] = Key::Tab;
            keys[0x10] = Key::Q;
            keys[0x11] = Key::W;
            keys[0x12] = Key::E;
            keys[0x13] = Key::R;
            keys[0x14] = Key::T;
            keys[0x15] = Key::Y;
            keys[0x16] = Key::U;
            keys[0x17] = Key::I;
            keys[0x18] = Key::O;
            keys[0x19] = Key::P;
            keys[0x1A] = Key::LeftBracket;
            keys[0x1B] = Key::RightBracket;
            keys[0x1C] = Key::Enter;
            keys[0x1D] = Key::LeftControl;
            keys[0x1E] = Key::A;
            keys[0x1F] = Key::S;
            keys[0x20] = Key::D;
            keys[0x21] = Key::F;
            keys[0x22] = Key::G;
            keys[0x23] = Key::H;
            keys[0x24] = Key::J;
            keys[0x25] = Key::K;
            keys[0x26] = Key::L;
            keys[0x27] = Key::Semicolon;
            keys[0x28] = Key::Apostrophe;
            keys[0x29] = Key::Backquote;
            keys[0x2A] = Key::LeftShift;
            keys[0x2B] = Key::Backslash;
            keys[0x2C] = Key::Z;
            keys[0x2D] = Key::X;
            keys[0x2E] = Key::C;
            keys[0x2F] = Key::V;
            keys[0x30] = Key::B;
            keys[0x31] = Key::N;
            keys[0x32] = Key::M;
            keys[0x33] = Key::Comma;
            keys[0x34] = Key::Period;
            keys[0x35] = Key::Slash;
            keys[0x36] = Key::RightShift;
            keys[0x37] = Key::NumberPadMultiply;
            keys[0x38] = Key::LeftAlt;
            keys[0x39] = Key::Space;
            keys[0x3A] = Key::CapsLock;
            keys[0x3B] = Key::F1;
            keys[0x3C] = Key::F2;
            keys[0x3D] = Key::F3;
            keys[0x3E] = Key::F4;
            keys[0x3F] = Key::F5;
            keys[0x40] = Key::F6;
            keys[0x41] = Key::F7;
            keys[0x42] = Key::F8;
            keys[0x43] = Key::F9;
            keys[0x44] = Key::F10;
            keys[0x45] = Key::Pause;
            keys[0x46] = Key::ScrollLock;
            keys[0x47] = Key::NumberPad7;
            keys[0x48] = Key::NumberPad8;
            keys[0x49] = Key::NumberPad9;
            keys[0x4A] = Key::NumberPadSubtract;
            keys[0x4B] = Key::NumberPad4;
            keys[0x4C] = Key::NumberPad5;
            keys[0x4D] = Key::NumberPad6;
            keys[0x4E] = Key::NumberPadAdd;
            keys[0x4F] = Key::NumberPad1;
            keys[0x50] = Key::NumberPad2;
            keys[0x51] = Key::NumberPad3;
            keys[0x52] = Key::NumberPad0;
            keys[0x53] = Key::NumberPadDecimal;
            keys[0x56] = Key::InternationalBackslash;
            keys[0x57] = Key::F11;
            keys[0x58] = Key::F12;
            keys[0x64] = Key::F13;
            keys[0x65] = Key::F14;
            keys[0x66] = Key::F15;
            keys[0x67] = Key::F16;
            keys[0x68] = Key::F17;
            keys[0x69] = Key::F18;
            keys[0x6A] = Key::F19;
            keys[0x6B] = Key::F20;
            keys[0x6C] = Key::F21;
            keys[0x6D] = Key::F22;
            keys[0x6E] = Key::F23;
            keys[0x76] = Key::F24;
            return keys;
        }();

        // Keys sent with the 0xE0 prefix.
        Key ExtendedKey(unsigned scanCode)
        {
            switch (scanCode)
            {
            case 0x1C: return Key::NumberPadEnter;
            case 0x1D: return Key::RightControl;
            case 0x35: return Key::NumberPadDivide;
            case 0x37: return Key::PrintScreen;
            case 0x38: return Key::RightAlt;
            case 0x45: return Key::NumberLock;
            case 0x46: return Key::Pause;
            case 0x47: return Key::Home;
            case 0x48: return Key::Up;
            case 0x49: return Key::PageUp;
            case 0x4B: return Key::Left;
            case 0x4D: return Key::Right;
            case 0x4F: return Key::End;
            case 0x50: return Key::Down;
            case 0x51: return Key::PageDown;
            case 0x52: return Key::Insert;
            case 0x53: return Key::Delete;
            case 0x5B: return Key::LeftMeta;
            case 0x5C: return Key::RightMeta;
            case 0x5D: return Key::Menu;
            default: return Key::Unknown;
            }
        }
    }

    Key KeyFromScanCode(unsigned scanCode, bool extended, unsigned virtualKey)
    {
        // Pause and Number Lock share scan code 0x45, and Windows does not mark
        // them apart reliably, so the virtual key decides.
        if (virtualKey == VK_PAUSE)
        {
            return Key::Pause;
        }
        if (virtualKey == VK_NUMLOCK)
        {
            return Key::NumberLock;
        }

        // Some programs that send keys leave the scan code out. Windows can find
        // it from the virtual key, but not whether it had the extended prefix,
        // which is what tells the arrows apart from the number pad.
        if (scanCode == 0)
        {
            switch (virtualKey)
            {
            case VK_LEFT: return Key::Left;
            case VK_RIGHT: return Key::Right;
            case VK_UP: return Key::Up;
            case VK_DOWN: return Key::Down;
            case VK_HOME: return Key::Home;
            case VK_END: return Key::End;
            case VK_PRIOR: return Key::PageUp;
            case VK_NEXT: return Key::PageDown;
            case VK_INSERT: return Key::Insert;
            case VK_DELETE: return Key::Delete;
            case VK_DIVIDE: return Key::NumberPadDivide;
            case VK_RCONTROL: return Key::RightControl;
            case VK_RMENU: return Key::RightAlt;
            case VK_LWIN: return Key::LeftMeta;
            case VK_RWIN: return Key::RightMeta;
            case VK_APPS: return Key::Menu;
            case VK_SNAPSHOT: return Key::PrintScreen;
            default: break;
            }
        }
        if (scanCode == 0 && virtualKey != 0)
        {
            unsigned mapped = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC_EX);
            scanCode = mapped & 0xFF;
            extended = (mapped & 0xFF00) == 0xE000;
        }

        if (extended)
        {
            return ExtendedKey(scanCode);
        }
        return scanCode < PlainKeys.size() ? PlainKeys[scanCode] : Key::Unknown;
    }
}
