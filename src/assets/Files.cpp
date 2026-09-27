#include <easyforge/assets/Files.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <unordered_map>

#include "ProgramFolder.h"

namespace easyforge
{
    namespace internal
    {
        // A file on disk, or a part of one when it is an entry inside a pack.
        class FileSource
        {
        public:
            FileSource(std::filesystem::path path, std::uint64_t start, std::uint64_t size)
                : Path(std::move(path)), Start(start), Length(size)
            {
            }

            bool Open()
            {
                Stream.open(Path, std::ios::binary);
                return Stream.is_open();
            }

            std::uint64_t Size() const { return Length; }

            std::size_t ReadAt(std::uint64_t offset, std::span<std::uint8_t> output)
            {
                if (offset >= Length || output.empty())
                {
                    return 0;
                }
                std::uint64_t count = std::min<std::uint64_t>(output.size(), Length - offset);

                std::lock_guard lock(Mutex);
                Stream.clear();
                Stream.seekg(static_cast<std::streamoff>(Start + offset));
                Stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(count));
                return static_cast<std::size_t>(Stream.gcount());
            }

        private:
            std::filesystem::path Path;
            std::uint64_t Start;
            std::uint64_t Length;
            std::mutex Mutex;
            std::ifstream Stream;
        };
    }

    namespace
    {
        constexpr char PackMagic[6] = { 'E', 'F', 'P', 'A', 'C', 'K' };
        constexpr std::uint16_t PackVersion = 1;

        struct PackEntry
        {
            std::uint64_t Offset = 0;
            std::uint64_t Size = 0;
        };

        struct Mounted
        {
            std::string Name;
            std::filesystem::path Location;
            bool IsPack = false;
            std::unordered_map<std::string, PackEntry> Entries;
        };

        struct MountState
        {
            std::shared_mutex Mutex;
            std::vector<std::shared_ptr<const Mounted>> Places;
        };

        MountState& Mounts()
        {
            static MountState state;
            return state;
        }

        std::filesystem::path ToPath(std::string_view utf8)
        {
            return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
        }

        std::string ToText(const std::filesystem::path& path)
        {
            std::u8string text = path.generic_u8string();
            return std::string(text.begin(), text.end());
        }

        // Forward slashes, no "." or ".." parts, no leading "./".
        std::string NormalizeRelative(std::string_view path)
        {
            std::string text(path);
            std::replace(text.begin(), text.end(), '\\', '/');
            return ToText(ToPath(text).lexically_normal());
        }

        bool IsAbsolute(std::string_view path)
        {
            return ToPath(path).has_root_path();
        }

        bool IsRegularFile(const std::filesystem::path& path)
        {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
        }

        void WriteLittle(std::ofstream& stream, std::uint64_t value, int byteCount)
        {
            for (int index = 0; index < byteCount; ++index)
            {
                char byte = static_cast<char>((value >> (8 * index)) & 0xFF);
                stream.write(&byte, 1);
            }
        }

        std::uint64_t ReadLittle(std::ifstream& stream, int byteCount)
        {
            std::uint64_t value = 0;
            for (int index = 0; index < byteCount; ++index)
            {
                char byte = 0;
                stream.read(&byte, 1);
                value |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(byte)) << (8 * index);
            }
            return value;
        }

        Result<std::shared_ptr<Mounted>> ReadPack(const std::filesystem::path& location, std::string name)
        {
            std::ifstream stream(location, std::ios::binary);
            if (!stream)
            {
                return Failure(std::format("{} could not be opened", name));
            }

            char magic[sizeof(PackMagic)] = {};
            stream.read(magic, sizeof(magic));
            if (!stream || std::memcmp(magic, PackMagic, sizeof(PackMagic)) != 0)
            {
                return Failure(std::format("{} is neither a folder nor an easyforge pack", name));
            }
            std::uint64_t version = ReadLittle(stream, 2);
            if (version != PackVersion)
            {
                return Failure(std::format("{} is a pack of version {}, which this easyforge cannot read", name, version));
            }

            std::error_code error;
            std::uint64_t fileSize = std::filesystem::file_size(location, error);
            std::uint64_t count = ReadLittle(stream, 4);

            auto mounted = std::make_shared<Mounted>();
            mounted->Name = std::move(name);
            mounted->Location = location;
            mounted->IsPack = true;
            for (std::uint64_t index = 0; index < count; ++index)
            {
                std::uint64_t pathLength = ReadLittle(stream, 2);
                std::string path(static_cast<std::size_t>(pathLength), '\0');
                stream.read(path.data(), static_cast<std::streamsize>(pathLength));
                PackEntry entry;
                entry.Offset = ReadLittle(stream, 8);
                entry.Size = ReadLittle(stream, 8);
                if (!stream || entry.Offset > fileSize || entry.Size > fileSize - entry.Offset)
                {
                    return Failure(std::format("{} is damaged: its list of files is cut short or wrong", mounted->Name));
                }
                mounted->Entries.emplace(std::move(path), entry);
            }
            return mounted;
        }

        // Finds a file and opens it. Mounted places come first, most recent first.
        Result<std::shared_ptr<internal::FileSource>> Locate(std::string_view path)
        {
            auto open = [](std::filesystem::path location, std::uint64_t start,
                            std::uint64_t size) -> std::shared_ptr<internal::FileSource> {
                auto source = std::make_shared<internal::FileSource>(std::move(location), start, size);
                return source->Open() ? source : nullptr;
            };

            auto openDisk = [&open](const std::filesystem::path& location) -> std::shared_ptr<internal::FileSource> {
                std::error_code error;
                std::uint64_t size = std::filesystem::file_size(location, error);
                return error ? nullptr : open(location, 0, size);
            };

            if (IsAbsolute(path))
            {
                std::filesystem::path location = ToPath(path);
                if (IsRegularFile(location))
                {
                    if (auto source = openDisk(location))
                    {
                        return source;
                    }
                    return Failure(std::format("{} exists but could not be opened", path));
                }
                return Failure(std::format("{} was not found", path));
            }

            std::string relative = NormalizeRelative(path);

            std::vector<std::shared_ptr<const Mounted>> places;
            {
                MountState& state = Mounts();
                std::shared_lock lock(state.Mutex);
                places = state.Places;
            }
            for (auto place = places.rbegin(); place != places.rend(); ++place)
            {
                const Mounted& mounted = **place;
                if (mounted.IsPack)
                {
                    auto found = mounted.Entries.find(relative);
                    if (found != mounted.Entries.end())
                    {
                        if (auto source = open(mounted.Location, found->second.Offset, found->second.Size))
                        {
                            return source;
                        }
                    }
                }
                else
                {
                    std::filesystem::path location = mounted.Location / ToPath(relative);
                    if (IsRegularFile(location))
                    {
                        if (auto source = openDisk(location))
                        {
                            return source;
                        }
                    }
                }
            }

            std::filesystem::path working = ToPath(relative);
            if (IsRegularFile(working))
            {
                if (auto source = openDisk(working))
                {
                    return source;
                }
            }

            std::filesystem::path programFolder = internal::ExecutableFolder();
            if (!programFolder.empty())
            {
                std::filesystem::path beside = programFolder / ToPath(relative);
                if (IsRegularFile(beside))
                {
                    if (auto source = openDisk(beside))
                    {
                        return source;
                    }
                }
            }

            std::error_code error;
            std::string message = std::format("{} was not found in the working directory ({})", relative,
                ToText(std::filesystem::current_path(error)));
            if (!programFolder.empty())
            {
                message += std::format(" or next to the program ({})", ToText(programFolder));
            }
            if (!places.empty())
            {
                message += std::format(" or in the {} mounted folders and packs", places.size());
            }
            return Failure(message);
        }
    }

    std::uint64_t FileReader::Size() const
    {
        return Source ? Source->Size() : 0;
    }

    std::size_t FileReader::ReadAt(std::uint64_t offset, std::span<std::uint8_t> output) const
    {
        return Source ? Source->ReadAt(offset, output) : 0;
    }

    Result<> Files::Mount(std::string_view folderOrPack)
    {
        std::filesystem::path location = ToPath(folderOrPack);
        if (!IsAbsolute(folderOrPack))
        {
            std::error_code error;
            if (!std::filesystem::exists(location, error))
            {
                std::filesystem::path beside = internal::ExecutableFolder() / location;
                if (std::filesystem::exists(beside, error))
                {
                    location = beside;
                }
            }
        }

        std::error_code error;
        std::shared_ptr<Mounted> mounted;
        if (std::filesystem::is_directory(location, error))
        {
            mounted = std::make_shared<Mounted>();
            mounted->Name = std::string(folderOrPack);
            mounted->Location = location;
        }
        else if (std::filesystem::is_regular_file(location, error))
        {
            Result<std::shared_ptr<Mounted>> pack = ReadPack(location, std::string(folderOrPack));
            if (!pack)
            {
                return Failure(pack.Error());
            }
            mounted = std::move(pack).Get();
        }
        else
        {
            return Failure(std::format("{} was not found, so it could not be mounted", folderOrPack));
        }

        MountState& state = Mounts();
        std::unique_lock lock(state.Mutex);
        state.Places.push_back(std::move(mounted));
        return {};
    }

    void Files::Unmount(std::string_view folderOrPack)
    {
        MountState& state = Mounts();
        std::unique_lock lock(state.Mutex);
        auto found = std::find_if(state.Places.rbegin(), state.Places.rend(),
            [folderOrPack](const std::shared_ptr<const Mounted>& place) { return place->Name == folderOrPack; });
        if (found != state.Places.rend())
        {
            state.Places.erase(std::next(found).base());
        }
    }

    void Files::UnmountAll()
    {
        MountState& state = Mounts();
        std::unique_lock lock(state.Mutex);
        state.Places.clear();
    }

    Result<std::vector<std::uint8_t>> Files::Read(std::string_view path)
    {
        Result<std::shared_ptr<internal::FileSource>> source = Locate(path);
        if (!source)
        {
            return Failure(source.Error());
        }

        std::uint64_t size = (*source)->Size();
        if (size > static_cast<std::uint64_t>(SIZE_MAX / 2))
        {
            return Failure(std::format("{} is too large to read at once; use Files::Open", path));
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if ((*source)->ReadAt(0, bytes) != bytes.size())
        {
            return Failure(std::format("{} could not be read to the end", path));
        }
        return bytes;
    }

    FileReader Files::Open(std::string_view path)
    {
        FileReader reader;
        Result<std::shared_ptr<internal::FileSource>> source = Locate(path);
        if (source)
        {
            reader.Source = std::move(source).Get();
        }
        else
        {
            reader.ErrorText = source.Error();
        }
        return reader;
    }

    bool Files::Exists(std::string_view path)
    {
        return static_cast<bool>(Locate(path));
    }

    std::string Files::ProgramFolder()
    {
        std::filesystem::path folder = internal::ExecutableFolder();
        if (folder.empty())
        {
            return {};
        }
        std::string text = ToText(folder);
        if (!text.empty() && text.back() != '/')
        {
            text += '/';
        }
        return text;
    }

    Result<> Files::CreatePack(std::string_view folder, std::string_view packPath)
    {
        std::filesystem::path root = ToPath(folder);
        std::error_code error;
        if (!std::filesystem::is_directory(root, error))
        {
            return Failure(std::format("{} is not a folder", folder));
        }

        struct Source
        {
            std::string Name;
            std::filesystem::path Location;
            std::uint64_t Size = 0;
        };
        std::vector<Source> sources;
        for (const auto& item : std::filesystem::recursive_directory_iterator(root, error))
        {
            if (!item.is_regular_file())
            {
                continue;
            }
            std::string name = ToText(item.path().lexically_relative(root));
            if (name.size() > 0xFFFF)
            {
                return Failure(std::format("the path {} is too long for a pack", name));
            }
            sources.push_back({ name, item.path(), item.file_size() });
        }
        if (error)
        {
            return Failure(std::format("{} could not be listed: {}", folder, error.message()));
        }
        std::sort(sources.begin(), sources.end(), [](const Source& first, const Source& second) {
            return first.Name < second.Name;
        });

        std::uint64_t offset = sizeof(PackMagic) + 2 + 4;
        for (const Source& source : sources)
        {
            offset += 2 + source.Name.size() + 8 + 8;
        }

        std::ofstream output(ToPath(packPath), std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return Failure(std::format("{} could not be created", packPath));
        }
        output.write(PackMagic, sizeof(PackMagic));
        WriteLittle(output, PackVersion, 2);
        WriteLittle(output, sources.size(), 4);
        for (const Source& source : sources)
        {
            WriteLittle(output, source.Name.size(), 2);
            output.write(source.Name.data(), static_cast<std::streamsize>(source.Name.size()));
            WriteLittle(output, offset, 8);
            WriteLittle(output, source.Size, 8);
            offset += source.Size;
        }

        std::vector<char> buffer(1 << 16);
        for (const Source& source : sources)
        {
            std::ifstream input(source.Location, std::ios::binary);
            if (!input)
            {
                return Failure(std::format("{} could not be read", source.Name));
            }
            std::uint64_t copied = 0;
            while (copied < source.Size)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                std::streamsize count = input.gcount();
                if (count <= 0)
                {
                    return Failure(std::format("{} changed size while the pack was being written", source.Name));
                }
                output.write(buffer.data(), count);
                copied += static_cast<std::uint64_t>(count);
            }
        }

        if (!output.flush())
        {
            return Failure(std::format("{} could not be written; the disk may be full", packPath));
        }
        return {};
    }

    std::string Files::FolderOf(std::string_view path)
    {
        std::size_t slash = path.find_last_of("/\\");
        return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash + 1));
    }

    std::string Files::ExtensionOf(std::string_view path)
    {
        std::size_t slash = path.find_last_of("/\\");
        std::size_t dot = path.find_last_of('.');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
        {
            return {};
        }
        std::string extension(path.substr(dot));
        for (char& character : extension)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        return extension;
    }
}
