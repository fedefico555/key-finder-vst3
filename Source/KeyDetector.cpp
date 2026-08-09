#include "KeyDetector.h"

// Profili tonali di Krumhansl-Kessler (pesi percettivi per ciascun grado
// della scala, indice 0 = tonica). Valori standard della letteratura MIR.
const std::array<float, 12> KeyDetector::majorProfile {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};

const std::array<float, 12> KeyDetector::minorProfile {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

KeyDetector::KeyDetector()
{
    fifo.resize((size_t) fftSize, 0.0f);
    fftData.resize((size_t) fftSize * 2, 0.0f);
}

void KeyDetector::prepare(double newSampleRate, int /*maximumBlockSize*/)
{
    sampleRate = newSampleRate;
    reset();
}

void KeyDetector::reset()
{
    std::fill(fifo.begin(), fifo.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    chroma.fill(0.0f);
    smoothedChroma.fill(0.0f);
    fifoIndex = 0;
    runningLevel = 0.0f;
    hasValidResult = false;
    currentConfidence = 0.0f;
    currentLevelDb = -100.0f;
}

void KeyDetector::pushAudioBlock(const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = samples[i];

        // Livello per il VU meter (media mobile del valore assoluto)
        runningLevel = 0.995f * runningLevel + 0.005f * std::abs(s);

        fifo[(size_t) fifoIndex++] = s;

        if (fifoIndex == fftSize)
        {
            fifoIndex = 0;
            processFrame();
        }
    }

    const float db = juce::Decibels::gainToDecibels(runningLevel, -100.0f);
    currentLevelDb = db;
}

void KeyDetector::processFrame()
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::copy(fifo.begin(), fifo.end(), fftData.begin());

    window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    computeChromaFromMagnitudes();
    correlateWithProfiles();
}

void KeyDetector::computeChromaFromMagnitudes()
{
    chroma.fill(0.0f);

    const int numBins = fftSize / 2;
    const double binWidth = sampleRate / (double) fftSize;

    // Ignoriamo le frequenze troppo basse (rumble) e troppo alte (poco
    // rilevanti per l'armonia), tipicamente 60 Hz - 5 kHz.
    const double minFreq = 60.0;
    const double maxFreq = 5000.0;

    for (int bin = 1; bin < numBins; ++bin)
    {
        const double freq = bin * binWidth;
        if (freq < minFreq || freq > maxFreq)
            continue;

        const float magnitude = fftData[(size_t) bin];
        if (magnitude <= 0.0f)
            continue;

        // Conversione frequenza -> nota MIDI (continua) -> pitch class
        const double midiNote = 69.0 + 12.0 * std::log2(freq / 440.0);
        int pitchClass = ((int) std::lround(midiNote)) % 12;
        if (pitchClass < 0)
            pitchClass += 12;

        chroma[(size_t) pitchClass] += magnitude;
    }

    // Normalizzazione (somma = 1) per rendere il vettore indipendente dal volume
    float sum = 0.0f;
    for (float v : chroma)
        sum += v;

    if (sum > 1.0e-6f)
    {
        for (float& v : chroma)
            v /= sum;
    }

    // Media mobile esponenziale per stabilizzare la stima nel tempo
    constexpr float alpha = 0.12f;
    for (size_t i = 0; i < 12; ++i)
        smoothedChroma[i] = alpha * chroma[i] + (1.0f - alpha) * smoothedChroma[i];
}

static float correlate(const std::array<float, 12>& chroma, const std::array<float, 12>& profile, int rotation)
{
    // Media dei due vettori
    float meanChroma = 0.0f, meanProfile = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        meanChroma += chroma[(size_t) i];
        meanProfile += profile[(size_t) i];
    }
    meanChroma /= 12.0f;
    meanProfile /= 12.0f;

    float numerator = 0.0f, denomChroma = 0.0f, denomProfile = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        const int rotatedIndex = (i + rotation) % 12;
        const float c = chroma[(size_t) i] - meanChroma;
        const float p = profile[(size_t) rotatedIndex] - meanProfile;
        numerator += c * p;
        denomChroma += c * c;
        denomProfile += p * p;
    }

    const float denom = std::sqrt(denomChroma * denomProfile);
    if (denom < 1.0e-9f)
        return 0.0f;

    return numerator / denom; // coefficiente di correlazione di Pearson, -1..1
}

void KeyDetector::correlateWithProfiles()
{
    float bestScore = -2.0f;
    float secondBestScore = -2.0f;
    int bestPitchClass = 0;
    bool bestIsMinor = false;

    for (int pc = 0; pc < 12; ++pc)
    {
        const float scoreMajor = correlate(smoothedChroma, majorProfile, pc);
        const float scoreMinor = correlate(smoothedChroma, minorProfile, pc);

        if (scoreMajor > bestScore)
        {
            secondBestScore = bestScore;
            bestScore = scoreMajor;
            bestPitchClass = pc;
            bestIsMinor = false;
        }
        else if (scoreMajor > secondBestScore)
        {
            secondBestScore = scoreMajor;
        }

        if (scoreMinor > bestScore)
        {
            secondBestScore = bestScore;
            bestScore = scoreMinor;
            bestPitchClass = pc;
            bestIsMinor = true;
        }
        else if (scoreMinor > secondBestScore)
        {
            secondBestScore = scoreMinor;
        }
    }

    // La confidenza è il margine tra la migliore e la seconda migliore ipotesi,
    // mappato in 0..1
    const float margin = juce::jmax(0.0f, bestScore - secondBestScore);
    const float confidence = juce::jlimit(0.0f, 1.0f, margin * 2.5f);

    currentPitchClass = bestPitchClass;
    currentIsMinor = bestIsMinor;
    currentConfidence = confidence;
    hasValidResult = true;
}

KeyDetector::Result KeyDetector::getCurrentResult() const
{
    Result r;
    r.valid = hasValidResult.load();
    r.pitchClass = currentPitchClass.load();
    r.isMinor = currentIsMinor.load();
    r.confidence = currentConfidence.load();
    r.inputLevelDb = currentLevelDb.load();
    r.keyInfo = CamelotWheel::getKeyInfo(r.pitchClass, r.isMinor);
    return r;
}
