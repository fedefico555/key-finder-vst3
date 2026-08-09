#include "PluginProcessor.h"
#include "PluginEditor.h"

KeyFinderAudioProcessor::KeyFinderAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

KeyFinderAudioProcessor::~KeyFinderAudioProcessor() = default;

void KeyFinderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    keyDetector.prepare(sampleRate, samplesPerBlock);
}

void KeyFinderAudioProcessor::releaseResources()
{
    keyDetector.reset();
}

bool KeyFinderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accettiamo mono o stereo, in e out devono corrispondere
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void KeyFinderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels > 0 && numSamples > 0)
    {
        // Mix a mono per l'analisi (media dei canali), il plugin non altera
        // l'audio: è un analizzatore "in linea" (pass-through).
        std::vector<float> monoBuffer((size_t) numSamples, 0.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                monoBuffer[(size_t) i] += channelData[i] / (float) numChannels;
        }

        keyDetector.pushAudioBlock(monoBuffer.data(), numSamples);
    }

    // Nessuna modifica al segnale: l'audio esce identico a come è entrato.
}

juce::AudioProcessorEditor* KeyFinderAudioProcessor::createEditor()
{
    return new KeyFinderAudioProcessorEditor(*this);
}

bool KeyFinderAudioProcessor::hasEditor() const { return true; }

const juce::String KeyFinderAudioProcessor::getName() const { return JucePlugin_Name; }

bool KeyFinderAudioProcessor::acceptsMidi() const { return false; }
bool KeyFinderAudioProcessor::producesMidi() const { return false; }
bool KeyFinderAudioProcessor::isMidiEffect() const { return false; }
double KeyFinderAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int KeyFinderAudioProcessor::getNumPrograms() { return 1; }
int KeyFinderAudioProcessor::getCurrentProgram() { return 0; }
void KeyFinderAudioProcessor::setCurrentProgram(int) {}
const juce::String KeyFinderAudioProcessor::getProgramName(int) { return {}; }
void KeyFinderAudioProcessor::changeProgramName(int, const juce::String&) {}

void KeyFinderAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void KeyFinderAudioProcessor::setStateInformation(const void*, int) {}

// Questa funzione viene richiesta da JUCE per creare l'istanza del plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyFinderAudioProcessor();
}
