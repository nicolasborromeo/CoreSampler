#pragma once
#include <JuceHeader.h>

// ============================================================
// BufferReader — reads from an in-memory AudioBuffer at any offset
// ============================================================
struct BufferReader : public juce::AudioFormatReader
{
    BufferReader (const juce::AudioBuffer<float>& buf, double sr, int startSample, int length)
        : juce::AudioFormatReader (nullptr, "Buffer"), source (&buf), offset (startSample)
    {
        sampleRate            = sr;
        numChannels           = (unsigned int) buf.getNumChannels();
        lengthInSamples       = (juce::int64) length;
        bitsPerSample         = 32;
        usesFloatingPointData = true;
    }

    bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDest,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            if (destChannels[ch] == nullptr) continue;
            int srcCh = juce::jmin (ch, source->getNumChannels() - 1);
            const float* src = source->getReadPointer (srcCh) + offset + (int) startSampleInFile;
            float*       dst = reinterpret_cast<float*> (destChannels[ch]) + startOffsetInDest;
            juce::FloatVectorOperations::copy (dst, src, numSamples);
        }
        return true;
    }

    const juce::AudioBuffer<float>* source;
    int offset;
};

// ============================================================
// CoreSamplerVoice
//
// Custom synthesiser voice that reads from a SamplerSound but
// applies OUR ADSR parameters (so the knobs actually work).
// juce::SamplerVoice bakes ADSR in at sound-creation time and
// exposes no way to change it dynamically — this solves that.
// ============================================================
class CoreSamplerVoice : public juce::SynthesiserVoice
{
public:
    // adsrP  : shared reference to the processor's ADSR struct
    // srcSR  : shared reference to the processor's fileSampleRate
    //          (used to calculate pitch ratio — tells us the original pitch)
    CoreSamplerVoice (const juce::ADSR::Parameters& adsrP, const double& srcSR,
                      const std::atomic<float>& fadeP, std::atomic<double>& playbackPos)
        : adsrParams (adsrP), fileSR (srcSR), fadeParam (fadeP), playbackPos (playbackPos) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<juce::SamplerSound*> (s) != nullptr;
    }

    void startNote (int midiNote, float velocity,
                    juce::SynthesiserSound* s, int /*pitchWheel*/) override
    {
        if (dynamic_cast<juce::SamplerSound*> (s) != nullptr)
        {
            // Middle C (note 60) plays at original pitch; every semitone doubles/halves speed.
            // fileSR / getSampleRate() corrects for any sample-rate mismatch.
            pitchRatio  = std::pow (2.0, (midiNote - 60) / 12.0)
                          * fileSR / getSampleRate();
            position    = 0.0;
            lgain       = velocity;
            rgain       = velocity;
            fadeSamples = (int) (fadeParam.load() * getSampleRate());
            fadeCounter = 0;
            adsr.setSampleRate (getSampleRate());
            adsr.setParameters (adsrParams);
            adsr.noteOn();
        }
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff) { adsr.noteOff(); }
        else              { clearCurrentNote(); adsr.reset(); position = 0.0; playbackPos.store (-1.0); }
    }

    void renderNextBlock (juce::AudioBuffer<float>& out,
                          int startSample, int numSamples) override
    {
        if (auto* sound = dynamic_cast<const juce::SamplerSound*> (getCurrentlyPlayingSound().get()))
        {
            const auto& data = *sound->getAudioData();
            const float* srcL = data.getReadPointer (0);
            const float* srcR = data.getNumChannels() > 1 ? data.getReadPointer (1) : srcL;

            // Pick up any knob changes on the current note
            adsr.setParameters (adsrParams);

            float* dstL = out.getWritePointer (0, startSample);
            float* dstR = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : nullptr;
            int    len  = data.getNumSamples();

            while (--numSamples >= 0)
            {
                int   pos   = (int) position;
                float alpha = (float) (position - pos);
                float inv   = 1.0f - alpha;

                // Linear interpolation between adjacent samples
                float l = pos + 1 < len ? srcL[pos] * inv + srcL[pos + 1] * alpha
                                        : srcL[juce::jmin (pos, len - 1)];
                float r = pos + 1 < len ? srcR[pos] * inv + srcR[pos + 1] * alpha
                                        : srcR[juce::jmin (pos, len - 1)];

                float env      = adsr.getNextSample();
                float fadeGain = (fadeCounter < fadeSamples)
                                   ? (float) fadeCounter / (float) fadeSamples
                                   : 1.0f;
                ++fadeCounter;
                *dstL++ += l * lgain * env * fadeGain;
                if (dstR != nullptr) *dstR++ += r * rgain * env * fadeGain;

                position += pitchRatio;

                if (position >= len || !adsr.isActive())
                {
                    clearCurrentNote();
                    adsr.reset();
                    playbackPos.store (-1.0);
                    break;
                }
            }

            // Publish normalised position (0–1 within trimmed region) for the UI
            if (isVoiceActive())
                playbackPos.store (juce::jlimit (0.0, 1.0, position / (double) len));
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    const juce::ADSR::Parameters& adsrParams;
    const double&                 fileSR;
    const std::atomic<float>&     fadeParam;
    std::atomic<double>&          playbackPos;
    double     position    = 0.0;
    float      lgain = 0.0f, rgain = 0.0f;
    double     pitchRatio  = 0.0;
    int        fadeSamples = 0;
    int        fadeCounter = 0;
    juce::ADSR adsr;
};

// ============================================================
// CoreSamplerProcessor
// ============================================================
class CoreSamplerProcessor : public juce::AudioProcessor
{
public:
    CoreSamplerProcessor();
    ~CoreSamplerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override             { return true; }
    const juce::String getName() const override { return "Core Sampler"; }
    bool acceptsMidi() const override           { return true; }
    bool producesMidi() const override          { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                              { return 1; }
    int getCurrentProgram() override                           { return 0; }
    void setCurrentProgram (int) override                      {}
    const juce::String getProgramName (int) override           { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadSample (const juce::File& file);
    void rebuildSamplerSound();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail      thumbnail;

    // ADSR state shared with all voices — updated each processBlock
    juce::ADSR::Parameters adsrParams;

    // Normalised playback position (0–1 within trimmed region) written by the
    // active voice every audio block. -1.0 means nothing is currently playing.
    std::atomic<double> playbackPosition { -1.0 };

private:
    juce::Synthesiser        sampler;
    juce::AudioFormatManager formatManager;
    static constexpr int     NUM_VOICES = 8;

    juce::AudioBuffer<float> fileData;
    double                   fileSampleRate = 44100.0;

    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;
    juce::dsp::StateVariableTPTFilter<float> filterL2, filterR2;  // second stage for 24dB

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CoreSamplerProcessor)
};
