#include "PluginProcessor.h"
#include "PluginEditor.h"

const char* const QSubProcessor::noteNames[12] =
    { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

//==============================================================================
QSubProcessor::QSubProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "QSubParams", createParameterLayout())
{
    detRing.fill (0.0f);
}

QSubProcessor::~QSubProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout QSubProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        kSubLevel, "Sub Level",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        kWaveBlend, "Waveform",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        kDrive, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Log-spaced cutoff feels right for a lowpass.
    juce::NormalisableRange<float> cutoffRange (40.0f, 500.0f);
    cutoffRange.setSkewForCentre (120.0f);
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        kCutoff, "Cutoff", cutoffRange, 120.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        kAttack, "Attack",
        juce::NormalisableRange<float> (1.0f, 200.0f, 0.1f), 5.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        kRelease, "Release",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f), 200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        kPitchDetect, "Pitch Detect", true));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        kKeyNote, "Key",
        juce::StringArray ({ "C", "C#", "D", "D#", "E", "F",
                             "F#", "G", "G#", "A", "A#", "B" }), 9)); // default A

    return { params.begin(), params.end() };
}

//==============================================================================
void QSubProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    sampleRateHz = sampleRate;
    subPhase = 0.0;
    envState = 0.0f;
    lpState  = 0.0f;
    detLpState = 0.0f;
    smoothFreq = 55.0f;
    detRing.fill (0.0f);
    detRingPos = 0;
    detHopCount = 0;
    detectedFreqHz.store (0.0f);
    setLatencySamples (0); // sub is synthesized fresh; dry passes through undelayed
}

void QSubProcessor::releaseResources() {}

bool QSubProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

float QSubProcessor::frequencyForNote (int noteIndex) const
{
    // Octave 1: C1 = 32.703 Hz (equal temperament from A1 = 55 Hz).
    return 55.0f * std::pow (2.0f, (float (noteIndex) - 9.0f) / 12.0f);
}

juce::String QSubProcessor::getDetectedNoteName() const
{
    const float f = detectedFreqHz.load();
    if (f < 20.0f)
        return "--";

    // Nearest note in octave 1.
    int best = 0;
    float bestDist = 1.0e9f;
    for (int n = 0; n < 12; ++n)
    {
        const float d = std::abs (frequencyForNote (n) - f);
        if (d < bestDist) { bestDist = d; best = n; }
    }
    return juce::String (noteNames[best]) + "1";
}

void QSubProcessor::runPitchDetection (const float* mono, int numSamples)
{
    // One-pole lowpass at ~200 Hz into the ring buffer.
    const float lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                         * 200.0f / (float) sampleRateHz);

    for (int i = 0; i < numSamples; ++i)
    {
        detLpState += lpCoef * (mono[i] - detLpState);
        detRing[(size_t) detRingPos] = detLpState;
        detRingPos = (detRingPos + 1) % detWindowSize;

        if (++detHopCount >= 512)
        {
            detHopCount = 0;

            // Autocorrelation over lags for 30..200 Hz.
            const int minLag = juce::jmax (1, (int) (sampleRateHz / 200.0));
            const int maxLag = juce::jmin (detWindowSize / 2, (int) (sampleRateHz / 30.0));

            // RMS gate: hold last pitch on near-silence.
            double sumSq = 0.0;
            for (int k = 0; k < detWindowSize; ++k)
                sumSq += (double) detRing[(size_t) k] * detRing[(size_t) k];
            const float rms = (float) std::sqrt (sumSq / detWindowSize);

            if (rms > 0.004f)
            {
                float bestCorr = -1.0f;
                int bestLag = minLag;

                for (int lag = minLag; lag <= maxLag; ++lag)
                {
                    double corr = 0.0, n0 = 0.0, n1 = 0.0;
                    for (int k = 0; k + lag < detWindowSize; ++k)
                    {
                        const float a = detRing[(size_t) k];
                        const float b = detRing[(size_t) (k + lag)];
                        corr += (double) a * b;
                        n0 += (double) a * a;
                        n1 += (double) b * b;
                    }
                    const float norm = (float) (corr / (std::sqrt (n0 * n1) + 1.0e-9));
                    if (norm > bestCorr) { bestCorr = norm; bestLag = lag; }
                }

                if (bestCorr > 0.55f) // require decent periodicity
                {
                    // Parabolic interpolation for sub-sample accuracy.
                    auto corrAt = [&] (int lag) -> double
                    {
                        if (lag < 1 || lag >= detWindowSize - 1) return 0.0;
                        double c = 0.0;
                        for (int k = 0; k + lag < detWindowSize; ++k)
                            c += (double) detRing[(size_t) k] * detRing[(size_t) (k + lag)];
                        return c;
                    };
                    const double cm1 = corrAt (bestLag - 1);
                    const double c0  = corrAt (bestLag);
                    const double cp1 = corrAt (bestLag + 1);
                    const double denom = cm1 - 2.0 * c0 + cp1;
                    double shift = 0.0;
                    if (std::abs (denom) > 1.0e-9)
                        shift = 0.5 * (cm1 - cp1) / denom;

                    const float freq = (float) sampleRateHz / (float) (bestLag + shift);
                    if (freq >= 28.0f && freq <= 210.0f)
                        detectedFreqHz.store (freq);
                }
            }
        }
    }
}

void QSubProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    const float subLevel = apvts.getRawParameterValue (kSubLevel)->load() / 100.0f;
    const float waveBlend = apvts.getRawParameterValue (kWaveBlend)->load() / 100.0f;
    const float drive = 1.0f + apvts.getRawParameterValue (kDrive)->load() / 100.0f * 9.0f;
    const float cutoff = apvts.getRawParameterValue (kCutoff)->load();
    const float attackMs = apvts.getRawParameterValue (kAttack)->load();
    const float releaseMs = apvts.getRawParameterValue (kRelease)->load();
    const bool pitchDetect = apvts.getRawParameterValue (kPitchDetect)->load() > 0.5f;
    const int keyNote = (int) apvts.getRawParameterValue (kKeyNote)->load();

    if (pitchDetect)
    {
        // Feed mono into the detector (stack buffer for typical block sizes).
        if (numSamples <= 2048)
        {
            float mono[2048];
            for (int i = 0; i < numSamples; ++i)
                mono[i] = 0.5f * (left[i] + right[i]);
            runPitchDetection (mono, numSamples);
        }
        else
        {
            std::vector<float> mono ((size_t) numSamples);
            for (int i = 0; i < numSamples; ++i)
                mono[(size_t) i] = 0.5f * (left[i] + right[i]);
            runPitchDetection (mono.data(), numSamples);
        }
    }

    // Target frequency: detected (slewed) or fixed key.
    float targetFreq;
    if (pitchDetect)
    {
        const float det = detectedFreqHz.load();
        targetFreq = det > 20.0f ? det : smoothFreq; // hold last on silence
    }
    else
    {
        targetFreq = frequencyForNote (keyNote);
    }
    // Slew the frequency to avoid zipper jumps (20 ms-ish smoothing).
    const float slewCoef = 1.0f - std::exp (-1.0f / (0.02f * (float) sampleRateHz));
    // (applied per sample below)

    const float attackCoef  = 1.0f - std::exp (-1.0f / ((attackMs  / 1000.0f) * (float) sampleRateHz));
    const float releaseCoef = 1.0f - std::exp (-1.0f / ((releaseMs / 1000.0f) * (float) sampleRateHz));
    const float lpCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                         * cutoff / (float) sampleRateHz);
    const float driveNorm = 1.0f / std::tanh (drive);
    const float triAmt = waveBlend;
    const float sineAmt = 1.0f - waveBlend;
    const float subGain = subLevel * 0.9f; // headroom

    const double phaseIncBase = 1.0 / sampleRateHz;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right[i];
        const float monoIn = 0.5f * (inL + inR);

        // Envelope follower on the input.
        const float envCoef = std::abs (monoIn) > envState ? attackCoef : releaseCoef;
        envState += envCoef * (std::abs (monoIn) - envState);

        // Slewed frequency.
        smoothFreq += slewCoef * (targetFreq - smoothFreq);
        const float freq = juce::jlimit (25.0f, 220.0f, smoothFreq);

        // Sub oscillator.
        subPhase += freq * phaseIncBase;
        if (subPhase >= 1.0) subPhase -= 1.0;
        const float sine = std::sin (2.0f * juce::MathConstants<float>::pi * (float) subPhase);
        const float tri  = 2.0f * std::abs (2.0f * (float) subPhase - 1.0f) - 1.0f;
        float sub = sineAmt * sine + triAmt * tri;

        // Drive then lowpass.
        sub = std::tanh (drive * sub) * driveNorm;
        lpState += lpCoef * (sub - lpState);

        const float subOut = lpState * subGain * juce::jmin (envState * 3.0f, 1.0f);

        left[i]  = inL + subOut;
        right[i] = inR + subOut;
    }
}

//==============================================================================
const juce::String QSubProcessor::getName() const { return "QSub"; }
bool QSubProcessor::acceptsMidi() const { return false; }
bool QSubProcessor::producesMidi() const { return false; }
bool QSubProcessor::isMidiEffect() const { return false; }
double QSubProcessor::getTailLengthSeconds() const { return 0.0; }

int QSubProcessor::getNumPrograms() { return 1; }
int QSubProcessor::getCurrentProgram() { return 0; }
void QSubProcessor::setCurrentProgram (int) {}
const juce::String QSubProcessor::getProgramName (int) { return {}; }
void QSubProcessor::changeProgramName (int, const juce::String&) {}

void QSubProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void QSubProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* QSubProcessor::createEditor() { return new QSubEditor (*this); }
bool QSubProcessor::hasEditor() const { return true; }

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QSubProcessor();
}
