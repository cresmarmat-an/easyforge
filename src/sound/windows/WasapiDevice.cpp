// The default output device through WASAPI, in shared mode, woken by the
// device whenever it wants more sound.

#include "../OutputDevice.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <future>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <wrl/client.h>

#if defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

namespace easyforge::internal
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        // PKEY_Device_FriendlyName, written out so that no GUIDs have to be
        // defined in this file.
        constexpr PROPERTYKEY FriendlyNameKey = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

        // The subformat GUIDs of WAVEFORMATEXTENSIBLE are a format tag followed
        // by the same twelve bytes.
        bool IsFloatFormat(const WAVEFORMATEX& format)
        {
            if (format.wBitsPerSample != 32)
            {
                return false;
            }
            if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
            {
                return true;
            }
            if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE || format.cbSize < 22)
            {
                return false;
            }
            const GUID& subformat = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format).SubFormat;
            constexpr unsigned char rest[8] = { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };
            return subformat.Data1 == WAVE_FORMAT_IEEE_FLOAT && subformat.Data2 == 0x0000 && subformat.Data3 == 0x0010 &&
                   std::equal(std::begin(rest), std::end(rest), subformat.Data4);
        }

        std::string Narrow(const wchar_t* text)
        {
            if (!text)
            {
                return {};
            }
            int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (size <= 1)
            {
                return {};
            }
            std::string narrow(static_cast<std::size_t>(size - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, narrow.data(), size, nullptr, nullptr);
            return narrow;
        }

        std::string FriendlyName(IMMDevice& device)
        {
            ComPtr<IPropertyStore> properties;
            if (FAILED(device.OpenPropertyStore(STGM_READ, &properties)))
            {
                return {};
            }
            PROPVARIANT value;
            PropVariantInit(&value);
            std::string name;
            if (SUCCEEDED(properties->GetValue(FriendlyNameKey, &value)) && value.vt == VT_LPWSTR)
            {
                name = Narrow(value.pwszVal);
            }
            PropVariantClear(&value);
            return name;
        }

        // A device open and running.
        struct Session
        {
            ComPtr<IAudioClient> Audio;
            ComPtr<IAudioRenderClient> Render;
            HANDLE Ready = nullptr;
            UINT32 BufferFrames = 0;
            int SampleRate = 0;
            int ChannelCount = 0;

            ~Session() { Close(); }

            void Close()
            {
                if (Audio)
                {
                    Audio->Stop();
                }
                Render.Reset();
                Audio.Reset();
                if (Ready)
                {
                    CloseHandle(Ready);
                    Ready = nullptr;
                }
            }
        };

        class WasapiDevice;

        // Tells the device thread when the system's default output changes.
        class DefaultWatcher final : public IMMNotificationClient
        {
        public:
            explicit DefaultWatcher(WasapiDevice& owner) : Owner(owner) {}

            ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
            ULONG STDMETHODCALLTYPE Release() override { return 1; }

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID identifier, void** object) override
            {
                if (identifier == __uuidof(IUnknown) || identifier == __uuidof(IMMNotificationClient))
                {
                    *object = static_cast<IMMNotificationClient*>(this);
                    return S_OK;
                }
                *object = nullptr;
                return E_NOINTERFACE;
            }

            HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override;
            HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

        private:
            WasapiDevice& Owner;
        };

        class WasapiDevice final : public OutputDevice
        {
        public:
            WasapiDevice(DeviceClient& client, const DeviceSettings& settings)
                : Client(client), Settings(settings), Wake(CreateEventW(nullptr, FALSE, FALSE, nullptr)), Watcher(*this)
            {
            }

            ~WasapiDevice() override
            {
                Stopping.store(true);
                SetEvent(Wake);
                if (Thread.joinable())
                {
                    Thread.join();
                }
                CloseHandle(Wake);
            }

            void Run(std::promise<std::string>& opened);

            void DefaultChanged()
            {
                ChangedDefault.store(true);
                SetEvent(Wake);
            }

            std::thread Thread;

        private:
            std::string Open(IMMDeviceEnumerator& enumerator, Session& session);
            void Feed(Session& session);
            void RenderSilently(int sampleRate, int channelCount);

            DeviceClient& Client;
            DeviceSettings Settings;
            HANDLE Wake;
            DefaultWatcher Watcher;
            std::atomic<bool> Stopping { false };
            std::atomic<bool> ChangedDefault { false };
            std::vector<float> Scratch;
        };

        HRESULT STDMETHODCALLTYPE DefaultWatcher::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR)
        {
            if (flow == eRender && role == eConsole)
            {
                Owner.DefaultChanged();
            }
            return S_OK;
        }

        std::string WasapiDevice::Open(IMMDeviceEnumerator& enumerator, Session& session)
        {
            session.Close();
            ComPtr<IMMDevice> device;
            if (FAILED(enumerator.GetDefaultAudioEndpoint(eRender, eConsole, &device)))
            {
                return "there is no sound output device";
            }
            std::string name = FriendlyName(*device.Get());
            HRESULT result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(session.Audio.GetAddressOf()));
            if (FAILED(result))
            {
                return std::format("the sound device {} could not be opened (0x{:08X})", name, static_cast<unsigned>(result));
            }
            WAVEFORMATEX* mix = nullptr;
            if (FAILED(session.Audio->GetMixFormat(&mix)))
            {
                session.Close();
                return std::format("the sound device {} did not say what it plays", name);
            }

            // The mixer works in 32-bit float. When the device mixes in something
            // else, or a rate of its own was asked for, Windows converts.
            int rate = Settings.SampleRate > 0 ? Settings.SampleRate : static_cast<int>(mix->nSamplesPerSec);
            int channels = mix->nChannels;
            WAVEFORMATEXTENSIBLE wanted {};
            DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
            const WAVEFORMATEX* format = mix;
            if (!IsFloatFormat(*mix) || rate != static_cast<int>(mix->nSamplesPerSec))
            {
                wanted.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
                wanted.Format.nChannels = static_cast<WORD>(channels);
                wanted.Format.nSamplesPerSec = static_cast<DWORD>(rate);
                wanted.Format.wBitsPerSample = 32;
                wanted.Format.nBlockAlign = static_cast<WORD>(channels * 4);
                wanted.Format.nAvgBytesPerSec = wanted.Format.nSamplesPerSec * wanted.Format.nBlockAlign;
                wanted.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
                wanted.Samples.wValidBitsPerSample = 32;
                wanted.dwChannelMask = mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE ? reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix)->dwChannelMask : 0;
                wanted.SubFormat = { WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
                format = &wanted.Format;
                flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            }
            REFERENCE_TIME duration = static_cast<REFERENCE_TIME>(static_cast<double>(Settings.Latency) * 10'000'000.0);
            result = session.Audio->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, format, nullptr);
            CoTaskMemFree(mix);
            if (FAILED(result))
            {
                session.Close();
                return std::format("the sound device {} could not be started (0x{:08X})", name, static_cast<unsigned>(result));
            }
            session.Ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!session.Ready || FAILED(session.Audio->SetEventHandle(session.Ready)) ||
                FAILED(session.Audio->GetBufferSize(&session.BufferFrames)) ||
                FAILED(session.Audio->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(session.Render.GetAddressOf()))))
            {
                session.Close();
                return std::format("the sound device {} could not be started", name);
            }
            session.SampleRate = rate;
            session.ChannelCount = channels;
            Client.DeviceFormat(rate, channels, name);

            // Starts with the device's buffer full of the first sound, not silence.
            Feed(session);
            if (FAILED(session.Audio->Start()))
            {
                session.Close();
                return std::format("the sound device {} could not be started", name);
            }
            return {};
        }

        void WasapiDevice::Feed(Session& session)
        {
            UINT32 padding = 0;
            if (FAILED(session.Audio->GetCurrentPadding(&padding)))
            {
                session.Close();
                return;
            }
            UINT32 frames = session.BufferFrames - padding;
            if (frames == 0)
            {
                return;
            }
            BYTE* data = nullptr;
            if (FAILED(session.Render->GetBuffer(frames, &data)))
            {
                session.Close();
                return;
            }
            Client.DeviceRender(reinterpret_cast<float*>(data), static_cast<int>(frames));
            session.Render->ReleaseBuffer(frames, 0);
        }

        void WasapiDevice::RenderSilently(int sampleRate, int channelCount)
        {
            // Without a device, sounds still move along in time, unheard.
            int frames = sampleRate / 100;
            Scratch.resize(static_cast<std::size_t>(frames) * static_cast<std::size_t>(channelCount));
            Client.DeviceRender(Scratch.data(), frames);
        }

        void WasapiDevice::Run(std::promise<std::string>& opened)
        {
            HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            DWORD taskIndex = 0;
            HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
#if defined(_M_X64) || defined(_M_IX86)
            // Numbers too small to matter slow filters down a great deal; flush them to zero.
            _mm_setcsr(_mm_getcsr() | 0x8040);
#endif

            ComPtr<IMMDeviceEnumerator> enumerator;
            Session session;
            std::string problem;
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(enumerator.GetAddressOf()))))
            {
                problem = "the system's sound devices cannot be listed";
            }
            else
            {
                problem = Open(*enumerator.Get(), session);
            }
            bool watching = enumerator && SUCCEEDED(enumerator->RegisterEndpointNotificationCallback(&Watcher));
            bool failed = !problem.empty();
            opened.set_value(problem);

            int rate = session.SampleRate;
            int channels = session.ChannelCount;
            ULONGLONG lastTry = GetTickCount64();
            while (!failed && !Stopping.load())
            {
                if (ChangedDefault.exchange(false))
                {
                    Open(*enumerator.Get(), session);
                    lastTry = GetTickCount64();
                }
                if (!session.Audio)
                {
                    // No device: keep time, and look for one every second.
                    RenderSilently(rate, channels);
                    WaitForSingleObject(Wake, 10);
                    if (GetTickCount64() - lastTry >= 1000)
                    {
                        Open(*enumerator.Get(), session);
                        lastTry = GetTickCount64();
                    }
                    if (session.Audio)
                    {
                        rate = session.SampleRate;
                        channels = session.ChannelCount;
                    }
                    continue;
                }
                rate = session.SampleRate;
                channels = session.ChannelCount;
                HANDLE handles[2] = { session.Ready, Wake };
                WaitForMultipleObjects(2, handles, FALSE, 200);
                if (Stopping.load() || ChangedDefault.load())
                {
                    continue;
                }
                // A device that was unplugged makes these calls fail, which closes
                // the session; the next pass opens the new default.
                Feed(session);
                if (!session.Audio)
                {
                    Open(*enumerator.Get(), session);
                    lastTry = GetTickCount64();
                }
            }

            session.Close();
            if (watching)
            {
                enumerator->UnregisterEndpointNotificationCallback(&Watcher);
            }
            enumerator.Reset();
            if (task)
            {
                AvRevertMmThreadCharacteristics(task);
            }
            if (SUCCEEDED(initialized))
            {
                CoUninitialize();
            }
        }
    }

    std::unique_ptr<OutputDevice> StartOutputDevice(DeviceClient& client, const DeviceSettings& settings, std::string& error)
    {
        auto device = std::make_unique<WasapiDevice>(client, settings);
        std::promise<std::string> opened;
        std::future<std::string> result = opened.get_future();
        WasapiDevice* running = device.get();
        // The thread owns the promise, which can still be in set_value when get
        // returns here.
        device->Thread = std::thread([running, promise = std::move(opened)]() mutable { running->Run(promise); });
        std::string problem = result.get();
        if (!problem.empty())
        {
            error = problem;
            return nullptr;
        }
        return device;
    }
}
