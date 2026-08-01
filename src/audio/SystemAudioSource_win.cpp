#include "audio/SystemAudioSource.h"

#ifdef Q_OS_WIN

#include <QtGlobal>
#include <QVector>

#include <atomic>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>   // IMMDeviceEnumerator, CLSID_MMDeviceEnumerator, eRender/eConsole
#include <audioclient.h>   // IAudioClient, AUDCLNT_STREAMFLAGS_LOOPBACK, IAudioCaptureClient

// WASAPI COM GUID 手工定义:部分 SDK / Qt MSVC 套件的 uuid.lib 不导出这几个音频 GUID
// (链接报 LNK2019;INITGUID 在此文件不管用,因为顶部 Qt 头已先含过 windows.h,DEFINE_GUID
// 早已是 extern 声明形)。头里已声明 EXTERN_C,这里补定义即可,不再依赖 uuid.lib。
extern "C" const GUID CLSID_MMDeviceEnumerator =
    {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
extern "C" const GUID IID_IAudioClient =
    {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
extern "C" const GUID IID_IAudioCaptureClient =
    {0xC8ADBD64, 0xE71E, 0x48A0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xE3, 0x17}};

// Windows WASAPI loopback:抓「默认输出设备」的混音(任何 app 的声音都经过这里),
// 下混 mono float,按 ~2048 样本切片发 pcmFrame。复用 FeatureExtractor::compute(PCM 路径,
// 低频/BPM 质量比网页 bins 路径好)。线程独立 + atomic stop flag,避开 QThread 亲和性坑。

struct SystemAudioSource::Impl {
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* wfx = nullptr;
    std::thread th;
    std::atomic<bool> run{false};
    int sr = 48000;
    int channels = 2;
    bool isFloat = false;     // mix format 通常是 IEEE float32/48k
    int frameBytes = 8;       // nBlockAlign
};

SystemAudioSource::SystemAudioSource(QObject* parent)
    : QObject(parent), impl_(new Impl) {}

SystemAudioSource::~SystemAudioSource() { stop(); delete impl_; }

bool SystemAudioSource::start() {
    if (active_ || !impl_) return false;
    // 主线程 COM:Qt 可能已把主线程设成 STA,再 init MTA 会 RPC_E_CHANGED_MODE —— 忽略,
    // 继续用现有 apartment(CoCreateInstance 跨 apt 仍可用)。
    HRESULT ci = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Q_UNUSED(ci);

    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&en))) || !en)
        return false;
    IMMDevice* dev = nullptr;
    HRESULT hr = en->GetDefaultAudioEndpoint(eRender, eConsole, &dev);
    en->Release();
    if (FAILED(hr) || !dev) return false;
    hr = dev->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                       reinterpret_cast<void**>(&impl_->client));
    dev->Release();
    if (FAILED(hr) || !impl_->client) return false;

    hr = impl_->client->GetMixFormat(&impl_->wfx);
    if (FAILED(hr) || !impl_->wfx) { stop(); return false; }
    impl_->sr = static_cast<int>(impl_->wfx->nSamplesPerSec);
    impl_->channels = static_cast<int>(impl_->wfx->nChannels);
    impl_->frameBytes = impl_->wfx->nBlockAlign;
    // mix format 几乎总是 float32(EXTENSIBLE + 32bit IEEE);少数 int16。按位深判够用。
    impl_->isFloat = (impl_->wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                     (impl_->wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                      impl_->wfx->wBitsPerSample == 32);

    const REFERENCE_TIME dur = 10 * 1000 * 1000;   // 1s 缓冲(reftimes = 100ns)
    hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                   dur, 0, impl_->wfx, nullptr);
    if (FAILED(hr)) { stop(); return false; }
    hr = impl_->client->GetService(IID_IAudioCaptureClient,
                                   reinterpret_cast<void**>(&impl_->capture));
    if (FAILED(hr) || !impl_->capture) { stop(); return false; }
    hr = impl_->client->Start();
    if (FAILED(hr)) { stop(); return false; }

    impl_->run = true;
    active_ = true;
    emit activeChanged(true);
    impl_->th = std::thread([this] { loop(); });
    return true;
}

void SystemAudioSource::stop() {
    if (!impl_) return;
    impl_->run = false;
    if (impl_->client) impl_->client->Stop();   // 先停 client,释放可能阻塞在 GetBuffer 的捕获线程(防 stop/dtor 挂起 UI)
    if (impl_->th.joinable()) impl_->th.join();
    if (impl_->capture) { impl_->capture->Release(); impl_->capture = nullptr; }
    if (impl_->client)  { impl_->client->Release(); impl_->client = nullptr; }
    if (impl_->wfx)     { CoTaskMemFree(impl_->wfx); impl_->wfx = nullptr; }
    if (active_) { active_ = false; emit activeChanged(false); }
}

void SystemAudioSource::loop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);   // 捕获线程走 MTA
    constexpr int winSize = 2048;                     // 对齐 FeatureExtractor::kN
    QVector<float> buf;
    buf.reserve(winSize * 2);
    while (impl_->run) {
        UINT32 count = 0;
        DWORD flags = 0;        // GetBuffer 第三参是 DWORD*(MSVC 下 DWORD≠UINT32,别用后者)
        LPBYTE data = nullptr;
        const HRESULT hr = impl_->capture->GetBuffer(&data, &count, &flags, nullptr, nullptr);
        if (hr == AUDCLNT_S_BUFFER_EMPTY) { Sleep(5); continue; }
        if (FAILED(hr)) { Sleep(10); continue; }
        if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && count > 0) {
            const int ch = impl_->channels;
            const bool fl = impl_->isFloat;
            for (UINT32 i = 0; i < count; ++i) {
                const BYTE* frame = data + (i * impl_->frameBytes);
                float s = 0.0f;
                if (fl) {
                    const auto* p = reinterpret_cast<const float*>(frame);
                    for (int c = 0; c < ch; ++c) s += p[c];
                } else {
                    const auto* p = reinterpret_cast<const short*>(frame);
                    for (int c = 0; c < ch; ++c) s += p[c] / 32768.0f;
                }
                buf.append(s / static_cast<float>(ch));   // 多声道均值 → mono
            }
        }
        impl_->capture->ReleaseBuffer(count);
        while (buf.size() >= winSize) {
            emit pcmFrame(buf.mid(0, winSize), impl_->sr);   // mid 拷贝 → 队列跨线程安全
            buf.remove(0, winSize);
        }
    }
    CoUninitialize();
}

#endif  // Q_OS_WIN
