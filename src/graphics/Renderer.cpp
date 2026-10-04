#include <easyforge/graphics/Renderer.h>

#include <algorithm>
#include <array>

#include <easyforge/core/Log.h>
#include <easyforge/core/Scalar.h>

#include "RendererState.h"

namespace easyforge
{
    namespace internal
    {
        namespace
        {
            constexpr std::array<gpu::VertexAttribute, 11> ShapeAttributes = { {
                { "ORIGIN", gpu::VertexFormat::Float2, 0 },
                { "AXIS_X", gpu::VertexFormat::Float2, 8 },
                { "AXIS_Y", gpu::VertexFormat::Float2, 16 },
                { "SIZE", gpu::VertexFormat::Float2, 24 },
                { "COORDINATES", gpu::VertexFormat::Float4, 32 },
                { "FILL", gpu::VertexFormat::Float4, 48 },
                { "BORDER", gpu::VertexFormat::Float4, 64 },
                { "SHAPE", gpu::VertexFormat::Float4, 80 },
                { "CLIP", gpu::VertexFormat::Float4, 96 },
                { "GRADIENT", gpu::VertexFormat::Float4, 112 },
                { "GRADIENT_LINE", gpu::VertexFormat::Float4, 128 },
            } };
            static_assert(sizeof(ShapeInstance) == 144);

            // The blur shader's constants, in the order of its Blur buffer.
            struct BlurConstants
            {
                float Step[2];
                float Radius;
                float Spread;

                // The last place to read, in texture coordinates: the picture can be
                // larger than the area copied into its top left.
                float Limit[2];
                float Reserved[2];
            };

            // Pictures for layers and blurs are made in steps of this many pixels,
            // so an area that grows or shrinks a little keeps its picture.
            constexpr int PictureStep = 64;

            int Stepped(int size)
            {
                return (Max(size, 1) + PictureStep - 1) / PictureStep * PictureStep;
            }

            // Frames a picture may go unused before it is let go.
            constexpr std::uint64_t PictureFrames = 120;

            constexpr std::array<gpu::VertexAttribute, 3> MeshAttributes = { {
                { "POSITION", gpu::VertexFormat::Float3, 0 },
                { "NORMAL", gpu::VertexFormat::Float3, 12 },
                { "TEXCOORD", gpu::VertexFormat::Float2, 24 },
            } };

            // The mesh shader's constants, in the order of its Object buffer.
            struct MeshConstants
            {
                Matrix4 World;
                Matrix4 ViewProjection;
                Matrix4 NormalMatrix;
                float BaseColor[4];
                float TowardSun[4];
                float SunColor[4];
                float Ambient[4];
                float CameraPosition[4];
                float Material[4];
            };

            void Store(float (&target)[4], Color color)
            {
                target[0] = color.Red;
                target[1] = color.Green;
                target[2] = color.Blue;
                target[3] = color.Alpha;
            }
        }

        RendererState::~RendererState()
        {
            if (Context)
            {
                Context->WaitUntilIdle();
            }
            if (TheHost && ClaimedSurface)
            {
                TheHost->ReleaseSurface();
            }
        }

        Result<> RendererState::Start(std::shared_ptr<Host> host, const RendererSettings& settings)
        {
            Settings = settings;
            if (host)
            {
                if (!host->IsOpen())
                {
                    return Failure("the window is closed");
                }
                if (!host->ClaimSurface())
                {
                    return Failure("the window already has a renderer. A window has one renderer; with an interface "
                                   "from ui, draw inside ui::DrawingArea or ui::SceneView instead");
                }
                ClaimedSurface = true;
                TheHost = std::move(host);
            }
            else if (settings.Width <= 0 || settings.Height <= 0)
            {
                return Failure("a renderer without a host needs a Width and Height in pixels");
            }

            Result<std::shared_ptr<gpu::Device>> device = gpu::SharedDevice(settings.Adapter);
            if (!device)
            {
                return Failure(device.Error());
            }
            Device = *device;
            Resources = DeviceResources::For(Device);
            Context = Device->CreateContext();

            if (TheHost)
            {
                Vector2 pixels = TheHost->PixelSize();
                Result<std::unique_ptr<gpu::Surface>> surface = Device->CreateSurface(TheHost->NativeSurface(),
                    static_cast<int>(pixels.X), static_cast<int>(pixels.Y), TheHost->IsTransparent());
                if (!surface)
                {
                    return Failure(surface.Error());
                }
                Surface = std::move(surface).Get();
            }
            else
            {
                Offscreen = Device->CreateTexture(
                    { .Width = settings.Width, .Height = settings.Height, .RenderTarget = true, .Name = "renderer image" });
                if (!Offscreen)
                {
                    return Failure("the GPU could not make an image of that size");
                }
            }

            gpu::BuiltInShaders shaders = gpu::ShadersOfBackend();
            Result<std::unique_ptr<gpu::Pipeline>> shape = Device->CreatePipeline({
                .VertexShader = shaders.Shape,
                .PixelShader = shaders.Shape,
                .Attributes = ShapeAttributes,
                .VertexStride = sizeof(ShapeInstance),
                .PerInstance = true,
                .TriangleStrip = true,
                .Blend = gpu::Blending::Premultiplied,
                .Name = "shapes",
            });
            if (!shape)
            {
                return Failure(shape.Error());
            }
            ShapePipeline = std::move(shape).Get();

            Result<std::unique_ptr<gpu::Pipeline>> replace = Device->CreatePipeline({
                .VertexShader = shaders.Shape,
                .PixelShader = shaders.Shape,
                .Attributes = ShapeAttributes,
                .VertexStride = sizeof(ShapeInstance),
                .PerInstance = true,
                .TriangleStrip = true,
                .Name = "replacing shapes",
            });
            if (!replace)
            {
                return Failure(replace.Error());
            }
            ReplacePipeline = std::move(replace).Get();

            Result<std::unique_ptr<gpu::Pipeline>> mesh = Device->CreatePipeline({
                .VertexShader = shaders.Mesh,
                .PixelShader = shaders.Mesh,
                .Attributes = MeshAttributes,
                .VertexStride = 32,
                .ColorFormat = gpu::TextureFormat::Rgba8,
                .DepthTest = true,
                .CullBackFaces = false,
                .SampleCount = SceneSamples,
                .Name = "meshes",
            });
            if (!mesh)
            {
                return Failure(mesh.Error());
            }
            MeshPipeline = std::move(mesh).Get();

            Result<std::unique_ptr<gpu::Pipeline>> blur = Device->CreatePipeline({
                .VertexShader = shaders.Blur,
                .PixelShader = shaders.Blur,
                .TriangleStrip = true,
                .ColorFormat = gpu::TextureFormat::Rgba8,
                .Name = "blur",
            });
            if (!blur)
            {
                return Failure(blur.Error());
            }
            BlurPipeline = std::move(blur).Get();
            Made = true;
            return {};
        }

        Vector2 RendererState::PixelSize() const
        {
            if (TheHost)
            {
                return TheHost->PixelSize();
            }
            return { static_cast<float>(Settings.Width), static_cast<float>(Settings.Height) };
        }

        float RendererState::Scale() const
        {
            return TheHost ? TheHost->Scale() : Max(Settings.Scale, 0.01f);
        }

        gpu::Texture* RendererState::SceneTarget(std::size_t index, int width, int height)
        {
            width = Max(width, 1);
            height = Max(height, 1);
            if (SceneTargetPool.size() <= index)
            {
                SceneTargetPool.resize(index + 1);
            }
            SceneTargets& targets = SceneTargetPool[index];
            if (!targets.Resolved || targets.Resolved->Width() != width || targets.Resolved->Height() != height)
            {
                targets.Color = Device->CreateTexture({ .Width = width, .Height = height, .RenderTarget = true,
                    .SampleCount = SceneSamples, .Name = "scene" });
                targets.Depth = Device->CreateTexture({ .Width = width, .Height = height,
                    .Format = gpu::TextureFormat::Depth32, .SampleCount = SceneSamples, .Name = "scene depth" });
                targets.Resolved = Device->CreateTexture(
                    { .Width = width, .Height = height, .RenderTarget = true, .Name = "scene picture" });
            }
            return targets.Resolved.get();
        }

        gpu::Texture* RendererState::LayerTarget(std::size_t index, int width, int height)
        {
            if (LayerTargetPool.size() <= index)
            {
                LayerTargetPool.resize(index + 1);
            }
            PooledPicture& pooled = LayerTargetPool[index];
            int pictureWidth = Stepped(width);
            int pictureHeight = Stepped(height);
            if (!pooled.Picture || pooled.Picture->Width() != pictureWidth || pooled.Picture->Height() != pictureHeight)
            {
                pooled.Picture =
                    Device->CreateTexture({ .Width = pictureWidth, .Height = pictureHeight, .RenderTarget = true, .Name = "layer" });
            }
            pooled.LastFrame = FrameNumber;
            return pooled.Picture.get();
        }

        std::array<gpu::Texture*, 3> RendererState::BlurTargets(std::size_t index, int width, int height)
        {
            if (BlurTargetPool.size() <= index)
            {
                BlurTargetPool.resize(index + 1);
            }
            std::array<PooledPicture, 3>& targets = BlurTargetPool[index];
            int pictureWidth = Stepped(width);
            int pictureHeight = Stepped(height);
            for (PooledPicture& target : targets)
            {
                if (!target.Picture || target.Picture->Width() != pictureWidth || target.Picture->Height() != pictureHeight)
                {
                    target.Picture =
                        Device->CreateTexture({ .Width = pictureWidth, .Height = pictureHeight, .RenderTarget = true, .Name = "blur" });
                }
                target.LastFrame = FrameNumber;
            }
            return { targets[0].Picture.get(), targets[1].Picture.get(), targets[2].Picture.get() };
        }

        void RendererState::LetGoOfUnusedPictures()
        {
            for (PooledPicture& pooled : LayerTargetPool)
            {
                if (pooled.Picture && pooled.LastFrame + PictureFrames < FrameNumber)
                {
                    pooled.Picture.reset();
                }
            }
            for (std::array<PooledPicture, 3>& targets : BlurTargetPool)
            {
                for (PooledPicture& pooled : targets)
                {
                    if (pooled.Picture && pooled.LastFrame + PictureFrames < FrameNumber)
                    {
                        pooled.Picture.reset();
                    }
                }
            }
        }

        void RendererState::DrawBlur(gpu::Commands& commands, const DrawStep& step, gpu::Texture& target)
        {
            gpu::Texture& first = *step.BlurTargets[0];
            gpu::Texture& second = *step.BlurTargets[1];
            gpu::ScissorRectangle area { static_cast<int>(step.BlurArea.X), static_cast<int>(step.BlurArea.Y),
                static_cast<int>(step.BlurArea.Width), static_cast<int>(step.BlurArea.Height) };
            commands.CopyTexture(target, area, first, 0, 0);
            commands.CopyTexture(target, area, *step.BlurTargets[2], 0, 0);

            // Across into the second picture, then down back into the first.
            float pictureWidth = static_cast<float>(first.Width());
            float pictureHeight = static_cast<float>(first.Height());
            BlurConstants constants { { 1.0f / pictureWidth, 0.0f }, step.BlurRadius, Max(step.BlurRadius * 0.5f, 0.5f),
                { (step.BlurArea.Width - 0.5f) / pictureWidth, (step.BlurArea.Height - 0.5f) / pictureHeight }, {} };
            for (int pass = 0; pass < 2; ++pass)
            {
                gpu::Texture& from = pass == 0 ? first : second;
                gpu::Texture& into = pass == 0 ? second : first;
                commands.BeginPass({ .ColorTarget = &into, .ClearColor = false });
                commands.SetPipeline(*BlurPipeline);
                commands.SetConstants(&constants, sizeof(constants));
                std::array<gpu::Texture*, 1> textures = { &from };
                commands.SetTextures(textures, gpu::Sampling::LinearClamp);
                commands.Draw(4);
                commands.EndPass();
                constants.Step[0] = 0.0f;
                constants.Step[1] = 1.0f / pictureHeight;
            }
        }

        void RendererState::DrawRecorded(gpu::Commands& commands, const internal::DrawList& list, gpu::Texture& target, Color clear)
        {
            commands.BeginPass({ .ColorTarget = &target, .Clear = clear });
            if (!list.Instances.empty())
            {
                commands.SetPipeline(*ShapePipeline);
                commands.SetVertices(list.Instances.data(), list.Instances.size() * sizeof(ShapeInstance));
            }
            float constants[4] = { static_cast<float>(target.Width()), static_cast<float>(target.Height()), 0.0f, 0.0f };
            bool shapesReady = false;
            for (const DrawStep& step : list.Steps)
            {
                std::array<gpu::Texture*, 1> textures = { step.Texture };
                if (step.Backdrop)
                {
                    // What is drawn so far has to be in the target before it can be
                    // blurred, so the pass ends here and starts again after.
                    commands.EndPass();
                    DrawBlur(commands, step, target);
                    commands.BeginPass({ .ColorTarget = &target, .ClearColor = false });
                    commands.SetPipeline(*ReplacePipeline);
                    commands.SetVertices(list.Instances.data(), list.Instances.size() * sizeof(ShapeInstance));
                    commands.SetConstants(constants, sizeof(constants));
                    std::array<gpu::Texture*, 2> pictures = { step.BlurTargets[0], step.BlurTargets[2] };
                    commands.SetTextures(pictures, gpu::Sampling::LinearClamp);
                    commands.Draw(4, 1, 0, step.First);
                    commands.SetPipeline(*ShapePipeline);
                    shapesReady = true;
                    continue;
                }
                if (step.Pipeline)
                {
                    commands.SetPipeline(*step.Pipeline);
                    commands.SetConstants(step.Constants.data(), step.Constants.size());
                    commands.SetTextures(textures, gpu::Sampling::LinearClamp);
                    commands.Draw(4);
                    shapesReady = false;
                    continue;
                }
                if (!shapesReady)
                {
                    // The vertex data stays bound while shaders come and go; the
                    // pipeline and constants are set again.
                    commands.SetPipeline(*ShapePipeline);
                    commands.SetConstants(constants, sizeof(constants));
                    shapesReady = true;
                }
                commands.SetTextures(textures, step.Sampling);
                commands.Draw(4, step.Count, 0, step.First);
            }
            commands.EndPass();
        }

        void RendererState::DrawScene(gpu::Commands& commands, const ScenePass& pass, SceneTargets& targets)
        {
            commands.BeginPass({ .ColorTarget = targets.Color.get(), .Clear = pass.Background,
                .DepthTarget = targets.Depth.get(), .ResolveTarget = targets.Resolved.get() });
            commands.SetPipeline(*MeshPipeline);

            const Camera& camera = pass.ViewPoint;
            float aspect = static_cast<float>(pass.Width) / static_cast<float>(Max(pass.Height, 1));
            Matrix4 view = Matrix4::LookAt(camera.Position, camera.Target, camera.Up);
            Matrix4 projection = Matrix4::Perspective(Radians(Clamp(camera.FieldOfView, 1.0f, 179.0f)), aspect,
                Max(camera.Near, 0.0001f), Max(camera.Far, camera.Near + 0.001f));

            MeshConstants constants {};
            constants.ViewProjection = projection * view;
            Vector3 towardSun = Length(pass.Light.Direction) > 0.0f ? Normalize(-pass.Light.Direction) : Vector3 { 0, 1, 0 };
            constants.TowardSun[0] = towardSun.X;
            constants.TowardSun[1] = towardSun.Y;
            constants.TowardSun[2] = towardSun.Z;
            Color sun = pass.Light.Color.ToLinear();
            Store(constants.SunColor, { sun.Red * pass.Light.Intensity, sun.Green * pass.Light.Intensity,
                                          sun.Blue * pass.Light.Intensity, 1.0f });
            Store(constants.Ambient, pass.Ambient.ToLinear());
            constants.CameraPosition[0] = camera.Position.X;
            constants.CameraPosition[1] = camera.Position.Y;
            constants.CameraPosition[2] = camera.Position.Z;

            for (const ScenePass::Object& object : pass.Objects)
            {
                const GpuModel& model = object.Model->On(Device);
                constants.World = object.World;
                constants.NormalMatrix = Transpose(Inverse(object.World));
                for (const GpuMesh& mesh : model.Meshes)
                {
                    const ModelData& data = object.Model->Data;
                    MaterialData material;
                    gpu::Texture* texture = nullptr;
                    gpu::Sampling sampling = gpu::Sampling::LinearRepeat;
                    if (mesh.MaterialIndex >= 0 && mesh.MaterialIndex < static_cast<int>(data.Materials.size()))
                    {
                        std::size_t index = static_cast<std::size_t>(mesh.MaterialIndex);
                        material = data.Materials[index];
                        const Texture& picture = object.Model->MaterialTextures[index];
                        if (picture)
                        {
                            texture = picture.State()->On(Device);
                            sampling = picture.State()->Sampling();
                        }
                    }
                    Store(constants.BaseColor, material.BaseColor.ToLinear());
                    constants.Material[0] = texture ? 1.0f : 0.0f;
                    constants.Material[1] = Clamp(material.Roughness, 0.0f, 1.0f);

                    commands.SetConstants(&constants, sizeof(constants));
                    std::array<gpu::Texture*, 1> textures = { texture };
                    commands.SetTextures(textures, sampling);
                    commands.SetVertexBuffer(*mesh.Vertices);
                    commands.SetIndexBuffer(*mesh.Indices);
                    commands.DrawIndexed(mesh.IndexCount);
                }
            }
            commands.EndPass();
        }

        void RendererState::Finish()
        {
            FrameRecorder& frame = Frame;
            frame.Active = false;
            if (frame.ListStack.size() > 1)
            {
                Log(LogLevel::Warning, "A frame ended with {} layers still open; call EndLayer for each BeginLayer",
                    frame.ListStack.size() - 1);
            }
            if (TheHost && !TheHost->IsOpen())
            {
                frame.Lists.clear();
                frame.Scenes.clear();
                frame.Used.clear();
                return;
            }

            gpu::Commands& commands = Context->BeginFrame();
            for (std::size_t index = 0; index < frame.Scenes.size(); ++index)
            {
                DrawScene(commands, frame.Scenes[index], SceneTargetPool[index]);
            }
            // Layers that were never ended are drawn too, so their pictures are
            // not left over from another frame.
            for (std::size_t index = 1; index < frame.Lists.size(); ++index)
            {
                if (std::find(frame.FinishedLayers.begin(), frame.FinishedLayers.end(), index) == frame.FinishedLayers.end())
                {
                    frame.FinishedLayers.push_back(index);
                }
            }
            for (std::size_t index : frame.FinishedLayers)
            {
                // A layer whose picture could not be made is left out.
                const internal::DrawList& layer = frame.Lists[index];
                if (layer.Target)
                {
                    DrawRecorded(commands, layer, *layer.Target, Color::Transparent);
                }
            }

            gpu::Texture& target = Surface ? Surface->CurrentTexture() : *Offscreen;
            DrawRecorded(commands, frame.Lists[0], target, Clear);
            Context->EndFrame(Surface.get(), TheHost ? TheHost->VerticalSync() : false);

            frame.Lists.clear();
            frame.Scenes.clear();
            frame.Used.clear();
            ++FrameNumber;
            LetGoOfUnusedPictures();
        }
    }

    using internal::RendererState;

    Renderer Renderer::New(std::shared_ptr<Host> host, const RendererSettings& settings)
    {
        auto state = std::make_shared<RendererState>();
        if (!host)
        {
            state->ErrorText = "Renderer::New was given no host; to draw into an image, pass only the settings";
            return Renderer(state);
        }
        Result<> started = state->Start(std::move(host), settings);
        if (!started)
        {
            state->ErrorText = started.Error();
        }
        return Renderer(state);
    }

    Renderer Renderer::New(const RendererSettings& settings)
    {
        auto state = std::make_shared<RendererState>();
        Result<> started = state->Start(nullptr, settings);
        if (!started)
        {
            state->ErrorText = started.Error();
        }
        return Renderer(state);
    }

    Renderer::Renderer() : State(std::make_shared<RendererState>())
    {
    }

    Renderer::Renderer(std::shared_ptr<internal::RendererState> state) : State(std::move(state))
    {
    }

    Renderer::operator bool() const
    {
        return State->Made;
    }

    const std::string& Renderer::Error() const
    {
        return State->ErrorText;
    }

    Canvas Renderer::BeginFrame(Color clear) const
    {
        if (!State->Made)
        {
            return Canvas();
        }
        if (State->Frame.Active)
        {
            Log(LogLevel::Warning, "Renderer::BeginFrame was called twice without EndFrame; the first frame is dropped");
        }
        Vector2 pixels = State->PixelSize();
        if (State->Surface)
        {
            State->Surface->Resize(static_cast<int>(pixels.X), static_cast<int>(pixels.Y));
            pixels = { static_cast<float>(State->Surface->Width()), static_cast<float>(State->Surface->Height()) };
        }
        State->Clear = clear;
        State->BlursRecorded = 0;
        State->Frame.Begin(pixels, State->Scale(), static_cast<float>(State->SinceStart.Seconds()));
        return Canvas(&State->Frame);
    }

    void Renderer::EndFrame() const
    {
        if (State->Made && State->Frame.Active)
        {
            State->Finish();
        }
    }

    ImageData Renderer::Capture() const
    {
        if (!State->Made)
        {
            return {};
        }
        if (State->Surface)
        {
            return State->Surface->ReadLastShown();
        }
        return State->Device->ReadTexture(*State->Offscreen);
    }

    Vector2 Renderer::Size() const
    {
        return State->Made ? State->PixelSize() / State->Scale() : Vector2 {};
    }

    Vector2 Renderer::PixelSize() const
    {
        return State->Made ? State->PixelSize() : Vector2 {};
    }

    float Renderer::Scale() const
    {
        return State->Made ? State->Scale() : 1.0f;
    }

    std::string Renderer::Description() const
    {
        return State->Device ? State->Device->Description() : std::string();
    }
}
