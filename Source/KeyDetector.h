#pragma once
#include <JuceHeader.h>
#include "CamelotWheel.h"

/**
    Analizza un flusso audio in ingresso e stima la tonalità in tempo reale.

    Pipeline:
      1. Finestra di campioni -> FFT
      2. Magnitudine spettro -> proiezione sui 12 pitch class (chroma vector),
         usando mappatura log-frequenza -> nota, ottava-invariante
      3. Chroma vector smussato nel tempo (media mobile esponenziale)
      4. Correlazione con i 24 profili tonali di Krumhansl-Schmuckler
         (12 maggiori + 12 minori, ciascuno ruotato) -> tonalità con
         correlazione massima = stima corrente, con relativo indice di
         confidenza (margine rispetto alla seconda ipotesi migliore).
*/
class KeyDetector
{
public:
    KeyDetector();

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();

    // Da chiamare per ogni blocco audio ricevuto da processBlock()
    void pushAudioBlock(const float* samples, int numSamples);

    struct Result
    {
        int pitchClass = 0;
        bool isMinor = false;
        float confidence = 0.0f;       // 0..1
        float inputLevelDb = -100.0f;  // per il VU meter
        CamelotWheel::KeyInfo keyInfo;
        bool valid = false;
    };

    Result getCurrentResult() const;

private:
    static constexpr int fftOrder = 12;               // 2^12 = 4096 campioni
    static constexpr int fftSize = 1 << fftOrder;

    void processFrame();
    void computeChromaFromMagnitudes();
    void correlateWithProfiles();

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize,
                                                  juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> fifo;
    std::vector<float> fftData;
    int fifoIndex = 0;

    double sampleRate = 44100.0;

    std::array<float, 12> chroma {};          // chroma corrente (frame singolo)
    std::array<float, 12> smoothedChroma {};  // chroma smussato nel tempo

    // Profili tonali di Krumhansl-Schmuckler (pesi di rilevanza per ciascun
    // grado della scala, a partire dalla tonica)
    static const std::array<float, 12> majorProfile;
    static const std::array<float, 12> minorProfile;

    std::atomic<int> currentPitchClass { 0 };
    std::atomic<bool> currentIsMinor { false };
    std::atomic<float> currentConfidence { 0.0f };
    std::atomic<float> currentLevelDb { -100.0f };
    std::atomic<bool> hasValidResult { false };

    float runningLevel = 0.0f;
};
