#pragma once
#include <QObject>
#include <QByteArray>
#include <cmath>

#ifdef HAS_QT_MULTIMEDIA
#include <QBuffer>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#endif

/**
 * SoundEngine — Procedural sound effects for poker.
 *
 * Generates sine-wave PCM audio in memory (no WAV files needed).
 * Uses Qt Multimedia (QAudioSink) if available, otherwise Windows waveOut.
 */
class SoundEngine : public QObject {
    Q_OBJECT
public:
    static SoundEngine& instance();

    void playCardDeal();   // short click 2000Hz 50ms
    void playCardFlip();   // swish sweep 1000Hz 100ms
    void playChipBet();    // clink 3000Hz 30ms
    void playWin();        // ascending 500->1500Hz 300ms
    void playLose();       // descending 1500->500Hz 300ms
    void playFold();       // soft thud 200Hz 80ms
    void playCheck();      // tap 1500Hz 20ms
    void playTimer();      // tick 800Hz 10ms
    void playAllIn();      // dramatic sweep 200->3000Hz 500ms

    void setVolume(float vol); // 0.0 .. 1.0
    void setEnabled(bool on) { m_enabled = on; }
    bool isEnabled() const { return m_enabled; }

private:
    explicit SoundEngine(QObject* parent = nullptr);
    ~SoundEngine() override = default;

    // Generate raw PCM 16-bit mono samples at 44100Hz
    QByteArray generateTone(double freqHz, int durationMs, double amplitude = 0.5);
    QByteArray generateSweep(double startHz, double endHz, int durationMs, double amplitude = 0.5);

    // Play raw PCM data
    void playPCM(const QByteArray& pcm);

    float m_volume = 0.6f;
    bool m_enabled = true;
    static const int SAMPLE_RATE = 44100;

#ifdef HAS_QT_MULTIMEDIA
    QAudioFormat m_format;
    struct PlaybackSlot {
        QBuffer* buffer = nullptr;
        QAudioSink* sink = nullptr;
    };
    static const int MAX_SLOTS = 4;
    PlaybackSlot m_slots[MAX_SLOTS];
    int m_nextSlot = 0;
#endif
};
