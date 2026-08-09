#include "KeyDetector.h"

// Profili tonali di Krumhansl-Kessler (letteratura MIR standard)
const std::array<float, 12> KeyDetector::majorProfile {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};

const std::array<float, 12> KeyDetector::minorProfile {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

KeyDetector::KeyDetector()
{
    ring.resize((size_t) fftSize, 0.0f);
    fftData.resize((size_t) fftSize * 2, 0.0f);
    magnitudes.resize((size_t) fftSize / 2, 0.0f);
}

void KeyDetector::prepare(double newSampleRate, int /*maximumBlockSize*/)
{
    sampleRate = newSampleRate;
    reset();
}

void KeyDetector::reset()
{
    std::fill(ring.begin(), ring.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(magnitudes.begin(), magnitudes.end(), 0.0f);
    smoothedScores.fill(0.0f);
    scoresInitialised = false;

    ringWritePos = 0;
    samplesSinceLastAnalysis = 0;
    runningLevel = 0.0f;

    displayedIndex = -1;
    pendingIndex = -1;
    pendingStreak = 0;

    hasValidResult = false;
    currentConfidence = 0.0f;
    currentLevelDb = -100.0f;
}

void KeyDetector::pushAudioBlock(const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = samples[i];

        runningLevel = 0.995f * runningLevel + 0.005f * std::abs(s);

        ring[(size_t) ringWritePos] = s;
        ringWritePos = (ringWritePos + 1) % fftSize;
        ++samplesSinceLastAnalysis;

        if (samplesSinceLastAnalysis >= hopSize)
        {
            samplesSinceLastAnalysis = 0;

            const float db = juce::Decibels::gainToDecibels(runningLevel, -100.0f);
            currentLevelDb = db;

            processFrame();
        }
    }

    const float db = juce::Decibels::gainToDecibels(runningLevel, -100.0f);
    currentLevelDb = db;
}

void KeyDetector::processFrame()
{
    // Estrae la finestra di analisi dal buffer circolare, in ordine cronologico
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringWritePos + i) % fftSize)];

    window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const int numBins = fftSize / 2;
    for (int i = 0; i < numBins; ++i)
        magnitudes[(size_t) i] = fftData[(size_t) i];

    // Cancello di silenzio: sotto questa soglia non aggiorniamo la stima
    // (il livello per il VU meter viene comunque aggiornato altrove).
    if (currentLevelDb.load() < -55.0f)
        return;

    std::array<float, 12> fullChroma {};
    std::array<float, 12> bassChroma {};
    computeChromaFromMagnitudes(fullChroma, 60.0, 5000.0);
    computeChromaFromMagnitudes(bassChroma, 40.0, 300.0);

    // Il basso è un indizio forte della tonica nella musica moderna prodotta:
    // gli diamo più peso rispetto al resto dello spettro.
    constexpr float bassWeight = 1.8f;

    std::array<float, 12> combined {};
    float sum = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        combined[(size_t) i] = fullChroma[(size_t) i] + bassWeight * bassChroma[(size_t) i];
        sum += combined[(size_t) i];
    }
    if (sum > 1.0e-6f)
        for (auto& v : combined)
            v /= sum;

    correlateAndAccumulate(combined);
}

void KeyDetector::computeChromaFromMagnitudes(std::array<float, 12>& outChroma, double minFreqHz, double maxFreqHz) const
{
    outChroma.fill(0.0f);

    const int numBins = (int) magnitudes.size();
    if (numBins < 4)
        return;

    const double binWidth = sampleRate / (double) fftSize;

    // Consideriamo solo i PICCHI locali dello spettro (non ogni singolo bin):
    // sono molto più rappresentativi delle vere componenti tonali e molto
    // meno sensibili al rumore/allargamento spettrale.
    for (int bin = 2; bin < numBins - 2; ++bin)
    {
        const float m0 = magnitudes[(size_t) (bin - 1)];
        const float m1 = magnitudes[(size_t) bin];
        const float m2 = magnitudes[(size_t) (bin + 1)];

        if (m1 <= m0 || m1 <= m2)
            continue;

        const double freqApprox = bin * binWidth;
        if (freqApprox < minFreqHz || freqApprox > maxFreqHz)
            continue;

        // Interpolazione parabolica per una stima di frequenza più precisa
        // della sola risoluzione del bin FFT.
        const float denom = (m0 - 2.0f * m1 + m2);
        const float p = std::abs(denom) > 1.0e-9f ? juce::jlimit(-0.5f, 0.5f, 0.5f * (m0 - m2) / denom) : 0.0f;
        const double freq = ((double) bin + (double) p) * binWidth;
        if (freq <= 0.0)
            continue;

        const double midiNote = 69.0 + 12.0 * std::log2(freq / 440.0);
        double pcContinuous = std::fmod(midiNote, 12.0);
        if (pcContinuous < 0.0)
            pcContinuous += 12.0;

        int nearest = (int) std::lround(pcContinuous);
        const double frac = pcContinuous - (double) nearest;
        nearest = ((nearest % 12) + 12) % 12;

        // Diffusione triangolare tra il pitch class più vicino e il suo
        // vicino nella direzione dell'errore di arrotondamento, invece di
        // un arrotondamento secco: riduce il rumore di quantizzazione.
        const int neighbour = ((nearest + (frac >= 0.0 ? 1 : -1)) % 12 + 12) % 12;
        const float w0 = (float) (1.0 - std::abs(frac));
        const float w1 = (float) std::abs(frac);

        outChroma[(size_t) nearest] += m1 * w0;
        outChroma[(size_t) neighbour] += m1 * w1;
    }
}

static float correlate(const std::array<float, 12>& chroma, const std::array<float, 12>& profile, int rotation)
{
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

    return numerator / denom;
}

void KeyDetector::correlateAndAccumulate(const std::array<float, 12>& chroma)
{
    std::array<float, 24> rawScores {};
    for (int pc = 0; pc < 12; ++pc)
    {
        rawScores[(size_t) pc]      = correlate(chroma, majorProfile, pc);
        rawScores[(size_t) pc + 12] = correlate(chroma, minorProfile, pc);
    }

    // Integrazione lenta nel tempo: il risultato riflette diversi secondi di
    // audio, non un singolo frame. Con hop = fftSize/2 a 44.1 kHz un frame
    // arriva circa ogni 93 ms; alpha=0.06 corrisponde a una costante di
    // tempo di alcuni secondi.
    constexpr float alpha = 0.06f;
    if (!scoresInitialised)
    {
        smoothedScores = rawScores;
        scoresInitialised = true;
    }
    else
    {
        for (int i = 0; i < 24; ++i)
            smoothedScores[(size_t) i] = alpha * rawScores[(size_t) i] + (1.0f - alpha) * smoothedScores[(size_t) i];
    }

    int bestIndex = 0;
    float bestScore = smoothedScores[0];
    for (int i = 1; i < 24; ++i)
    {
        if (smoothedScores[(size_t) i] > bestScore)
        {
            bestScore = smoothedScores[(size_t) i];
            bestIndex = i;
        }
    }

    // --- Isteresi: cambiamo la tonalità mostrata solo se una nuova ipotesi
    //     resta stabilmente in testa per diversi frame consecutivi. ---
    if (displayedIndex < 0)
    {
        displayedIndex = bestIndex;
        pendingIndex = -1;
        pendingStreak = 0;
    }
    else if (bestIndex == displayedIndex)
    {
        pendingIndex = -1;
        pendingStreak = 0;
    }
    else
    {
        if (pendingIndex == bestIndex)
            ++pendingStreak;
        else
        {
            pendingIndex = bestIndex;
            pendingStreak = 1;
        }

        if (pendingStreak >= streakToSwitch)
        {
            displayedIndex = bestIndex;
            pendingIndex = -1;
            pendingStreak = 0;
        }
    }

    // Confidenza: margine tra il punteggio della tonalità mostrata e la
    // migliore fra tutte le altre 23 ipotesi.
    float bestOther = -2.0f;
    for (int i = 0; i < 24; ++i)
        if (i != displayedIndex)
            bestOther = juce::jmax(bestOther, smoothedScores[(size_t) i]);

    const float margin = juce::jmax(0.0f, smoothedScores[(size_t) displayedIndex] - bestOther);
    const float confidence = juce::jlimit(0.0f, 1.0f, margin * 2.2f);

    currentPitchClass = displayedIndex % 12;
    currentIsMinor = displayedIndex >= 12;
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
