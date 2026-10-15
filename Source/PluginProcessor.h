#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

//==============================================================================
/*
    QSub — 808 sub-bass generator for Headroom Audio.

    Real-time insert effect. Signal path per sample:
      input -> pitch detector (autocorrelation, 30-200 Hz)
            -> envelope follower (attack/release)
            -> sub oscillator (sine/triangle blend at detected or fixed pitch)
            -> tanh drive -> one-pole lowpass -> subLevel * envelope
      output = dry input + sub voice

    The sub is synthesized fresh (not a delayed copy), so the plugin reports
    zero latency.
*/
class QSubProcessor : public juce::AudioProcessor
{
public:
    QSubProcessor();
    ~QSubProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- parameters ----
    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* kSubLevel    = "subLevel";    // %, 0..100
    static constexpr const char* kWaveBlend   = "waveBlend";   // %, 0=sine..100=triangle
    static constexpr const char* kDrive       = "drive";       // %, 0..100
    static constexpr const char* kCutoff      = "cutoff";      // Hz, 40..500
    static constexpr const char* kAttack      = "attack";      // ms, 1..200
    static constexpr const char* kRelease     = "release";     // ms, 20..1000
    static constexpr const char* kPitchDetect = "pitchDetect"; // bool
    static constexpr const char* kKeyNote     = "keyNote";     // choice 0..11 (C..B)

    static const char* const noteNames[12];

    // Thread-safe readouts for the UI.
    float getDetectedFrequency() const noexcept { return detectedFreqHz.load(); }
    juce::String getDetectedNoteName() const;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Pitch detection: lowpassed input -> 2048-sample ring -> autocorrelation
    // over lags for 30..200 Hz. Runs at block rate, not per sample.
    void runPitchDetection (const float* mono, int numSamples);

    float frequencyForNote (int noteIndex) const; // octave 1: C1=32.70 .. B1=61.74

    // --- per-sample DSP state ---
    double sampleRateHz = 44100.0;
    double subPhase = 0.0;          // 0..1
    float  envState = 0.0f;         // envelope follower
    float  lpState  = 0.0f;         // one-pole lowpass on the sub
    float  detLpState = 0.0f;       // lowpass for the pitch detector input
    float  smoothFreq = 55.0f;      // slewed detected frequency

    static constexpr int detWindowSize = 2048;
    std::array<float, detWindowSize> detRing {};
    int detRingPos = 0;
    int detHopCount = 0;

    std::atomic<float> detectedFreqHz { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QSubProcessor)
};
