#include "SoundEngine.h"
#include <QtMath>
#include <QThread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef HAS_QT_MULTIMEDIA
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
#endif

SoundEngine& SoundEngine::instance()
{
    static SoundEngine s;
    return s;
}

SoundEngine::SoundEngine(QObject* parent)
    : QObject(parent)
{
#ifdef HAS_QT_MULTIMEDIA
    m_format.setSampleRate(SAMPLE_RATE);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
    for (int i = 0; i < MAX_SLOTS; ++i) {
        m_slots[i].buffer = nullptr;
        m_slots[i].sink = nullptr;
    }
#endif
}

QByteArray SoundEngine::generateTone(double freqHz, int durationMs, double amplitude)
{
    int numSamples = static_cast<int>(SAMPLE_RATE * durationMs / 1000.0);
    QByteArray pcm;
    pcm.resize(numSamples * 2); // 16-bit = 2 bytes
    auto* data = reinterpret_cast<qint16*>(pcm.data());

    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / SAMPLE_RATE;
        double env = 1.0 - static_cast<double>(i) / numSamples;
        double sample = amplitude * env * std::sin(2.0 * M_PI * freqHz * t);
        data[i] = static_cast<qint16>(sample * 32767.0 * m_volume);
    }
    return pcm;
}

QByteArray SoundEngine::generateSweep(double startHz, double endHz, int durationMs, double amplitude)
{
    int numSamples = static_cast<int>(SAMPLE_RATE * durationMs / 1000.0);
    QByteArray pcm;
    pcm.resize(numSamples * 2);
    auto* data = reinterpret_cast<qint16*>(pcm.data());

    double phase = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / numSamples;
        double freq = startHz + (endHz - startHz) * t;
        double env = 1.0 - t * 0.5;
        phase += 2.0 * M_PI * freq / SAMPLE_RATE;
        double sample = amplitude * env * std::sin(phase);
        data[i] = static_cast<qint16>(sample * 32767.0 * m_volume);
    }
    return pcm;
}

void SoundEngine::playPCM(const QByteArray& pcm)
{
    if (!m_enabled || pcm.isEmpty()) return;

#ifdef HAS_QT_MULTIMEDIA
    // Qt Multimedia path
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) return;

    auto& slot = m_slots[m_nextSlot];
    m_nextSlot = (m_nextSlot + 1) % MAX_SLOTS;

    if (slot.sink) {
        slot.sink->stop();
        slot.sink->deleteLater();
        slot.sink = nullptr;
    }
    if (slot.buffer) {
        slot.buffer->close();
        slot.buffer->deleteLater();
        slot.buffer = nullptr;
    }

    slot.buffer = new QBuffer(this);
    slot.buffer->setData(pcm);
    slot.buffer->open(QIODevice::ReadOnly);

    slot.sink = new QAudioSink(device, m_format, this);
    slot.sink->start(slot.buffer);

#elif defined(_WIN32)
    // Windows waveOut path — fire and forget in a detached thread
    // Build a complete WAV in memory (header + PCM data)
    QByteArray wav;
    int dataSize = pcm.size();
    int fileSize = 36 + dataSize;

    // RIFF header
    wav.append("RIFF", 4);
    quint32 chunkSize = static_cast<quint32>(fileSize);
    wav.append(reinterpret_cast<const char*>(&chunkSize), 4);
    wav.append("WAVE", 4);

    // fmt sub-chunk
    wav.append("fmt ", 4);
    quint32 fmtSize = 16;
    wav.append(reinterpret_cast<const char*>(&fmtSize), 4);
    quint16 audioFormat = 1; // PCM
    wav.append(reinterpret_cast<const char*>(&audioFormat), 2);
    quint16 numChannels = 1;
    wav.append(reinterpret_cast<const char*>(&numChannels), 2);
    quint32 sampleRate = SAMPLE_RATE;
    wav.append(reinterpret_cast<const char*>(&sampleRate), 4);
    quint32 byteRate = SAMPLE_RATE * 2; // 16-bit mono
    wav.append(reinterpret_cast<const char*>(&byteRate), 4);
    quint16 blockAlign = 2;
    wav.append(reinterpret_cast<const char*>(&blockAlign), 2);
    quint16 bitsPerSample = 16;
    wav.append(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data sub-chunk
    wav.append("data", 4);
    quint32 dataSizeU = static_cast<quint32>(dataSize);
    wav.append(reinterpret_cast<const char*>(&dataSizeU), 4);
    wav.append(pcm);

    // Play asynchronously using PlaySound with SND_MEMORY | SND_ASYNC | SND_NODEFAULT
    // Copy the WAV data so it persists during async playback
    // PlaySound with SND_ASYNC expects the memory to remain valid.
    // Use a static buffer approach (one sound at a time for simplicity).
    static QByteArray s_wavBuffer;
    s_wavBuffer = wav;
    PlaySoundA(s_wavBuffer.constData(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);

#else
    Q_UNUSED(pcm);
    // No audio backend available on this platform
#endif
}

// ─── Public Sound Methods ────────────────────────────────────────────────────

void SoundEngine::playCardDeal()
{
    playPCM(generateTone(2000.0, 50, 0.4));
}

void SoundEngine::playCardFlip()
{
    playPCM(generateSweep(1500.0, 600.0, 100, 0.35));
}

void SoundEngine::playChipBet()
{
    playPCM(generateTone(3000.0, 30, 0.45));
}

void SoundEngine::playWin()
{
    playPCM(generateSweep(500.0, 1500.0, 300, 0.5));
}

void SoundEngine::playLose()
{
    playPCM(generateSweep(1500.0, 500.0, 300, 0.4));
}

void SoundEngine::playFold()
{
    playPCM(generateTone(200.0, 80, 0.3));
}

void SoundEngine::playCheck()
{
    playPCM(generateTone(1500.0, 20, 0.35));
}

void SoundEngine::playTimer()
{
    playPCM(generateTone(800.0, 10, 0.25));
}

void SoundEngine::playAllIn()
{
    playPCM(generateSweep(200.0, 3000.0, 500, 0.55));
}

void SoundEngine::setVolume(float vol)
{
    m_volume = qBound(0.0f, vol, 1.0f);
}
