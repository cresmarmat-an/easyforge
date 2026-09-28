#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include <easyforge/core/Log.h>

#include "../gpu/Gpu.h"
#include "Direct3D12.h"

namespace easyforge::internal::direct3d12
{
    namespace
    {
        constexpr int FramesInFlight = 2;
        constexpr UINT TextureSlots = 4;

        // Shader-visible descriptors: each context takes a slice for each frame
        // slot, and binding textures uses four at a time.
        constexpr UINT VisibleDescriptors = 262144;
        constexpr UINT DescriptorsPerSlot = 16384;
        constexpr UINT SlicesAvailable = VisibleDescriptors / (DescriptorsPerSlot * FramesInFlight);

        constexpr UINT64 UploadPageSize = 1024 * 1024;

        UINT64 AlignUp(UINT64 value, UINT64 alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        DXGI_FORMAT ToFormat(gpu::TextureFormat format)
        {
            switch (format)
            {
            case gpu::TextureFormat::Rgba8: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case gpu::TextureFormat::R8: return DXGI_FORMAT_R8_UNORM;
            case gpu::TextureFormat::Surface: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case gpu::TextureFormat::Depth32: return DXGI_FORMAT_D32_FLOAT;
            }
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        std::wstring ToWide(std::string_view text)
        {
            return std::wstring(text.begin(), text.end());
        }

        D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER barrier {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            return barrier;
        }

        constexpr D3D12_RESOURCE_STATES ShaderResource =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    class Device;

    // CPU descriptors of one kind, handed out and taken back by index.
    class DescriptorPool
    {
    public:
        void Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity)
        {
            D3D12_DESCRIPTOR_HEAP_DESC description {};
            description.Type = type;
            description.NumDescriptors = capacity;
            device->CreateDescriptorHeap(&description, IID_PPV_ARGS(&Heap));
            Increment = device->GetDescriptorHandleIncrementSize(type);
            Capacity = capacity;
        }

        UINT Allocate()
        {
            if (!Free.empty())
            {
                UINT index = Free.back();
                Free.pop_back();
                return index;
            }
            return Next < Capacity ? Next++ : Invalid;
        }

        void Release(UINT index)
        {
            if (index != Invalid)
            {
                Free.push_back(index);
            }
        }

        D3D12_CPU_DESCRIPTOR_HANDLE Handle(UINT index) const
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = Heap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(index) * Increment;
            return handle;
        }

        static constexpr UINT Invalid = ~0u;

    private:
        ComPtr<ID3D12DescriptorHeap> Heap;
        UINT Increment = 0;
        UINT Capacity = 0;
        UINT Next = 0;
        std::vector<UINT> Free;
    };

    // Upload memory handed out in order from pages, all of which are reused once
    // the GPU has finished the work that read them.
    class LinearUploader
    {
    public:
        struct Allocation
        {
            ID3D12Resource* Resource = nullptr;
            UINT64 Offset = 0;
            std::uint8_t* Data = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS Address = 0;
        };

        Allocation Allocate(ID3D12Device* device, UINT64 size, UINT64 alignment)
        {
            while (true)
            {
                if (Current < Pages.size())
                {
                    Page& page = Pages[Current];
                    UINT64 offset = AlignUp(Offset, alignment);
                    if (offset + size <= page.Size)
                    {
                        Offset = offset + size;
                        return { page.Resource.Get(), offset, page.Mapped + offset, page.Resource->GetGPUVirtualAddress() + offset };
                    }
                    ++Current;
                    Offset = 0;
                    continue;
                }
                Pages.push_back(CreatePage(device, std::max(UploadPageSize, AlignUp(size, 65536))));
            }
        }

        // Every page can be written again: the GPU has finished with them.
        void Reset()
        {
            Current = 0;
            Offset = 0;
        }

    private:
        struct Page
        {
            ComPtr<ID3D12Resource> Resource;
            std::uint8_t* Mapped = nullptr;
            UINT64 Size = 0;
        };

        static Page CreatePage(ID3D12Device* device, UINT64 size)
        {
            D3D12_HEAP_PROPERTIES heap {};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC description {};
            description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            description.Width = size;
            description.Height = 1;
            description.DepthOrArraySize = 1;
            description.MipLevels = 1;
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            Page page;
            page.Size = size;
            device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, IID_PPV_ARGS(&page.Resource));
            D3D12_RANGE nothing { 0, 0 };
            page.Resource->Map(0, &nothing, reinterpret_cast<void**>(&page.Mapped));
            return page;
        }

        std::vector<Page> Pages;
        std::size_t Current = 0;
        UINT64 Offset = 0;
    };

    class Texture final : public gpu::Texture
    {
    public:
        Texture(Device& owner, ComPtr<ID3D12Resource> resource, gpu::TextureFormat format, int width, int height,
            int sampleCount, D3D12_RESOURCE_STATES state, bool owned);
        ~Texture() override;

        int Width() const override { return TextureWidth; }
        int Height() const override { return TextureHeight; }
        gpu::TextureFormat Format() const override { return TextureFormat; }

        Device& Owner;
        ComPtr<ID3D12Resource> Resource;
        gpu::TextureFormat TextureFormat;
        int TextureWidth;
        int TextureHeight;
        int SampleCount;
        D3D12_RESOURCE_STATES State;
        UINT ShaderView = DescriptorPool::Invalid;
        UINT TargetView = DescriptorPool::Invalid;
        UINT DepthView = DescriptorPool::Invalid;
        bool Owned;
    };

    class Buffer final : public gpu::Buffer
    {
    public:
        Buffer(Device& owner, ComPtr<ID3D12Resource> resource, std::size_t size, bool indices);
        ~Buffer() override;
        std::size_t Size() const override { return BufferSize; }

        Device& Owner;
        ComPtr<ID3D12Resource> Resource;
        std::size_t BufferSize;
        bool Indices;
    };

    class Pipeline final : public gpu::Pipeline
    {
    public:
        explicit Pipeline(Device& owner) : Owner(owner) {}
        ~Pipeline() override;

        Device& Owner;
        ComPtr<ID3D12PipelineState> State;
        D3D_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        UINT Stride = 0;
    };

    class Surface;

    class Device final : public gpu::Device
    {
    public:
        static Result<std::shared_ptr<Device>> Create(GraphicsAdapter adapter);
        ~Device() override;

        std::string Description() const override { return DescriptionText; }
        std::unique_ptr<gpu::Texture> CreateTexture(const gpu::TextureSettings& settings) override;
        void UploadTexture(gpu::Texture& texture, int x, int y, int width, int height, const std::uint8_t* pixels,
            std::size_t stride) override;
        std::unique_ptr<gpu::Buffer> CreateBuffer(std::span<const std::uint8_t> data, bool indices, std::string_view name) override;
        Result<std::unique_ptr<gpu::Pipeline>> CreatePipeline(const gpu::PipelineSettings& settings) override;
        Result<std::unique_ptr<gpu::Surface>> CreateSurface(const easyforge::Surface& native, int width, int height,
            bool transparent) override;
        std::unique_ptr<gpu::Context> CreateContext() override;
        ImageData ReadTexture(gpu::Texture& texture) override;
        std::uint64_t Identifier() const override { return UniqueNumber; }

        // Keeps an object alive until the GPU can no longer be using it.
        void Retire(ComPtr<IUnknown> object);
        void CollectGarbage();

        // Sends the pending uploads together with `lists`, and signals the fence.
        UINT64 Submit(std::span<ID3D12CommandList* const> lists);
        void WaitFor(UINT64 fence);
        void WaitUntilIdle() { WaitFor(Submit({})); }
        UINT64 CompletedFence() const { return Fence->GetCompletedValue(); }

        // Reports what the validation layer found, when it is on.
        void ReportValidationMessages();

        ComPtr<IDXGIFactory4> Factory;
        ComPtr<ID3D12Device> Device3D;
        ComPtr<ID3D12CommandQueue> Queue;
        ComPtr<ID3D12RootSignature> RootSignature;
        ComPtr<ID3D12DescriptorHeap> VisibleHeap;
        UINT VisibleIncrement = 0;
        DescriptorPool ShaderViews;
        DescriptorPool TargetViews;
        DescriptorPool DepthViews;
        UINT NullShaderView = DescriptorPool::Invalid;
        std::array<bool, SlicesAvailable> SliceUsed {};
        bool TearingSupported = false;
        int OpenRecordings = 0;

    private:
        struct UploadBatch
        {
            ComPtr<ID3D12CommandAllocator> Allocator;
            ComPtr<ID3D12GraphicsCommandList> List;
            LinearUploader Memory;
            UINT64 Fence = 0;
            bool Recording = false;
        };

        UploadBatch& CurrentUploads();

        ComPtr<ID3D12Fence> Fence;
        HANDLE FenceEvent = nullptr;
        UINT64 NextFence = 1;
        std::vector<std::pair<ComPtr<IUnknown>, UINT64>> Retired;
        std::vector<std::unique_ptr<UploadBatch>> UploadBatches;
        UploadBatch* OpenUploads = nullptr;
        std::map<std::string, ComPtr<ID3DBlob>, std::less<>> CompiledShaders;
        ComPtr<ID3D12InfoQueue> Validation;
        std::string DescriptionText;
        std::uint64_t UniqueNumber = 0;
    };

    Texture::Texture(Device& owner, ComPtr<ID3D12Resource> resource, gpu::TextureFormat format, int width, int height,
        int sampleCount, D3D12_RESOURCE_STATES state, bool owned)
        : Owner(owner), Resource(std::move(resource)), TextureFormat(format), TextureWidth(width), TextureHeight(height),
          SampleCount(sampleCount), State(state), Owned(owned)
    {
    }

    Texture::~Texture()
    {
        Owner.ShaderViews.Release(ShaderView);
        Owner.TargetViews.Release(TargetView);
        Owner.DepthViews.Release(DepthView);
        if (Owned)
        {
            Owner.Retire(Resource);
        }
    }

    Buffer::Buffer(Device& owner, ComPtr<ID3D12Resource> resource, std::size_t size, bool indices)
        : Owner(owner), Resource(std::move(resource)), BufferSize(size), Indices(indices)
    {
    }

    Buffer::~Buffer()
    {
        Owner.Retire(Resource);
    }

    // A frame still on the GPU may use the pipeline, so it is kept until then.
    Pipeline::~Pipeline()
    {
        Owner.Retire(State);
    }

    // ---- Surface ------------------------------------------------------------

    class Surface final : public gpu::Surface
    {
    public:
        Surface(Device& owner) : Owner(owner) {}
        ~Surface() override
        {
            Owner.WaitUntilIdle();
            Buffers = {};
        }

        void Resize(int width, int height) override;
        int Width() const override { return SurfaceWidth; }
        int Height() const override { return SurfaceHeight; }
        gpu::Texture& CurrentTexture() override { return *Buffers[SwapChain->GetCurrentBackBufferIndex()]; }
        ImageData ReadLastShown() override;

        void TakeBuffers();
        void Present(bool verticalSync);

        Device& Owner;
        ComPtr<IDXGISwapChain3> SwapChain;
        ComPtr<IDCompositionDevice> Composition;
        ComPtr<IDCompositionTarget> CompositionTarget;
        ComPtr<IDCompositionVisual> CompositionVisual;
        std::array<std::unique_ptr<Texture>, 2> Buffers;
        int SurfaceWidth = 0;
        int SurfaceHeight = 0;
        UINT Flags = 0;
        UINT LastShown = ~0u;
    };

    void Surface::TakeBuffers()
    {
        for (UINT index = 0; index < Buffers.size(); ++index)
        {
            ComPtr<ID3D12Resource> buffer;
            SwapChain->GetBuffer(index, IID_PPV_ARGS(&buffer));
            auto texture = std::make_unique<Texture>(Owner, buffer, gpu::TextureFormat::Surface, SurfaceWidth,
                SurfaceHeight, 1, D3D12_RESOURCE_STATE_PRESENT, false);
            texture->TargetView = Owner.TargetViews.Allocate();
            Owner.Device3D->CreateRenderTargetView(buffer.Get(), nullptr, Owner.TargetViews.Handle(texture->TargetView));
            Buffers[index] = std::move(texture);
        }
    }

    void Surface::Resize(int width, int height)
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (width == SurfaceWidth && height == SurfaceHeight)
        {
            return;
        }
        Owner.WaitUntilIdle();
        Buffers = {};
        SwapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, Flags);
        SurfaceWidth = width;
        SurfaceHeight = height;
        LastShown = ~0u;
        TakeBuffers();
    }

    void Surface::Present(bool verticalSync)
    {
        LastShown = SwapChain->GetCurrentBackBufferIndex();
        UINT presentFlags = !verticalSync && (Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        SwapChain->Present(verticalSync ? 1 : 0, presentFlags);
    }

    ImageData Surface::ReadLastShown()
    {
        if (LastShown >= Buffers.size())
        {
            return {};
        }
        return Owner.ReadTexture(*Buffers[LastShown]);
    }

    // ---- Context and commands --------------------------------------------------

    class Context final : public gpu::Context, public gpu::Commands
    {
    public:
        explicit Context(Device& owner);
        ~Context() override;

        gpu::Commands& BeginFrame() override;
        void EndFrame(gpu::Surface* surface, bool verticalSync) override;
        void WaitUntilIdle() override;

        void BeginPass(const gpu::PassSettings& settings) override;
        void EndPass() override;
        void SetPipeline(gpu::Pipeline& pipeline) override;
        void SetTextures(std::span<gpu::Texture* const> textures, gpu::Sampling sampling) override;
        void SetConstants(const void* data, std::size_t size) override;
        void SetVertices(const void* data, std::size_t size) override;
        void SetVertexBuffer(gpu::Buffer& buffer) override;
        void SetIndexBuffer(gpu::Buffer& buffer) override;
        void SetScissor(gpu::ScissorRectangle rectangle) override;
        void Draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
            std::uint32_t firstInstance) override;
        void DrawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex, std::int32_t baseVertex) override;

    private:
        void Move(Texture& texture, D3D12_RESOURCE_STATES state);

        struct Slot
        {
            ComPtr<ID3D12CommandAllocator> Allocator;
            UINT64 Fence = 0;
            LinearUploader Upload;
            UINT DescriptorStart = 0;
            UINT DescriptorsUsed = 0;
        };

        Device& Owner;
        std::array<Slot, FramesInFlight> Slots;
        ComPtr<ID3D12GraphicsCommandList> List;
        UINT Slice = 0;
        int Current = 0;
        bool Recording = false;
        Texture* ColorTarget = nullptr;
        Texture* ResolveTarget = nullptr;
        Pipeline* CurrentPipeline = nullptr;
    };

    Context::Context(Device& owner) : Owner(owner)
    {
        for (UINT slice = 0; slice < SlicesAvailable; ++slice)
        {
            if (!Owner.SliceUsed[slice])
            {
                Owner.SliceUsed[slice] = true;
                Slice = slice;
                break;
            }
        }
        for (int index = 0; index < FramesInFlight; ++index)
        {
            Owner.Device3D->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Slots[index].Allocator));
            Slots[index].DescriptorStart = (Slice * FramesInFlight + static_cast<UINT>(index)) * DescriptorsPerSlot;
        }
        Owner.Device3D->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Slots[0].Allocator.Get(), nullptr,
            IID_PPV_ARGS(&List));
        List->Close();
    }

    Context::~Context()
    {
        WaitUntilIdle();
        Owner.SliceUsed[Slice] = false;
    }

    void Context::WaitUntilIdle()
    {
        for (const Slot& slot : Slots)
        {
            Owner.WaitFor(slot.Fence);
        }
    }

    gpu::Commands& Context::BeginFrame()
    {
        Slot& slot = Slots[static_cast<std::size_t>(Current)];
        Owner.WaitFor(slot.Fence);
        Owner.CollectGarbage();
        slot.Allocator->Reset();
        slot.Upload.Reset();
        slot.DescriptorsUsed = 0;
        List->Reset(slot.Allocator.Get(), nullptr);

        ID3D12DescriptorHeap* heaps[] = { Owner.VisibleHeap.Get() };
        List->SetDescriptorHeaps(1, heaps);
        List->SetGraphicsRootSignature(Owner.RootSignature.Get());
        Recording = true;
        ++Owner.OpenRecordings;
        return *this;
    }

    void Context::EndFrame(gpu::Surface* surface, bool verticalSync)
    {
        auto* windowSurface = static_cast<Surface*>(surface);
        if (windowSurface)
        {
            Move(static_cast<Texture&>(windowSurface->CurrentTexture()), D3D12_RESOURCE_STATE_PRESENT);
        }
        List->Close();
        Recording = false;
        --Owner.OpenRecordings;

        ID3D12CommandList* lists[] = { List.Get() };
        UINT64 fence = Owner.Submit(lists);
        if (windowSurface)
        {
            windowSurface->Present(verticalSync);
        }
        Slots[static_cast<std::size_t>(Current)].Fence = fence;
        Current = (Current + 1) % FramesInFlight;
        Owner.ReportValidationMessages();
    }

    void Context::Move(Texture& texture, D3D12_RESOURCE_STATES state)
    {
        if (texture.State == state)
        {
            return;
        }
        D3D12_RESOURCE_BARRIER barrier = Transition(texture.Resource.Get(), texture.State, state);
        List->ResourceBarrier(1, &barrier);
        texture.State = state;
    }

    void Context::BeginPass(const gpu::PassSettings& settings)
    {
        ColorTarget = static_cast<Texture*>(settings.ColorTarget);
        ResolveTarget = static_cast<Texture*>(settings.ResolveTarget);
        auto* depth = static_cast<Texture*>(settings.DepthTarget);

        Move(*ColorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
        D3D12_CPU_DESCRIPTOR_HANDLE color = Owner.TargetViews.Handle(ColorTarget->TargetView);
        D3D12_CPU_DESCRIPTOR_HANDLE depthHandle {};
        if (depth)
        {
            Move(*depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            depthHandle = Owner.DepthViews.Handle(depth->DepthView);
        }
        List->OMSetRenderTargets(1, &color, FALSE, depth ? &depthHandle : nullptr);

        if (settings.ClearColor)
        {
            // Targets hold colors multiplied by their alpha.
            const Color& clear = settings.Clear;
            float values[4] = { clear.Red * clear.Alpha, clear.Green * clear.Alpha, clear.Blue * clear.Alpha, clear.Alpha };
            List->ClearRenderTargetView(color, values, 0, nullptr);
        }
        if (depth)
        {
            List->ClearDepthStencilView(depthHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }

        D3D12_VIEWPORT viewport { 0.0f, 0.0f, static_cast<float>(ColorTarget->TextureWidth),
            static_cast<float>(ColorTarget->TextureHeight), 0.0f, 1.0f };
        List->RSSetViewports(1, &viewport);
        D3D12_RECT scissor { 0, 0, ColorTarget->TextureWidth, ColorTarget->TextureHeight };
        List->RSSetScissorRects(1, &scissor);
        CurrentPipeline = nullptr;
    }

    void Context::EndPass()
    {
        if (ResolveTarget)
        {
            Move(*ColorTarget, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            Move(*ResolveTarget, D3D12_RESOURCE_STATE_RESOLVE_DEST);
            List->ResolveSubresource(ResolveTarget->Resource.Get(), 0, ColorTarget->Resource.Get(), 0,
                ToFormat(ResolveTarget->TextureFormat));
            Move(*ResolveTarget, ShaderResource);
        }
        ColorTarget = nullptr;
        ResolveTarget = nullptr;
        CurrentPipeline = nullptr;
    }

    void Context::SetPipeline(gpu::Pipeline& pipeline)
    {
        auto& chosen = static_cast<Pipeline&>(pipeline);
        if (&chosen == CurrentPipeline)
        {
            return;
        }
        CurrentPipeline = &chosen;
        List->SetPipelineState(chosen.State.Get());
        List->IASetPrimitiveTopology(chosen.Topology);
    }

    void Context::SetTextures(std::span<gpu::Texture* const> textures, gpu::Sampling sampling)
    {
        Slot& slot = Slots[static_cast<std::size_t>(Current)];
        if (slot.DescriptorsUsed + TextureSlots > DescriptorsPerSlot)
        {
            // Out of descriptors for this frame: reuse the slice from the start.
            // Only a frame with thousands of texture changes gets here.
            slot.DescriptorsUsed = 0;
        }
        UINT first = slot.DescriptorStart + slot.DescriptorsUsed;
        slot.DescriptorsUsed += TextureSlots;

        D3D12_CPU_DESCRIPTOR_HANDLE destination = Owner.VisibleHeap->GetCPUDescriptorHandleForHeapStart();
        destination.ptr += static_cast<SIZE_T>(first) * Owner.VisibleIncrement;
        for (UINT index = 0; index < TextureSlots; ++index)
        {
            UINT view = Owner.NullShaderView;
            if (index < textures.size() && textures[index])
            {
                auto* texture = static_cast<Texture*>(textures[index]);
                Move(*texture, ShaderResource);
                view = texture->ShaderView;
            }
            D3D12_CPU_DESCRIPTOR_HANDLE target = destination;
            target.ptr += static_cast<SIZE_T>(index) * Owner.VisibleIncrement;
            Owner.Device3D->CopyDescriptorsSimple(1, target, Owner.ShaderViews.Handle(view),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        D3D12_GPU_DESCRIPTOR_HANDLE table = Owner.VisibleHeap->GetGPUDescriptorHandleForHeapStart();
        table.ptr += static_cast<UINT64>(first) * Owner.VisibleIncrement;
        List->SetGraphicsRootDescriptorTable(1, table);
        List->SetGraphicsRoot32BitConstant(2, static_cast<UINT>(sampling), 0);
    }

    void Context::SetConstants(const void* data, std::size_t size)
    {
        Slot& slot = Slots[static_cast<std::size_t>(Current)];
        LinearUploader::Allocation memory = slot.Upload.Allocate(Owner.Device3D.Get(), AlignUp(size, 256), 256);
        std::memcpy(memory.Data, data, size);
        List->SetGraphicsRootConstantBufferView(0, memory.Address);
    }

    void Context::SetVertices(const void* data, std::size_t size)
    {
        Slot& slot = Slots[static_cast<std::size_t>(Current)];
        LinearUploader::Allocation memory = slot.Upload.Allocate(Owner.Device3D.Get(), size, 16);
        std::memcpy(memory.Data, data, size);
        D3D12_VERTEX_BUFFER_VIEW view { memory.Address, static_cast<UINT>(size), CurrentPipeline ? CurrentPipeline->Stride : 0 };
        List->IASetVertexBuffers(0, 1, &view);
    }

    void Context::SetVertexBuffer(gpu::Buffer& buffer)
    {
        auto& chosen = static_cast<Buffer&>(buffer);
        D3D12_VERTEX_BUFFER_VIEW view { chosen.Resource->GetGPUVirtualAddress(), static_cast<UINT>(chosen.BufferSize),
            CurrentPipeline ? CurrentPipeline->Stride : 0 };
        List->IASetVertexBuffers(0, 1, &view);
    }

    void Context::SetIndexBuffer(gpu::Buffer& buffer)
    {
        auto& chosen = static_cast<Buffer&>(buffer);
        D3D12_INDEX_BUFFER_VIEW view { chosen.Resource->GetGPUVirtualAddress(), static_cast<UINT>(chosen.BufferSize),
            DXGI_FORMAT_R32_UINT };
        List->IASetIndexBuffer(&view);
    }

    void Context::SetScissor(gpu::ScissorRectangle rectangle)
    {
        D3D12_RECT scissor { rectangle.X, rectangle.Y, rectangle.X + rectangle.Width, rectangle.Y + rectangle.Height };
        List->RSSetScissorRects(1, &scissor);
    }

    void Context::Draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
        std::uint32_t firstInstance)
    {
        List->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void Context::DrawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex, std::int32_t baseVertex)
    {
        List->DrawIndexedInstanced(indexCount, 1, firstIndex, baseVertex, 0);
    }

    // ---- Device ---------------------------------------------------------------

    namespace
    {
        ComPtr<ID3D12RootSignature> MakeRootSignature(ID3D12Device* device, std::string& error)
        {
            D3D12_DESCRIPTOR_RANGE range {};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = TextureSlots;
            range.BaseShaderRegister = 0;
            range.OffsetInDescriptorsFromTableStart = 0;

            std::array<D3D12_ROOT_PARAMETER, 3> parameters {};
            parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            parameters[0].Descriptor.ShaderRegister = 0;
            parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameters[1].DescriptorTable.NumDescriptorRanges = 1;
            parameters[1].DescriptorTable.pDescriptorRanges = &range;
            parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            parameters[2].Constants.ShaderRegister = 1;
            parameters[2].Constants.Num32BitValues = 1;
            parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // The four ways of sampling, as s0 to s3, in the order of gpu::Sampling.
            std::array<D3D12_STATIC_SAMPLER_DESC, 4> samplers {};
            for (UINT index = 0; index < samplers.size(); ++index)
            {
                D3D12_STATIC_SAMPLER_DESC& sampler = samplers[index];
                bool linear = index < 2;
                bool repeat = index % 2 == 1;
                sampler.Filter = linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
                D3D12_TEXTURE_ADDRESS_MODE address = repeat ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                sampler.AddressU = address;
                sampler.AddressV = address;
                sampler.AddressW = address;
                sampler.MaxLOD = D3D12_FLOAT32_MAX;
                sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                sampler.ShaderRegister = index;
                sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            }

            D3D12_ROOT_SIGNATURE_DESC description {};
            description.NumParameters = static_cast<UINT>(parameters.size());
            description.pParameters = parameters.data();
            description.NumStaticSamplers = static_cast<UINT>(samplers.size());
            description.pStaticSamplers = samplers.data();
            description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> serialized;
            ComPtr<ID3DBlob> errors;
            HRESULT result = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
            if (FAILED(result))
            {
                error = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                               : Describe(result);
                return nullptr;
            }
            ComPtr<ID3D12RootSignature> signature;
            result = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                IID_PPV_ARGS(&signature));
            if (FAILED(result))
            {
                error = Describe(result);
                return nullptr;
            }
            return signature;
        }

        std::uint64_t NextDeviceNumber()
        {
            static std::uint64_t next = 1;
            return next++;
        }

        bool ValidationRequested()
        {
            char* value = nullptr;
            std::size_t length = 0;
            bool requested = _dupenv_s(&value, &length, "EASYFORGE_GRAPHICS_VALIDATION") == 0 && value &&
                             std::string_view(value) == "1";
            std::free(value);
            return requested;
        }
    }

    Result<std::shared_ptr<Device>> Device::Create(GraphicsAdapter adapter)
    {
        auto device = std::make_shared<Device>();
        // The validation layer can only be turned on before the first device is
        // made in a process.
        static const bool validation = [] {
            ComPtr<ID3D12Debug> debug;
            if (!ValidationRequested() || FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            {
                return false;
            }
            debug->EnableDebugLayer();
            return true;
        }();

        HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&device->Factory));
        if (FAILED(result))
        {
            return Failure("Direct3D 12 could not start: DXGI is not available (" + Describe(result) + ")");
        }

        // The adapter asked for, falling back to the software renderer.
        ComPtr<IDXGIAdapter1> chosen;
        ComPtr<IDXGIFactory6> ordered;
        if (adapter != GraphicsAdapter::Software && SUCCEEDED(device->Factory.As(&ordered)))
        {
            DXGI_GPU_PREFERENCE preference = adapter == GraphicsAdapter::LowPower ? DXGI_GPU_PREFERENCE_MINIMUM_POWER
                                                                                  : DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
            ComPtr<IDXGIAdapter1> candidate;
            for (UINT index = 0; ordered->EnumAdapterByGpuPreference(index, preference, IID_PPV_ARGS(&candidate)) !=
                                 DXGI_ERROR_NOT_FOUND;
                 ++index)
            {
                DXGI_ADAPTER_DESC1 description {};
                candidate->GetDesc1(&description);
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                    SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device->Device3D))))
                {
                    chosen = candidate;
                    break;
                }
            }
        }
        if (!chosen)
        {
            ComPtr<IDXGIAdapter> warp;
            device->Factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
            if (!warp || FAILED(warp.As(&chosen)) ||
                FAILED(D3D12CreateDevice(chosen.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device->Device3D))))
            {
                return Failure("Direct3D 12 could not start: no adapter supports it, not even the software renderer");
            }
        }

        DXGI_ADAPTER_DESC1 adapterDescription {};
        chosen->GetDesc1(&adapterDescription);
        std::wstring adapterName = adapterDescription.Description;
        std::string narrow;
        for (wchar_t character : adapterName)
        {
            narrow += character < 128 ? static_cast<char>(character) : '?';
        }
        device->DescriptionText = "Direct3D 12 on " + narrow;
        device->UniqueNumber = NextDeviceNumber();

        if (validation)
        {
            device->Device3D.As(&device->Validation);
        }

        D3D12_COMMAND_QUEUE_DESC queueDescription {};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->Device3D->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&device->Queue));
        device->Device3D->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&device->Fence));
        device->FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

        std::string error;
        device->RootSignature = MakeRootSignature(device->Device3D.Get(), error);
        if (!device->RootSignature)
        {
            return Failure("Direct3D 12 could not make its root signature: " + error);
        }

        D3D12_DESCRIPTOR_HEAP_DESC visible {};
        visible.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        visible.NumDescriptors = VisibleDescriptors;
        visible.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->Device3D->CreateDescriptorHeap(&visible, IID_PPV_ARGS(&device->VisibleHeap));
        device->VisibleIncrement = device->Device3D->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->ShaderViews.Create(device->Device3D.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16384);
        device->TargetViews.Create(device->Device3D.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024);
        device->DepthViews.Create(device->Device3D.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256);

        // Unused texture slots read from a view of nothing, which gives zeros.
        device->NullShaderView = device->ShaderViews.Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC nothing {};
        nothing.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        nothing.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nothing.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nothing.Texture2D.MipLevels = 1;
        device->Device3D->CreateShaderResourceView(nullptr, &nothing, device->ShaderViews.Handle(device->NullShaderView));

        ComPtr<IDXGIFactory5> tearing;
        BOOL allowTearing = FALSE;
        if (SUCCEEDED(device->Factory.As(&tearing)) &&
            SUCCEEDED(tearing->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        {
            device->TearingSupported = allowTearing != FALSE;
        }
        return device;
    }

    Device::~Device()
    {
        WaitUntilIdle();
        Retired.clear();
        if (FenceEvent)
        {
            CloseHandle(FenceEvent);
        }
    }

    void Device::Retire(ComPtr<IUnknown> object)
    {
        // Frames still being recorded are sent later and get later fence values.
        Retired.emplace_back(std::move(object), NextFence + static_cast<UINT64>(OpenRecordings));
    }

    void Device::CollectGarbage()
    {
        UINT64 completed = CompletedFence();
        std::erase_if(Retired, [&](const auto& entry) { return entry.second <= completed; });
        for (std::unique_ptr<UploadBatch>& batch : UploadBatches)
        {
            if (!batch->Recording && batch->Fence <= completed && batch.get() != OpenUploads)
            {
                batch->Memory.Reset();
            }
        }
    }

    Device::UploadBatch& Device::CurrentUploads()
    {
        if (OpenUploads)
        {
            return *OpenUploads;
        }
        UINT64 completed = CompletedFence();
        UploadBatch* free = nullptr;
        for (std::unique_ptr<UploadBatch>& batch : UploadBatches)
        {
            if (!batch->Recording && batch->Fence <= completed)
            {
                free = batch.get();
                break;
            }
        }
        if (!free)
        {
            auto batch = std::make_unique<UploadBatch>();
            Device3D->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&batch->Allocator));
            Device3D->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, batch->Allocator.Get(), nullptr,
                IID_PPV_ARGS(&batch->List));
            batch->List->Close();
            free = batch.get();
            UploadBatches.push_back(std::move(batch));
        }
        free->Allocator->Reset();
        free->List->Reset(free->Allocator.Get(), nullptr);
        free->Memory.Reset();
        free->Recording = true;
        OpenUploads = free;
        return *free;
    }

    UINT64 Device::Submit(std::span<ID3D12CommandList* const> lists)
    {
        std::vector<ID3D12CommandList*> all;
        UploadBatch* uploads = OpenUploads;
        if (uploads)
        {
            uploads->List->Close();
            all.push_back(uploads->List.Get());
            OpenUploads = nullptr;
        }
        all.insert(all.end(), lists.begin(), lists.end());
        if (!all.empty())
        {
            Queue->ExecuteCommandLists(static_cast<UINT>(all.size()), all.data());
        }
        UINT64 fence = NextFence++;
        Queue->Signal(Fence.Get(), fence);
        if (uploads)
        {
            uploads->Fence = fence;
            uploads->Recording = false;
        }
        return fence;
    }

    void Device::WaitFor(UINT64 fence)
    {
        if (fence == 0 || Fence->GetCompletedValue() >= fence)
        {
            return;
        }
        Fence->SetEventOnCompletion(fence, FenceEvent);
        WaitForSingleObject(FenceEvent, INFINITE);
    }

    void Device::ReportValidationMessages()
    {
        if (!Validation)
        {
            return;
        }
        UINT64 count = Validation->GetNumStoredMessages();
        for (UINT64 index = 0; index < count; ++index)
        {
            SIZE_T length = 0;
            Validation->GetMessage(index, nullptr, &length);
            std::vector<std::uint8_t> storage(length);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            if (SUCCEEDED(Validation->GetMessage(index, message, &length)) &&
                (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION))
            {
                Log(LogLevel::Error, "Direct3D 12 validation: {}", std::string_view(message->pDescription, message->DescriptionByteLength > 0 ? message->DescriptionByteLength - 1 : 0));
            }
        }
        Validation->ClearStoredMessages();
    }

    std::unique_ptr<gpu::Texture> Device::CreateTexture(const gpu::TextureSettings& settings)
    {
        D3D12_HEAP_PROPERTIES heap {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC description {};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = static_cast<UINT64>(std::max(settings.Width, 1));
        description.Height = static_cast<UINT>(std::max(settings.Height, 1));
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = ToFormat(settings.Format);
        description.SampleDesc.Count = static_cast<UINT>(std::max(settings.SampleCount, 1));
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        bool depth = settings.Format == gpu::TextureFormat::Depth32;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
        D3D12_CLEAR_VALUE clear {};
        D3D12_CLEAR_VALUE* clearValue = nullptr;
        if (depth)
        {
            description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            clear.Format = description.Format;
            clear.DepthStencil.Depth = 1.0f;
            clearValue = &clear;
        }
        else if (settings.RenderTarget)
        {
            description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            clear.Format = description.Format;
            clearValue = &clear;
        }

        ComPtr<ID3D12Resource> resource;
        if (FAILED(Device3D->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, state, clearValue,
                IID_PPV_ARGS(&resource))))
        {
            return nullptr;
        }
        if (!settings.Name.empty())
        {
            resource->SetName(ToWide(settings.Name).c_str());
        }

        auto texture = std::make_unique<Texture>(*this, resource, settings.Format, static_cast<int>(description.Width),
            static_cast<int>(description.Height), static_cast<int>(description.SampleDesc.Count), state, true);
        if (depth)
        {
            texture->DepthView = DepthViews.Allocate();
            Device3D->CreateDepthStencilView(resource.Get(), nullptr, DepthViews.Handle(texture->DepthView));
            return texture;
        }
        if (settings.RenderTarget)
        {
            texture->TargetView = TargetViews.Allocate();
            Device3D->CreateRenderTargetView(resource.Get(), nullptr, TargetViews.Handle(texture->TargetView));
        }
        if (description.SampleDesc.Count == 1)
        {
            texture->ShaderView = ShaderViews.Allocate();
            Device3D->CreateShaderResourceView(resource.Get(), nullptr, ShaderViews.Handle(texture->ShaderView));
        }
        return texture;
    }

    void Device::UploadTexture(gpu::Texture& target, int x, int y, int width, int height, const std::uint8_t* pixels,
        std::size_t stride)
    {
        auto& texture = static_cast<Texture&>(target);
        if (width <= 0 || height <= 0)
        {
            return;
        }
        UINT bytesPerPixel = texture.TextureFormat == gpu::TextureFormat::R8 ? 1 : 4;
        UINT64 rowPitch = AlignUp(static_cast<UINT64>(width) * bytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

        UploadBatch& batch = CurrentUploads();
        LinearUploader::Allocation memory =
            batch.Memory.Allocate(Device3D.Get(), rowPitch * static_cast<UINT64>(height), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        for (int row = 0; row < height; ++row)
        {
            std::memcpy(memory.Data + rowPitch * static_cast<UINT64>(row), pixels + stride * static_cast<std::size_t>(row),
                static_cast<std::size_t>(width) * bytesPerPixel);
        }

        if (texture.State != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier = Transition(texture.Resource.Get(), texture.State, D3D12_RESOURCE_STATE_COPY_DEST);
            batch.List->ResourceBarrier(1, &barrier);
        }

        D3D12_TEXTURE_COPY_LOCATION source {};
        source.pResource = memory.Resource;
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint.Offset = memory.Offset;
        source.PlacedFootprint.Footprint.Format = ToFormat(texture.TextureFormat);
        source.PlacedFootprint.Footprint.Width = static_cast<UINT>(width);
        source.PlacedFootprint.Footprint.Height = static_cast<UINT>(height);
        source.PlacedFootprint.Footprint.Depth = 1;
        source.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(rowPitch);
        D3D12_TEXTURE_COPY_LOCATION destination {};
        destination.pResource = texture.Resource.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = 0;
        batch.List->CopyTextureRegion(&destination, static_cast<UINT>(x), static_cast<UINT>(y), 0, &source, nullptr);

        // Sampled textures stay readable by shaders between uploads.
        D3D12_RESOURCE_BARRIER back = Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, ShaderResource);
        batch.List->ResourceBarrier(1, &back);
        texture.State = ShaderResource;
    }

    std::unique_ptr<gpu::Buffer> Device::CreateBuffer(std::span<const std::uint8_t> data, bool indices, std::string_view name)
    {
        D3D12_HEAP_PROPERTIES heap {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC description {};
        description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Width = std::max<UINT64>(data.size(), 16);
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> resource;
        if (FAILED(Device3D->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&resource))))
        {
            return nullptr;
        }
        if (!name.empty())
        {
            resource->SetName(ToWide(name).c_str());
        }

        // The copy moves the buffer from the common state to the copy state by
        // itself. Frames sent together with the uploads need it readable already,
        // so it is moved on explicitly; afterwards it returns to common by itself.
        UploadBatch& batch = CurrentUploads();
        LinearUploader::Allocation memory = batch.Memory.Allocate(Device3D.Get(), data.size(), 16);
        std::memcpy(memory.Data, data.data(), data.size());
        batch.List->CopyBufferRegion(resource.Get(), 0, memory.Resource, memory.Offset, data.size());
        D3D12_RESOURCE_BARRIER readable = Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            indices ? D3D12_RESOURCE_STATE_INDEX_BUFFER : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        batch.List->ResourceBarrier(1, &readable);
        return std::make_unique<Buffer>(*this, resource, data.size(), indices);
    }

    Result<std::unique_ptr<gpu::Pipeline>> Device::CreatePipeline(const gpu::PipelineSettings& settings)
    {
        auto compile = [&](std::string_view source, std::string_view entry, const char* target) -> Result<ComPtr<ID3DBlob>> {
            std::string key = std::format("{}|{}|{}", target, entry, source);
            if (auto found = CompiledShaders.find(key); found != CompiledShaders.end())
            {
                return found->second;
            }
            ComPtr<ID3DBlob> code;
            ComPtr<ID3DBlob> errors;
            std::string entryName(entry);
            std::string sourceName = settings.Name.empty() ? std::string("shader") : std::string(settings.Name);
            HRESULT result = D3DCompile(source.data(), source.size(), sourceName.c_str(), nullptr, nullptr,
                entryName.c_str(), target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
            if (FAILED(result))
            {
                std::string message = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()),
                                                   errors->GetBufferSize())
                                             : Describe(result);
                return Failure(message);
            }
            CompiledShaders.emplace(std::move(key), code);
            return code;
        };

        Result<ComPtr<ID3DBlob>> vertex = compile(settings.VertexShader, settings.VertexEntry, "vs_5_1");
        if (!vertex)
        {
            return Failure(std::format("the vertex shader of {} did not compile: {}", settings.Name, vertex.Error()));
        }
        Result<ComPtr<ID3DBlob>> pixel = compile(settings.PixelShader, settings.PixelEntry, "ps_5_1");
        if (!pixel)
        {
            return Failure(std::format("the pixel shader of {} did not compile: {}", settings.Name, pixel.Error()));
        }

        std::vector<std::string> names;
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
        names.reserve(settings.Attributes.size());
        for (const gpu::VertexAttribute& attribute : settings.Attributes)
        {
            names.emplace_back(attribute.Name);
        }
        for (std::size_t index = 0; index < settings.Attributes.size(); ++index)
        {
            const gpu::VertexAttribute& attribute = settings.Attributes[index];
            D3D12_INPUT_ELEMENT_DESC element {};
            element.SemanticName = names[index].c_str();
            switch (attribute.Format)
            {
            case gpu::VertexFormat::Float1: element.Format = DXGI_FORMAT_R32_FLOAT; break;
            case gpu::VertexFormat::Float2: element.Format = DXGI_FORMAT_R32G32_FLOAT; break;
            case gpu::VertexFormat::Float3: element.Format = DXGI_FORMAT_R32G32B32_FLOAT; break;
            case gpu::VertexFormat::Float4: element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
            }
            element.AlignedByteOffset = attribute.Offset;
            element.InputSlotClass =
                settings.PerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            element.InstanceDataStepRate = settings.PerInstance ? 1 : 0;
            elements.push_back(element);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC description {};
        description.pRootSignature = RootSignature.Get();
        description.VS = { (*vertex)->GetBufferPointer(), (*vertex)->GetBufferSize() };
        description.PS = { (*pixel)->GetBufferPointer(), (*pixel)->GetBufferSize() };
        description.InputLayout = { elements.data(), static_cast<UINT>(elements.size()) };
        description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = ToFormat(settings.ColorFormat);
        description.SampleDesc.Count = static_cast<UINT>(std::max(settings.SampleCount, 1));
        description.SampleMask = UINT_MAX;

        // Every field needs a valid value, even for features that are off.
        for (D3D12_RENDER_TARGET_BLEND_DESC& target : description.BlendState.RenderTarget)
        {
            target.SrcBlend = D3D12_BLEND_ONE;
            target.DestBlend = D3D12_BLEND_ZERO;
            target.BlendOp = D3D12_BLEND_OP_ADD;
            target.SrcBlendAlpha = D3D12_BLEND_ONE;
            target.DestBlendAlpha = D3D12_BLEND_ZERO;
            target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            target.LogicOp = D3D12_LOGIC_OP_NOOP;
            target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        D3D12_DEPTH_STENCILOP_DESC keep { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
            D3D12_COMPARISON_FUNC_ALWAYS };
        description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        description.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        description.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        description.DepthStencilState.FrontFace = keep;
        description.DepthStencilState.BackFace = keep;

        D3D12_RENDER_TARGET_BLEND_DESC& blend = description.BlendState.RenderTarget[0];
        if (settings.Blend == gpu::Blending::Premultiplied)
        {
            blend.BlendEnable = TRUE;
            blend.SrcBlend = D3D12_BLEND_ONE;
            blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blend.BlendOp = D3D12_BLEND_OP_ADD;
            blend.SrcBlendAlpha = D3D12_BLEND_ONE;
            blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }

        description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        description.RasterizerState.CullMode = settings.CullBackFaces ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
        // Front faces wind counterclockwise, as in easyforge's model data.
        description.RasterizerState.FrontCounterClockwise = TRUE;
        description.RasterizerState.DepthClipEnable = TRUE;

        if (settings.DepthTest)
        {
            description.DepthStencilState.DepthEnable = TRUE;
            description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        }

        auto pipeline = std::make_unique<Pipeline>(*this);
        HRESULT result = Device3D->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&pipeline->State));
        if (FAILED(result))
        {
            ReportValidationMessages();
            return Failure(std::format("Direct3D 12 refused the pipeline {} ({})", settings.Name, Describe(result)));
        }
        pipeline->Topology = settings.TriangleStrip ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        pipeline->Stride = settings.VertexStride;
        return std::unique_ptr<gpu::Pipeline>(std::move(pipeline));
    }

    Result<std::unique_ptr<gpu::Surface>> Device::CreateSurface(const easyforge::Surface& native, int width, int height,
        bool transparent)
    {
        if (native.Kind != easyforge::Surface::Platform::Windows || !native.Handle)
        {
            return Failure("Direct3D 12 can only draw into a Windows window");
        }
        auto window = static_cast<HWND>(native.Handle);
        auto surface = std::make_unique<Surface>(*this);
        surface->SurfaceWidth = std::max(width, 1);
        surface->SurfaceHeight = std::max(height, 1);
        surface->Flags = TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        DXGI_SWAP_CHAIN_DESC1 description {};
        description.Width = static_cast<UINT>(surface->SurfaceWidth);
        description.Height = static_cast<UINT>(surface->SurfaceHeight);
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = static_cast<UINT>(surface->Buffers.size());
        // Sequential flips keep what was shown, so it can be read back with Capture.
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        description.Flags = surface->Flags;

        ComPtr<IDXGISwapChain1> swapChain;
        HRESULT result = S_OK;
        if (transparent)
        {
            // A see-through window is shown through DirectComposition, which blends
            // the image with what is behind it.
            description.Scaling = DXGI_SCALING_STRETCH;
            description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            result = Factory->CreateSwapChainForComposition(Queue.Get(), &description, nullptr, &swapChain);
            if (SUCCEEDED(result))
            {
                result = DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&surface->Composition));
            }
            if (SUCCEEDED(result))
            {
                surface->Composition->CreateTargetForHwnd(window, TRUE, &surface->CompositionTarget);
                surface->Composition->CreateVisual(&surface->CompositionVisual);
                surface->CompositionVisual->SetContent(swapChain.Get());
                surface->CompositionTarget->SetRoot(surface->CompositionVisual.Get());
                surface->Composition->Commit();
            }
        }
        else
        {
            description.Scaling = DXGI_SCALING_NONE;
            description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            result = Factory->CreateSwapChainForHwnd(Queue.Get(), window, &description, nullptr, nullptr, &swapChain);
        }
        if (FAILED(result) || FAILED(swapChain.As(&surface->SwapChain)))
        {
            return Failure("Direct3D 12 could not make a swap chain for the window (" + Describe(result) + ")");
        }
        // The window library handles full screen itself.
        Factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
        surface->TakeBuffers();
        return std::unique_ptr<gpu::Surface>(std::move(surface));
    }

    std::unique_ptr<gpu::Context> Device::CreateContext()
    {
        return std::make_unique<Context>(*this);
    }

    ImageData Device::ReadTexture(gpu::Texture& target)
    {
        auto& texture = static_cast<Texture&>(target);
        if (texture.SampleCount != 1 || texture.TextureFormat == gpu::TextureFormat::Depth32)
        {
            return {};
        }
        D3D12_RESOURCE_DESC description = texture.Resource->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
        UINT64 total = 0;
        Device3D->GetCopyableFootprints(&description, 0, 1, 0, &footprint, nullptr, nullptr, &total);

        D3D12_HEAP_PROPERTIES heap {};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bufferDescription {};
        bufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDescription.Width = total;
        bufferDescription.Height = 1;
        bufferDescription.DepthOrArraySize = 1;
        bufferDescription.MipLevels = 1;
        bufferDescription.SampleDesc.Count = 1;
        bufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> readback;
        Device3D->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufferDescription, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&readback));

        UploadBatch& batch = CurrentUploads();
        D3D12_RESOURCE_STATES before = texture.State;
        D3D12_RESOURCE_BARRIER toCopy = Transition(texture.Resource.Get(), before, D3D12_RESOURCE_STATE_COPY_SOURCE);
        batch.List->ResourceBarrier(1, &toCopy);
        D3D12_TEXTURE_COPY_LOCATION source {};
        source.pResource = texture.Resource.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION destination {};
        destination.pResource = readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        batch.List->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        D3D12_RESOURCE_BARRIER back = Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, before);
        batch.List->ResourceBarrier(1, &back);
        WaitFor(Submit({}));

        ImageData image(texture.TextureWidth, texture.TextureHeight);
        std::uint8_t* mapped = nullptr;
        D3D12_RANGE range { 0, static_cast<SIZE_T>(total) };
        readback->Map(0, &range, reinterpret_cast<void**>(&mapped));
        bool single = texture.TextureFormat == gpu::TextureFormat::R8;
        for (int y = 0; y < texture.TextureHeight; ++y)
        {
            const std::uint8_t* row = mapped + footprint.Offset + static_cast<UINT64>(footprint.Footprint.RowPitch) * static_cast<UINT64>(y);
            std::uint8_t* output = image.Pixels.data() + static_cast<std::size_t>(y) * image.Stride();
            for (int x = 0; x < texture.TextureWidth; ++x)
            {
                std::uint8_t* pixel = output + x * 4;
                if (single)
                {
                    pixel[0] = pixel[1] = pixel[2] = row[x];
                    pixel[3] = 255;
                    continue;
                }
                // Targets hold colors multiplied by alpha; images keep them apart.
                const std::uint8_t* stored = row + x * 4;
                std::uint8_t alpha = stored[3];
                for (int channel = 0; channel < 3; ++channel)
                {
                    pixel[channel] = alpha == 255 || alpha == 0
                                         ? stored[channel]
                                         : static_cast<std::uint8_t>(std::min(255, (stored[channel] * 255 + alpha / 2) / alpha));
                }
                pixel[3] = alpha;
            }
        }
        D3D12_RANGE nothing { 0, 0 };
        readback->Unmap(0, &nothing);
        return image;
    }
}

namespace easyforge::internal::gpu
{
    Result<std::shared_ptr<Device>> SharedDevice(GraphicsAdapter adapter)
    {
        static std::map<GraphicsAdapter, std::weak_ptr<Device>> devices;
        if (std::shared_ptr<Device> existing = devices[adapter].lock())
        {
            return existing;
        }
        Result<std::shared_ptr<direct3d12::Device>> created = direct3d12::Device::Create(adapter);
        if (!created)
        {
            return Failure(created.Error());
        }
        std::shared_ptr<Device> device = *created;
        devices[adapter] = device;
        return device;
    }
}
