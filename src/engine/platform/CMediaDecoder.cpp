/** @file CMediaDecoder.cpp
 * @brief Media Foundation handles compression; the game owns timing and output.
 */
#define NOMINMAX
#include "engine/platform/CMediaDecoder.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

using Microsoft::WRL::ComPtr;

namespace {
bool MediaResult(HRESULT result, const char *operation) {
    if (SUCCEEDED(result)) { return true; }
    std::printf("[media] %s failed: 0x%08lX\n", operation, static_cast<unsigned long>(result));
    return false;
}

/** Balance COM/MF ownership even when the caller already initialised COM. */
class MediaRuntime {
public:
    MediaRuntime() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_comOwned = SUCCEEDED(result);
        if (FAILED(result) && result != RPC_E_CHANGED_MODE) { MediaResult(result, "COM startup"); return; }
        m_ready = MediaResult(MFStartup(MF_VERSION), "MF startup");
    }
    ~MediaRuntime() {
        if (m_ready) { MFShutdown(); }
        if (m_comOwned) { CoUninitialize(); }
    }
    bool Ready() const { return m_ready; }
private:
    bool m_comOwned = false;
    bool m_ready = false;
};

bool OpenReader(const std::filesystem::path &path, bool video, ComPtr<IMFSourceReader> &reader) {
    ComPtr<IMFAttributes> attributes;
    if (!MediaResult(MFCreateAttributes(&attributes, 2), "reader attributes")) { return false; }
    if (video) { attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE); }
    if (!MediaResult(MFCreateSourceReaderFromURL(path.c_str(), attributes.Get(), &reader), "open file")) {
        std::printf("[media] file=%s\n", path.string().c_str());
        return false;
    }
    DWORD stream = MF_SOURCE_READER_FIRST_AUDIO_STREAM;
    if (video) { stream = MF_SOURCE_READER_FIRST_VIDEO_STREAM; }
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (!MediaResult(reader->SetStreamSelection(stream, TRUE), "select stream")) { return false; }
    ComPtr<IMFMediaType> type;
    if (!MediaResult(MFCreateMediaType(&type), "output type")) { return false; }
    if (video) {
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    } else {
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    }
    return MediaResult(reader->SetCurrentMediaType(stream, nullptr, type.Get()), "configure decoder");
}
}

bool DecodeMediaAudio(const std::filesystem::path &path, MediaAudio &audio) {
    MediaRuntime runtime;
    if (!runtime.Ready()) { return false; }
    ComPtr<IMFSourceReader> reader;
    if (!OpenReader(path, false, reader)) { return false; }
    ComPtr<IMFMediaType> type;
    if (!MediaResult(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type), "audio format")) { return false; }
    MediaAudio decoded;
    decoded.sampleRate = MFGetAttributeUINT32(type.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
    decoded.channels = MFGetAttributeUINT32(type.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0);
    if (decoded.sampleRate == 0 || decoded.channels == 0 ||
        MFGetAttributeUINT32(type.Get(), MF_MT_AUDIO_BITS_PER_SAMPLE, 0) != 16) { return false; }
    while (true) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        if (!MediaResult(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample), "audio sample")) { return false; }
        if (sample != nullptr) {
            ComPtr<IMFMediaBuffer> buffer;
            if (!MediaResult(sample->ConvertToContiguousBuffer(&buffer), "audio buffer")) { return false; }
            BYTE *bytes = nullptr;
            DWORD size = 0;
            if (!MediaResult(buffer->Lock(&bytes, nullptr, &size), "audio lock")) { return false; }
            decoded.samples.insert(decoded.samples.end(), bytes, bytes + size);
            buffer->Unlock();
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) { break; }
    }
    if (decoded.samples.empty()) { return false; }
    std::printf("[media] audio=%s rate=%u channels=%u bytes=%zu\n", path.filename().string().c_str(),
        decoded.sampleRate, decoded.channels, decoded.samples.size());
    audio = std::move(decoded);
    return true;
}

struct CMediaVideo::Impl {
    MediaRuntime runtime;
    ComPtr<IMFSourceReader> reader;
    unsigned width = 0;
    unsigned height = 0;
    LONG stride = 0;
    std::uint64_t durationMs = 0;
};

CMediaVideo::CMediaVideo() : m_impl(new Impl()) {}
CMediaVideo::~CMediaVideo() = default;
unsigned CMediaVideo::Width() const { return m_impl->width; }
unsigned CMediaVideo::Height() const { return m_impl->height; }
std::uint64_t CMediaVideo::DurationMs() const { return m_impl->durationMs; }

bool CMediaVideo::Open(const std::filesystem::path &path) {
    if (!m_impl->runtime.Ready()) { return false; }
    m_impl->reader.Reset();
    if (!OpenReader(path, true, m_impl->reader)) { return false; }
    ComPtr<IMFMediaType> type;
    if (!MediaResult(m_impl->reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type), "video format") ||
        !MediaResult(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &m_impl->width, &m_impl->height), "video dimensions")) { return false; }
    m_impl->stride = static_cast<LONG>(MFGetAttributeUINT32(type.Get(), MF_MT_DEFAULT_STRIDE, m_impl->width * 4));
    PROPVARIANT duration;
    PropVariantInit(&duration);
    if (SUCCEEDED(m_impl->reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration)) && duration.vt == VT_UI8) {
        m_impl->durationMs = duration.uhVal.QuadPart / 10000;
    }
    PropVariantClear(&duration);
    std::printf("[media] video=%s width=%u height=%u duration=%llu stride=%ld\n",
        path.filename().string().c_str(), Width(), Height(), DurationMs(), m_impl->stride);
    return Width() != 0 && Height() != 0;
}

bool CMediaVideo::ReadFrame(PNGImage &image, std::uint64_t &timestampMs, bool &ended) {
    ended = false;
    ComPtr<IMFSample> sample;
    LONGLONG timestamp = 0;
    while (sample == nullptr) {
        DWORD flags = 0;
        if (!MediaResult(m_impl->reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &timestamp, &sample), "video sample")) { return false; }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) { ended = true; return true; }
    }
    ComPtr<IMFMediaBuffer> buffer;
    if (!MediaResult(sample->ConvertToContiguousBuffer(&buffer), "video buffer")) { return false; }
    BYTE *bytes = nullptr;
    DWORD size = 0;
    if (!MediaResult(buffer->Lock(&bytes, nullptr, &size), "video lock")) { return false; }
    const unsigned rowBytes = static_cast<unsigned>(std::abs(m_impl->stride));
    if (size < rowBytes * Height() || rowBytes < Width() * 4) { buffer->Unlock(); return false; }
    image.width = Width();
    image.height = Height();
    image.pixels.resize(static_cast<std::size_t>(Width()) * Height() * 4);
    for (unsigned y = 0; y < Height(); ++y) {
        unsigned sourceY = y;
        if (m_impl->stride < 0) { sourceY = Height() - y - 1; }
        const BYTE *source = bytes + sourceY * rowBytes;
        std::uint8_t *destination = image.pixels.data() + y * Width() * 4;
        for (unsigned x = 0; x < Width(); ++x) {
            destination[x * 4] = source[x * 4 + 2];
            destination[x * 4 + 1] = source[x * 4 + 1];
            destination[x * 4 + 2] = source[x * 4];
            destination[x * 4 + 3] = 255;
        }
    }
    buffer->Unlock();
    timestampMs = static_cast<std::uint64_t>(std::max<LONGLONG>(0, timestamp / 10000));
    return true;
}
