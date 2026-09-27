#include <easyforge/assets/ModelData.h>

#include <charconv>
#include <cmath>
#include <format>
#include <map>
#include <unordered_map>

#include <easyforge/assets/Files.h>
#include <easyforge/core/Log.h>

namespace easyforge
{
    namespace
    {
        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            {
                text.remove_prefix(1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
            {
                text.remove_suffix(1);
            }
            return text;
        }

        // Splits off the first word of `text`, leaving the rest.
        std::string_view NextWord(std::string_view& text)
        {
            text = Trim(text);
            std::size_t end = text.find_first_of(" \t");
            std::string_view word = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view() : text.substr(end);
            return word;
        }

        bool ParseFloat(std::string_view text, float& value)
        {
            const char* end = text.data() + text.size();
            auto [position, error] = std::from_chars(text.data(), end, value);
            return error == std::errc() && position == end;
        }

        bool ParseInt(std::string_view text, int& value)
        {
            const char* end = text.data() + text.size();
            auto [position, error] = std::from_chars(text.data(), end, value);
            return error == std::errc() && position == end;
        }

        // Reads up to `count` numbers from the rest of a line.
        int ParseFloats(std::string_view text, float* values, int count)
        {
            int parsed = 0;
            while (parsed < count)
            {
                std::string_view word = NextWord(text);
                if (word.empty() || !ParseFloat(word, values[parsed]))
                {
                    break;
                }
                ++parsed;
            }
            return parsed;
        }

        // A texture statement may start with options such as "-bm 1.0"; the file
        // name is what follows them.
        std::string TexturePath(std::string_view rest, std::string_view folder)
        {
            rest = Trim(rest);
            if (!rest.empty() && rest.front() == '-')
            {
                std::size_t last = rest.find_last_of(" \t");
                rest = last == std::string_view::npos ? rest : rest.substr(last + 1);
            }
            std::string path(rest);
            for (char& character : path)
            {
                if (character == '\\')
                {
                    character = '/';
                }
            }
            return std::string(folder) + path;
        }

        std::vector<MaterialData> ParseMaterials(std::string_view text, std::string_view folder)
        {
            std::vector<MaterialData> materials;
            while (!text.empty())
            {
                std::size_t end = text.find('\n');
                std::string_view line = text.substr(0, end);
                text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);

                std::string_view rest = line;
                std::string_view keyword = NextWord(rest);
                if (keyword.empty() || keyword.front() == '#')
                {
                    continue;
                }
                if (keyword == "newmtl")
                {
                    MaterialData material;
                    material.Name = std::string(Trim(rest));
                    materials.push_back(std::move(material));
                    continue;
                }
                if (materials.empty())
                {
                    continue;
                }

                MaterialData& material = materials.back();
                float values[3] = {};
                if (keyword == "Kd" && ParseFloats(rest, values, 3) == 3)
                {
                    // MTL colors are reflectances in linear light.
                    Color color = Color::FromLinear({ values[0], values[1], values[2], 1.0f });
                    material.BaseColor = color.WithAlpha(material.BaseColor.Alpha);
                }
                else if (keyword == "Ke" && ParseFloats(rest, values, 3) == 3)
                {
                    material.Emissive = Color::FromLinear({ values[0], values[1], values[2], 1.0f });
                }
                else if (keyword == "d" && ParseFloats(rest, values, 1) == 1)
                {
                    material.BaseColor.Alpha = Clamp(values[0], 0.0f, 1.0f);
                }
                else if (keyword == "Tr" && ParseFloats(rest, values, 1) == 1)
                {
                    material.BaseColor.Alpha = Clamp(1.0f - values[0], 0.0f, 1.0f);
                }
                else if (keyword == "Ns" && ParseFloats(rest, values, 1) == 1)
                {
                    // Blinn-Phong shininess to roughness, the usual approximation.
                    material.Roughness = Clamp(std::sqrt(2.0f / (Max(values[0], 0.0f) + 2.0f)), 0.0f, 1.0f);
                }
                else if (keyword == "Pr" && ParseFloats(rest, values, 1) == 1)
                {
                    material.Roughness = Clamp(values[0], 0.0f, 1.0f);
                }
                else if (keyword == "Pm" && ParseFloats(rest, values, 1) == 1)
                {
                    material.Metallic = Clamp(values[0], 0.0f, 1.0f);
                }
                else if (keyword == "map_Kd")
                {
                    material.BaseColorTexture = TexturePath(rest, folder);
                }
                else if (keyword == "map_Bump" || keyword == "bump" || keyword == "norm" || keyword == "map_bump")
                {
                    material.NormalTexture = TexturePath(rest, folder);
                }
            }
            return materials;
        }

        struct VertexKey
        {
            int Position;
            int TextureCoordinate;
            int Normal;

            bool operator==(const VertexKey&) const = default;
        };

        struct VertexKeyHash
        {
            std::size_t operator()(const VertexKey& key) const
            {
                std::size_t hash = static_cast<std::size_t>(key.Position) * 73856093u;
                hash ^= static_cast<std::size_t>(key.TextureCoordinate) * 19349663u;
                hash ^= static_cast<std::size_t>(key.Normal) * 83492791u;
                return hash;
            }
        };

        struct MeshBuilder
        {
            MeshData Mesh;
            std::string MaterialName;
            std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> Seen;

            // For meshes with vertices lacking normals: each vertex's position index.
            std::vector<int> PositionOf;
            bool NeedsNormals = false;
        };
    }

    ModelData ModelData::DecodeObj(std::string_view objectText, std::string_view materialText, std::string_view folder)
    {
        ModelData model;
        model.Materials = ParseMaterials(materialText, folder);

        std::vector<Vector3> positions;
        std::vector<Vector2> textureCoordinates;
        std::vector<Vector3> normals;

        std::vector<MeshBuilder> builders;
        std::map<std::pair<std::string, std::string>, std::size_t> builderIndex;
        std::string groupName = "default";
        std::string materialName;
        MeshBuilder* current = nullptr;

        auto fail = [](int line, std::string message) {
            ModelData failed;
            failed.ErrorText = std::format("line {}: {}", line, message);
            return failed;
        };

        int lineNumber = 0;
        std::string_view text = objectText;
        while (!text.empty())
        {
            ++lineNumber;
            std::size_t end = text.find('\n');
            std::string_view line = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);

            std::string_view rest = line;
            std::string_view keyword = NextWord(rest);
            if (keyword.empty() || keyword.front() == '#')
            {
                continue;
            }

            float values[3] = {};
            if (keyword == "v")
            {
                if (ParseFloats(rest, values, 3) != 3)
                {
                    return fail(lineNumber, "a vertex position needs three numbers");
                }
                positions.push_back({ values[0], values[1], values[2] });
            }
            else if (keyword == "vt")
            {
                int count = ParseFloats(rest, values, 2);
                if (count < 1)
                {
                    return fail(lineNumber, "a texture coordinate needs at least one number");
                }
                textureCoordinates.push_back({ values[0], 1.0f - (count > 1 ? values[1] : 0.0f) });
            }
            else if (keyword == "vn")
            {
                if (ParseFloats(rest, values, 3) != 3)
                {
                    return fail(lineNumber, "a normal needs three numbers");
                }
                normals.push_back(Normalize(Vector3 { values[0], values[1], values[2] }));
            }
            else if (keyword == "o" || keyword == "g")
            {
                std::string_view name = Trim(rest);
                groupName = name.empty() ? "default" : std::string(name);
                current = nullptr;
            }
            else if (keyword == "usemtl")
            {
                materialName = std::string(Trim(rest));
                current = nullptr;
            }
            else if (keyword == "f")
            {
                if (current == nullptr)
                {
                    auto key = std::make_pair(groupName, materialName);
                    auto found = builderIndex.find(key);
                    if (found == builderIndex.end())
                    {
                        found = builderIndex.emplace(key, builders.size()).first;
                        MeshBuilder builder;
                        builder.Mesh.Name = groupName;
                        builder.MaterialName = materialName;
                        builders.push_back(std::move(builder));
                    }
                    current = &builders[found->second];
                }

                std::vector<std::uint32_t> corners;
                while (true)
                {
                    std::string_view word = NextWord(rest);
                    if (word.empty())
                    {
                        break;
                    }

                    // "v", "v/vt", "v//vn", or "v/vt/vn". Negative numbers count back from the latest.
                    int parts[3] = { 0, 0, 0 };
                    int part = 0;
                    std::size_t start = 0;
                    while (part < 3)
                    {
                        std::size_t slash = word.find('/', start);
                        std::string_view piece = word.substr(start, slash == std::string_view::npos ? slash : slash - start);
                        if (!piece.empty() && !ParseInt(piece, parts[part]))
                        {
                            return fail(lineNumber, std::format("\"{}\" is not a valid face corner", word));
                        }
                        ++part;
                        if (slash == std::string_view::npos)
                        {
                            break;
                        }
                        start = slash + 1;
                    }

                    auto resolve = [](int index, std::size_t count) {
                        return index < 0 ? static_cast<int>(count) + index : index - 1;
                    };
                    VertexKey key {
                        resolve(parts[0], positions.size()),
                        parts[1] == 0 ? -1 : resolve(parts[1], textureCoordinates.size()),
                        parts[2] == 0 ? -1 : resolve(parts[2], normals.size()),
                    };
                    if (parts[0] == 0 || key.Position < 0 || key.Position >= static_cast<int>(positions.size()))
                    {
                        return fail(lineNumber, std::format("a face uses vertex {}, but {} are defined so far", parts[0],
                            positions.size()));
                    }
                    if (key.TextureCoordinate >= static_cast<int>(textureCoordinates.size()) ||
                        (parts[1] != 0 && key.TextureCoordinate < 0))
                    {
                        return fail(lineNumber, std::format("a face uses texture coordinate {}, which is not defined", parts[1]));
                    }
                    if (key.Normal >= static_cast<int>(normals.size()) || (parts[2] != 0 && key.Normal < 0))
                    {
                        return fail(lineNumber, std::format("a face uses normal {}, which is not defined", parts[2]));
                    }

                    auto seen = current->Seen.find(key);
                    if (seen == current->Seen.end())
                    {
                        MeshVertex vertex;
                        vertex.Position = positions[static_cast<std::size_t>(key.Position)];
                        if (key.TextureCoordinate >= 0)
                        {
                            vertex.TextureCoordinate = textureCoordinates[static_cast<std::size_t>(key.TextureCoordinate)];
                        }
                        if (key.Normal >= 0)
                        {
                            vertex.Normal = normals[static_cast<std::size_t>(key.Normal)];
                        }
                        else
                        {
                            current->NeedsNormals = true;
                        }
                        auto index = static_cast<std::uint32_t>(current->Mesh.Vertices.size());
                        current->Mesh.Vertices.push_back(vertex);
                        current->PositionOf.push_back(key.Position);
                        seen = current->Seen.emplace(key, index).first;
                    }
                    corners.push_back(seen->second);
                }

                if (corners.size() < 3)
                {
                    return fail(lineNumber, "a face needs at least three corners");
                }
                // Polygons become a fan of triangles around their first corner.
                for (std::size_t corner = 1; corner + 1 < corners.size(); ++corner)
                {
                    current->Mesh.Indices.push_back(corners[0]);
                    current->Mesh.Indices.push_back(corners[corner]);
                    current->Mesh.Indices.push_back(corners[corner + 1]);
                }
            }
            // Other statements (smoothing groups, lines, points, curves) do not change the triangles.
        }

        for (MeshBuilder& builder : builders)
        {
            if (builder.Mesh.Indices.empty())
            {
                continue;
            }

            if (builder.NeedsNormals)
            {
                // Smooth normals: each position gets the area-weighted sum of the
                // normals of the faces around it, shared by every vertex at that position.
                std::unordered_map<int, Vector3> sums;
                const std::vector<MeshVertex>& vertices = builder.Mesh.Vertices;
                for (std::size_t index = 0; index + 2 < builder.Mesh.Indices.size(); index += 3)
                {
                    std::uint32_t a = builder.Mesh.Indices[index];
                    std::uint32_t b = builder.Mesh.Indices[index + 1];
                    std::uint32_t c = builder.Mesh.Indices[index + 2];
                    Vector3 faceNormal =
                        Cross(vertices[b].Position - vertices[a].Position, vertices[c].Position - vertices[a].Position);
                    sums[builder.PositionOf[a]] += faceNormal;
                    sums[builder.PositionOf[b]] += faceNormal;
                    sums[builder.PositionOf[c]] += faceNormal;
                }
                for (std::size_t vertex = 0; vertex < builder.Mesh.Vertices.size(); ++vertex)
                {
                    MeshVertex& target = builder.Mesh.Vertices[vertex];
                    if (LengthSquared(target.Normal) == 0.0f)
                    {
                        target.Normal = Normalize(sums[builder.PositionOf[vertex]]);
                    }
                }
            }

            for (std::size_t material = 0; material < model.Materials.size(); ++material)
            {
                if (model.Materials[material].Name == builder.MaterialName)
                {
                    builder.Mesh.MaterialIndex = static_cast<int>(material);
                }
            }
            model.Meshes.push_back(std::move(builder.Mesh));
        }

        if (model.Meshes.empty())
        {
            model.ErrorText = "it has no faces";
        }
        return model;
    }

    ModelData ModelData::Load(std::string_view path)
    {
        ModelData model;
        std::string extension = Files::ExtensionOf(path);
        if (extension == ".gltf" || extension == ".glb" || extension == ".fbx")
        {
            model.ErrorText = std::format("{}: {} models are not supported yet; easyforge reads OBJ", path, extension);
            return model;
        }
        if (extension != ".obj")
        {
            model.ErrorText = std::format("{}: it is not a model format easyforge can read (OBJ)", path);
            return model;
        }

        Result<std::vector<std::uint8_t>> bytes = Files::Read(path);
        if (!bytes)
        {
            model.ErrorText = bytes.Error();
            return model;
        }
        std::string_view objectText(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        std::string folder = Files::FolderOf(path);

        // Material libraries are named by "mtllib" lines, relative to the model.
        std::string materialText;
        std::string_view scan = objectText;
        while (!scan.empty())
        {
            std::size_t end = scan.find('\n');
            std::string_view line = scan.substr(0, end);
            scan = end == std::string_view::npos ? std::string_view() : scan.substr(end + 1);
            std::string_view rest = line;
            if (NextWord(rest) != "mtllib")
            {
                continue;
            }
            std::string library = folder + std::string(Trim(rest));
            Result<std::vector<std::uint8_t>> materials = Files::Read(library);
            if (materials)
            {
                materialText.append(reinterpret_cast<const char*>(materials->data()), materials->size());
                materialText += '\n';
            }
            else
            {
                Log(LogLevel::Warning, "{}: its material library could not be read, so it has no materials: {}", path,
                    materials.Error());
            }
        }

        model = DecodeObj(objectText, materialText, folder);
        if (!model.ErrorText.empty())
        {
            model.ErrorText = std::format("{}: {}", path, model.ErrorText);
        }
        return model;
    }

    Pending<ModelData> ModelData::LoadInBackground(std::string_view path)
    {
        return Pending<ModelData>(Jobs::Shared().Run([path = std::string(path)] { return Load(path); }));
    }

    BoundingBox ModelData::Bounds() const
    {
        BoundingBox box;
        for (const MeshData& mesh : Meshes)
        {
            for (const MeshVertex& vertex : mesh.Vertices)
            {
                box.Include(vertex.Position);
            }
        }
        return box;
    }
}
