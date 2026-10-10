#pragma once

// Switches the program to German number writing, "1,5" for one and a half,
// while it lives, to show that files and scripts read and write numbers the
// same in every country. Tests run one at a time, so no other test sees it.

#include <clocale>
#include <locale>
#include <string>

namespace easyforge::testing
{
    class CommaDecimals
    {
    public:
        CommaDecimals() : BeforeText(std::setlocale(LC_ALL, nullptr))
        {
            for (const char* name : { "de-DE", "de_DE.UTF-8", "de_DE", "German" })
            {
                try
                {
                    std::locale::global(std::locale(name));
                    break;
                }
                catch (...)
                {
                    // Not every standard library knows every name; the C
                    // locale below is what number reading goes through.
                }
            }
            for (const char* name : { "de-DE", "de_DE.UTF-8", "de_DE", "German" })
            {
                if (std::setlocale(LC_ALL, name))
                {
                    break;
                }
            }
        }

        ~CommaDecimals()
        {
            std::locale::global(Before);
            std::setlocale(LC_ALL, BeforeText.c_str());
        }

        CommaDecimals(const CommaDecimals&) = delete;
        CommaDecimals& operator=(const CommaDecimals&) = delete;

        // Whether the switch took: the C library now writes a comma.
        bool Active() const { return std::to_string(1.5).find(',') != std::string::npos; }

    private:
        std::locale Before;
        std::string BeforeText;
    };
}
