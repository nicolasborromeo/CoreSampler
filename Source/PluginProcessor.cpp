#include "PluginProcessor.h"
#include "PluginEditor.h"

#if JUCE_STANDALONE_APPLICATION
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace ParamIDs
{
    inline const juce::String volume     = "volume";
    inline const juce::String gain       = "gain";
    inline const juce::String fade       = "fade";
    inline const juce::String attack     = "attack";
    inline const juce::String decay      = "decay";
    inline const juce::String sustain    = "sustain";
    inline const juce::String release    = "release";
    inline const juce::String filterFreq = "filterFreq";
    inline const juce::String filterRes  = "filterRes";
    inline const juce::String transpose   = "transpose";
    inline const juce::String startPoint  = "startPoint";
    inline const juce::String endPoint    = "endPoint";
    inline const juce::String filterType  = "filterType";
    inline const juce::String filterSlope = "filterSlope";
}

juce::AudioProcessorValueTreeState::ParameterLayout
CoreSamplerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::volume,  "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f));

    // Gain: sample trim, 0 = silent, 1 = unity, 2 = double
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::gain, "Gain",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f, 0.5f), 1.0f));

    // Fade: fade-in at the start of each note (removes click at sample start point)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::fade, "Fade",
        juce::NormalisableRange<float> (0.0f, 0.2f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::attack,  "Attack",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.5f), 0.001f));  // 1 ms — lets transient through

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::decay,   "Decay",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.5f), 0.1f));   // 100 ms

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::sustain, "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));                    // 100% — full sustain

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::release, "Release",
        juce::NormalisableRange<float> (0.001f, 10.0f, 0.001f, 0.5f), 0.030f)); // 30 ms — no click, no tail

    // Filter frequency: 20 Hz to 20 kHz, log-ish scale.
    // Default = 20 kHz (fully open) so it has no effect until turned down.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::filterFreq, "Filter Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));

    // Resonance: 0.1 = flat (no resonance), higher = boosted peak at cutoff frequency
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::filterRes, "Filter Res",
        juce::NormalisableRange<float> (0.1f, 5.0f, 0.01f), 0.707f));

    // Transpose: integer semitones — shifts all played notes up or down
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamIDs::transpose, "Transpose", -24, 24, 0));

    // Start point: 0% to 90% through the sample.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::startPoint, "Start",
        juce::NormalisableRange<float> (0.0f, 0.9f, 0.001f), 0.0f));

    // End point: where playback stops (must be > startPoint).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::endPoint, "End",
        juce::NormalisableRange<float> (0.1f, 1.0f, 0.001f), 1.0f));

    // Filter type: 0 = LPF, 1 = HPF
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::filterType, "Filter Type",
        juce::StringArray { "LPF", "HPF" }, 0));

    // Filter slope: 0 = 12dB/oct (1 pole pair), 1 = 24dB/oct (2 pole pairs cascaded)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::filterSlope, "Filter Slope",
        juce::StringArray { "12dB", "24dB" }, 0));

    return { params.begin(), params.end() };
}

CoreSamplerProcessor::CoreSamplerProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      thumbnail (512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();

    for (int i = 0; i < NUM_VOICES; ++i)
        sampler.addVoice (new CoreSamplerVoice (adsrParams, fileSampleRate,
                          *apvts.getRawParameterValue (ParamIDs::fade)));

}

CoreSamplerProcessor::~CoreSamplerProcessor() {}

// ============================================================
// loadSample
//
// Reads the full audio file into RAM, updates the waveform
// thumbnail, and builds the first SamplerSound.
// Called from the editor's load button (message thread).
// ============================================================
void CoreSamplerProcessor::loadSample (const juce::File& file)
{
    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr) return;

    fileSampleRate   = reader->sampleRate;
    int totalSamples = (int) reader->lengthInSamples;
    int numCh        = (int) juce::jmin ((unsigned int) 2, reader->numChannels);

    fileData.setSize (numCh, totalSamples, false, true, false);
    reader->read (&fileData, 0, totalSamples, 0, true, numCh > 1);
    delete reader;

    // Give the thumbnail a source so it can draw the waveform in the editor
    thumbnail.setSource (new juce::FileInputSource (file));

    // Reset start/end markers to the full sample on every new load
    if (auto* p = apvts.getParameter (ParamIDs::startPoint))
        p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
    if (auto* p = apvts.getParameter (ParamIDs::endPoint))
        p->setValueNotifyingHost (p->convertTo0to1 (1.0f));

    rebuildSamplerSound();
}

// ============================================================
// rebuildSamplerSound
//
// Creates a new SamplerSound from the cached audio data,
// starting at the current start-point position.
// Safe to call from the message thread.
// ============================================================
void CoreSamplerProcessor::rebuildSamplerSound()
{
    if (fileData.getNumSamples() == 0) return;

    float startFraction = apvts.getRawParameterValue (ParamIDs::startPoint)->load();
    float endFraction   = apvts.getRawParameterValue (ParamIDs::endPoint)->load();

    int totalSamples = fileData.getNumSamples();
    int startSample  = juce::jlimit (0, totalSamples - 1, (int) (startFraction * totalSamples));
    int endSample    = juce::jlimit (startSample + 1, totalSamples, (int) (endFraction * totalSamples));
    int length       = endSample - startSample;

    double remainingSecs = (double) length / fileSampleRate;

    sampler.clearSounds();

    auto* reader = new BufferReader (fileData, fileSampleRate, startSample, length);

    juce::BigInteger midiNotes;
    midiNotes.setRange (0, 128, true);

    sampler.addSound (new juce::SamplerSound (
        "Sample", *reader, midiNotes,
        60,              // root note: middle C plays at original pitch
        0.01,            // attack (seconds)
        0.1,             // release (seconds)
        remainingSecs    // max duration of the sound
    ));

    delete reader;
}

// ============================================================
// prepareToPlay — called before audio starts
// ============================================================
void CoreSamplerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampler.setCurrentPlaybackSampleRate (sampleRate);

    // In standalone mode, auto-enable every available MIDI input so the user
    // never has to open Options → Audio/MIDI Settings manually.
#if JUCE_STANDALONE_APPLICATION
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        for (auto& d : juce::MidiInput::getAvailableDevices())
            holder->deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
#endif

    // Prepare the filter for the current sample rate and buffer size.
    // We use numChannels=1 and process L/R separately below.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 1;

    filterL.prepare (spec);  filterR.prepare (spec);
    filterL2.prepare (spec); filterR2.prepare (spec);
    filterL.setType  (juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.setType  (juce::dsp::StateVariableTPTFilterType::lowpass);
    filterL2.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR2.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
}

void CoreSamplerProcessor::releaseResources()
{
    filterL.reset();  filterR.reset();
    filterL2.reset(); filterR2.reset();
}

// ============================================================
// processBlock — called hundreds of times per second
// ============================================================
void CoreSamplerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    float volume     = apvts.getRawParameterValue (ParamIDs::volume)->load()
                       * apvts.getRawParameterValue (ParamIDs::gain)->load();
    float filterFreq = apvts.getRawParameterValue (ParamIDs::filterFreq)->load();
    float filterRes  = apvts.getRawParameterValue (ParamIDs::filterRes)->load();
    int   transpose  = (int) apvts.getRawParameterValue (ParamIDs::transpose)->load();

    // Update shared ADSR struct — each CoreSamplerVoice reads this every block
    adsrParams.attack  = apvts.getRawParameterValue (ParamIDs::attack)->load();
    adsrParams.decay   = apvts.getRawParameterValue (ParamIDs::decay)->load();
    adsrParams.sustain = apvts.getRawParameterValue (ParamIDs::sustain)->load();
    adsrParams.release = apvts.getRawParameterValue (ParamIDs::release)->load();

    // --- 1. Apply transpose ---
    // Shift every MIDI note number by the transpose amount before playing.
    juce::MidiBuffer transposedMidi;
    for (const auto metadata : midiMessages)
    {
        auto msg    = metadata.getMessage();
        int  pos    = metadata.samplePosition;

        if (msg.isNoteOn() || msg.isNoteOff())
        {
            int newNote = juce::jlimit (0, 127, msg.getNoteNumber() + transpose);
            if (msg.isNoteOn())
                msg = juce::MidiMessage::noteOn  (msg.getChannel(), newNote, msg.getVelocity());
            else
                msg = juce::MidiMessage::noteOff (msg.getChannel(), newNote, msg.getVelocity());
        }
        transposedMidi.addEvent (msg, pos);
    }

    // --- 2. Render audio ---
    buffer.clear();
    sampler.renderNextBlock (buffer, transposedMidi, 0, buffer.getNumSamples());

    // --- 3. Apply filter ---
    int filterTypeIdx  = (int) apvts.getRawParameterValue (ParamIDs::filterType)->load();
    int filterSlopeIdx = (int) apvts.getRawParameterValue (ParamIDs::filterSlope)->load();

    auto fType = (filterTypeIdx == 0) ? juce::dsp::StateVariableTPTFilterType::lowpass
                                      : juce::dsp::StateVariableTPTFilterType::highpass;
    filterL.setType (fType);  filterR.setType (fType);
    filterL2.setType (fType); filterR2.setType (fType);

    filterL.setCutoffFrequency (filterFreq);  filterL.setResonance (filterRes);
    filterR.setCutoffFrequency (filterFreq);  filterR.setResonance (filterRes);
    filterL2.setCutoffFrequency (filterFreq); filterL2.setResonance (filterRes);
    filterR2.setCutoffFrequency (filterFreq); filterR2.setResonance (filterRes);

    if (buffer.getNumChannels() > 0)
    {
        auto* data = buffer.getWritePointer (0);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            data[s] = filterL.processSample (0, data[s]);
            if (filterSlopeIdx == 1) data[s] = filterL2.processSample (0, data[s]);
        }
    }
    if (buffer.getNumChannels() > 1)
    {
        auto* data = buffer.getWritePointer (1);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            data[s] = filterR.processSample (0, data[s]);
            if (filterSlopeIdx == 1) data[s] = filterR2.processSample (0, data[s]);
        }
    }

    // --- 4. Apply volume ---
    buffer.applyGain (volume);
}

void CoreSamplerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CoreSamplerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* CoreSamplerProcessor::createEditor()
{
    return new CoreSamplerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CoreSamplerProcessor();
}
