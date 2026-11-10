#include "capture/wasapi_capture.h"
#include "dsp/pcm.h"
#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace daa {
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT hr, const char* operation) {
    if (FAILED(hr)) {
        std::ostringstream error;
        error << operation << " (0x" << std::hex << static_cast<unsigned long>(hr) << ')';
        throw std::runtime_error(error.str());
    }
}
class Handle {
public:
    explicit Handle(HANDLE value) : value_(value) { if (!value) throw std::runtime_error("Cannot create capture event"); }
    ~Handle() { CloseHandle(value_); }
    operator HANDLE() const { return value_; }
private: HANDLE value_;
};
class EndpointNotifications final : public IMMNotificationClient {
public:
    explicit EndpointNotifications(HANDLE restart) : restart_(restart) {}
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto count = --references_; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
        if (id == __uuidof(IUnknown) || id == __uuidof(IMMNotificationClient)) { *object = this; AddRef(); return S_OK; }
        *object = nullptr; return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && role == eMultimedia) SetEvent(restart_);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { SetEvent(restart_); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { SetEvent(restart_); return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
private:
    std::atomic<ULONG> references_{1};
    HANDLE restart_;
};
struct FormatDeleter { void operator()(WAVEFORMATEX* value) const { CoTaskMemFree(value); } };
}

WasapiCapture::WasapiCapture(AudioQueue& queue) : queue_(queue) {
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    restartEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!stopEvent_ || !restartEvent_) {
        if (stopEvent_) CloseHandle(stopEvent_);
        if (restartEvent_) CloseHandle(restartEvent_);
        throw std::runtime_error("Cannot create capture control events");
    }
}
WasapiCapture::~WasapiCapture() { stop(); CloseHandle(stopEvent_); CloseHandle(restartEvent_); }
void WasapiCapture::start() {
    if (thread_.joinable()) return;
    stopping_ = false; ResetEvent(stopEvent_);
    thread_ = std::thread([this] { run(); });
}
void WasapiCapture::stop() { stopping_ = true; SetEvent(stopEvent_); if (thread_.joinable()) thread_.join(); }
CaptureStatus WasapiCapture::status() const { std::lock_guard lock(statusMutex_); return status_; }
void WasapiCapture::setStatus(CaptureStatus status) { std::lock_guard lock(statusMutex_); status_ = std::move(status); }

void WasapiCapture::run() {
    const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { setStatus({"Error", {}, "Cannot initialize Windows audio", 0}); return; }
    DWORD taskIndex = 0;
    const auto priority = AvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);
    while (!stopping_) {
        try { stream(); }
        catch (const std::exception& error) { setStatus({"Reconnecting", {}, error.what(), 0}); }
        if (!stopping_) {
            const HANDLE events[]{stopEvent_, restartEvent_};
            WaitForMultipleObjects(2, events, FALSE, 1000);
        }
    }
    if (priority) AvRevertMmThreadCharacteristics(priority);
    CoUninitialize();
}

void WasapiCapture::stream() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)), "Open audio devices");
    ComPtr<IMMDevice> device;
    check(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device), "Find default playback output");
    std::wstring deviceName = L"Default Windows output";
    ComPtr<IPropertyStore> properties;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
        PROPVARIANT name; PropVariantInit(&name);
        if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR) deviceName = name.pwszVal;
        PropVariantClear(&name);
    }
    ComPtr<IAudioClient> client;
    check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.GetAddressOf())), "Open playback stream");
    WAVEFORMATEX* rawFormat = nullptr;
    check(client->GetMixFormat(&rawFormat), "Read output format");
    const std::unique_ptr<WAVEFORMATEX, FormatDeleter> format(rawFormat);
    bool floating = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool pcm = format->wFormatTag == WAVE_FORMAT_PCM;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format.get());
        constexpr GUID floatSubtype{STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT};
        constexpr GUID pcmSubtype{STATIC_KSDATAFORMAT_SUBTYPE_PCM};
        floating = IsEqualGUID(extended->SubFormat, floatSubtype);
        pcm = IsEqualGUID(extended->SubFormat, pcmSubtype);
    }
    if ((!pcm && !floating) || (floating && format->wBitsPerSample != 32) ||
        (!floating && format->wBitsPerSample != 8 && format->wBitsPerSample != 16 && format->wBitsPerSample != 24 && format->wBitsPerSample != 32))
        throw std::runtime_error("Unsupported playback sample format");
    if (format->nChannels < 1 || format->nChannels > 2)
        throw std::runtime_error("The prototype requires a mono/stereo playback endpoint");
    check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
          AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
          1000000, 0, format.get(), nullptr), "Initialize loopback");
    Handle ready(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    check(client->SetEventHandle(ready), "Set loopback event");
    ComPtr<IAudioCaptureClient> capture;
    check(client->GetService(IID_PPV_ARGS(&capture)), "Open capture service");
    ComPtr<EndpointNotifications> notifications;
    notifications.Attach(new EndpointNotifications(restartEvent_));
    check(enumerator->RegisterEndpointNotificationCallback(notifications.Get()), "Watch output changes");
    struct Registration {
        IMMDeviceEnumerator* enumerator; IMMNotificationClient* callback;
        ~Registration() { enumerator->UnregisterEndpointNotificationCallback(callback); }
    } registration{enumerator.Get(), notifications.Get()};
    check(client->Start(), "Start loopback capture");
    struct StopStream { IAudioClient* client; ~StopStream() { client->Stop(); } } stopStream{client.Get()};
    ++generation_;
    bool pendingGap = true, active = false;
    auto lastPacket = GetTickCount64();
    setStatus({"Waiting for playback", deviceName, {}, format->nSamplesPerSec});
    const HANDLE events[]{stopEvent_, restartEvent_, ready};
    while (!stopping_) {
        const auto wait = WaitForMultipleObjects(3, events, FALSE, 100);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_OBJECT_0 + 1) return;
        if (wait == WAIT_FAILED) throw std::runtime_error("Capture wait failed");
        UINT32 available = 0;
        check(capture->GetNextPacketSize(&available), "Poll loopback packet");
        while (available) {
            BYTE* data = nullptr;
            UINT32 count = 0; DWORD flags = 0;
            UINT64 devicePosition = 0, timestamp = 0;
            check(capture->GetBuffer(&data, &count, &flags, &devicePosition, &timestamp), "Read loopback packet");
            if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) pendingGap = true;
            const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            for (UINT32 offset = 0; offset < count; offset += blockFrames) {
                AudioBlock block;
                block.frames = std::min<UINT32>(blockFrames, count - offset);
                block.sampleRate = format->nSamplesPerSec; block.generation = generation_;
                block.discontinuity = pendingGap; block.samplePosition = devicePosition + offset;
                block.timestamp100ns = timestamp + UINT64(offset) * 10000000 / format->nSamplesPerSec;
                block.timestampValid = !(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR);
                if (!silent) {
                    for (UINT32 i = 0; i < block.frames; ++i) {
                        const auto* sample = data + (offset + i) * format->nBlockAlign;
                        block.samples[2 * i] = decodePcm(sample, format->wBitsPerSample, floating);
                        block.samples[2 * i + 1] = format->nChannels == 1 ? block.samples[2 * i] :
                            decodePcm(sample + format->wBitsPerSample / 8, format->wBitsPerSample, floating);
                    }
                }
                if (queue_.push(block)) pendingGap = false;
                else { ++overruns_; pendingGap = true; }
            }
            frames_ += count;
            check(capture->ReleaseBuffer(count), "Release loopback packet");
            lastPacket = GetTickCount64();
            if (!active) { setStatus({"Capturing", deviceName, {}, format->nSamplesPerSec}); active = true; }
            check(capture->GetNextPacketSize(&available), "Read next loopback packet");
        }
        if (active && GetTickCount64() - lastPacket > 500) {
            setStatus({"No audio packets - waiting", deviceName, {}, format->nSamplesPerSec});
            // A no-packet interval is unknown, not fabricated silence.
            active = false; pendingGap = true;
        }
    }
}
} // namespace daa
