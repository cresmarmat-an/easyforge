#include "Resources.h"

#include <cmath>
#include <cstring>

namespace easyforge::internal
{
    gpu::Texture* TextureState::On(const std::shared_ptr<gpu::Device>& device)
    {
        if (!Image)
        {
            return nullptr;
        }
        DeviceCopy<gpu::Texture>& copy = Copies[device->Identifier()];
        if (copy.Object && copy.Version == Version)
        {
            return copy.Object.get();
        }
        if (!copy.Object || copy.Object->Width() != Image.Width || copy.Object->Height() != Image.Height)
        {
            copy.Object = device->CreateTexture({ .Width = Image.Width, .Height = Image.Height, .Name = "texture" });
            copy.Device = device;
            if (!copy.Object)
            {
                return nullptr;
            }
        }
        device->UploadTexture(*copy.Object, 0, 0, Image.Width, Image.Height, Image.Pixels.data(), Image.Stride());
        copy.Version = Version;
        return copy.Object.get();
    }

    gpu::Sampling TextureState::Sampling() const
    {
        if (Settings.Smooth)
        {
            return Settings.Repeat ? gpu::Sampling::LinearRepeat : gpu::Sampling::LinearClamp;
        }
        return Settings.Repeat ? gpu::Sampling::NearestRepeat : gpu::Sampling::NearestClamp;
    }

    const GlyphPlacement& GlyphAtlas::Find(const FontState& font, int glyph, float pixelsPerEm)
    {
        // Sizes a quarter of a pixel apart share their glyphs.
        Key key { font.Identifier, glyph, static_cast<int>(std::lround(pixelsPerEm * 4.0f)) };
        auto found = Glyphs.find(key);
        if (found != Glyphs.end())
        {
            return found->second;
        }

        GlyphBitmap bitmap = font.Data.Rasterize(glyph, static_cast<float>(key.Size) / 4.0f);
        GlyphPlacement placement;
        placement.Width = bitmap.Width;
        placement.Height = bitmap.Height;
        placement.Left = bitmap.Left;
        placement.Top = bitmap.Top;
        if (bitmap.Width <= 0 || bitmap.Height <= 0 || bitmap.Width >= PageSize || bitmap.Height >= PageSize)
        {
            placement.Width = 0;
            placement.Height = 0;
            return Glyphs.emplace(key, placement).first->second;
        }

        // Rows of glyphs, with a pixel of space between them so sampling one never
        // picks up its neighbor.
        constexpr int Gap = 1;
        if (RowX + bitmap.Width + Gap > PageSize)
        {
            RowX = 0;
            RowY += RowHeight + Gap;
            RowHeight = 0;
        }
        if (Pages.empty() || RowY + bitmap.Height + Gap > PageSize)
        {
            Pages.push_back(Device->CreateTexture(
                { .Width = PageSize, .Height = PageSize, .Format = gpu::TextureFormat::R8, .Name = "glyph atlas" }));
            // A new page starts empty; clear it so the gaps between glyphs read as nothing.
            std::vector<std::uint8_t> zeros(static_cast<std::size_t>(PageSize) * PageSize, 0);
            Device->UploadTexture(*Pages.back(), 0, 0, PageSize, PageSize, zeros.data(), PageSize);
            RowX = 0;
            RowY = 0;
            RowHeight = 0;
        }
        placement.Page = static_cast<int>(Pages.size()) - 1;
        placement.X = RowX;
        placement.Y = RowY;
        Device->UploadTexture(*Pages.back(), RowX, RowY, bitmap.Width, bitmap.Height, bitmap.Coverage.data(),
            static_cast<std::size_t>(bitmap.Width));
        RowX += bitmap.Width + Gap;
        RowHeight = Max(RowHeight, bitmap.Height);
        return Glyphs.emplace(key, placement).first->second;
    }

    gpu::Texture* GlyphAtlas::Page(int index) const
    {
        return index >= 0 && index < static_cast<int>(Pages.size()) ? Pages[static_cast<std::size_t>(index)].get() : nullptr;
    }

    std::shared_ptr<DeviceResources> DeviceResources::For(const std::shared_ptr<gpu::Device>& device)
    {
        static std::map<std::uint64_t, std::weak_ptr<DeviceResources>> resources;
        std::weak_ptr<DeviceResources>& entry = resources[device->Identifier()];
        if (std::shared_ptr<DeviceResources> existing = entry.lock())
        {
            return existing;
        }
        auto created = std::make_shared<DeviceResources>(device);
        entry = created;
        return created;
    }

    const GpuModel& ModelState::On(const std::shared_ptr<gpu::Device>& device)
    {
        GpuModel& copy = Copies[device->Identifier()];
        if (copy.Device)
        {
            return copy;
        }
        copy.Device = device;
        for (const MeshData& mesh : Data.Meshes)
        {
            if (mesh.Vertices.empty() || mesh.Indices.empty())
            {
                continue;
            }
            GpuMesh uploaded;
            // Position, normal, and texture coordinate: eight floats a vertex.
            std::vector<float> vertices;
            vertices.reserve(mesh.Vertices.size() * 8);
            for (const MeshVertex& vertex : mesh.Vertices)
            {
                vertices.insert(vertices.end(), { vertex.Position.X, vertex.Position.Y, vertex.Position.Z, vertex.Normal.X,
                                                    vertex.Normal.Y, vertex.Normal.Z, vertex.TextureCoordinate.X,
                                                    vertex.TextureCoordinate.Y });
            }
            uploaded.Vertices = device->CreateBuffer(
                { reinterpret_cast<const std::uint8_t*>(vertices.data()), vertices.size() * sizeof(float) }, false, mesh.Name);
            uploaded.Indices = device->CreateBuffer({ reinterpret_cast<const std::uint8_t*>(mesh.Indices.data()),
                                                        mesh.Indices.size() * sizeof(std::uint32_t) },
                true, mesh.Name);
            uploaded.IndexCount = static_cast<std::uint32_t>(mesh.Indices.size());
            uploaded.MaterialIndex = mesh.MaterialIndex;
            if (uploaded.Vertices && uploaded.Indices)
            {
                copy.Meshes.push_back(std::move(uploaded));
            }
        }
        return copy;
    }
}
