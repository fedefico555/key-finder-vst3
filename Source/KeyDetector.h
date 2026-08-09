#pragma once
#include <JuceHeader.h>
#include "CamelotWheel.h"

/**
    Analizza un flusso audio in ingresso e stima la tonalità in tempo reale,
    con un approccio pensato per la STABILITÀ del risultato (non solo per la
    correttezza istantanea):

      1. Finestra di analisi ampia (8192 campioni) con overlap del 50%,
         per una risoluzione in frequenza sufficiente a distinguere bene
         anche le note basse.
      2. Estrazione dei picchi spettrali (non del semplice spettro grezzo),
         con interpolazione parabolica per una stima di frequenza più precisa
         e una diffusione "triangolare" del contributo tra i due pitch class
         più vicini, invece di un arrotondamento secco: riduce il rumore di
         quantizzazione tipico dei key-finder semplici.
      3. Una seconda chroma calcolata SOLO sulle frequenze basse (60-300 Hz
         circa) viene sommata con peso maggiore: nella musica prodotta il
         basso è uno degli indizi più affidabili della tonica.
      4. Cancello di silenzio: i frame troppo silenziosi non vengono usati,
         per non "sporcare" la stima con rumore di fondo.
      5. Le 24 ipotesi di tonalità (12 maggiori + 12 minori, correlazione di
         Pearson con i profili di Krumhansl-Schmuckler) vengono accumulate
         nel tempo con una media mobile esponenziale LENTA: il risultato
         mostrato è quindi il "voto" di diversi secondi di audio, non di un
         singolo istante.
      6. Isteresi: la tonalità mostrata cambia solo se una nuova ipotesi
         resta stabilmente in testa per un certo numero di frame consecutivi,
         evitando che il risultato "sfarfalli" tra due tonalità vicine.
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
        float confidence = 0.0f;       // 0..1, margine tra la 1a e la 2a ipotesi
        float inputLevelDb = -100.0f;  // per il VU meter
        CamelotWheel::KeyInfo keyInfo;
        bool valid = false;
    };

    Result getCurrentResult() const;

private:
    static constexpr int fftOrder = 13;                 // 2^13 = 8192 campioni
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 2;          // overlap del 50%

    void processFrame();
    void computeChromaFromMagnitudes(std::array<float, 12>& outChroma,
                                      double minFreqHz, double maxFreqHz) const;
    void correlateAndAccumulate(const std::array<float, 12>& chroma);

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize,
                                                  juce::dsp::WindowingFunction<float>::hann };

    // Buffer circolare: contiene sempre gli ultimi fftSize campioni ricevuti
    std::vector<float> ring;
    int ringWritePos = 0;
    int samplesSinceLastAnalysis = 0;

    std::vector<float> fftData;      // finestra estratta in ordine cronologico + lavoro FFT
    std::vector<float> magnitudes;   // spettro di magnitudine dell'ultimo frame

    double sampleRate = 44100.0;

    // Profili tonali di Krumhansl-Schmuckler
    static const std::array<float, 12> majorProfile;
    static const std::array<float, 12> minorProfile;

    // Punteggi di correlazione per le 24 ipotesi (12 maggiori + 12 minori),
    // integrati nel tempo con una media mobile esponenziale lenta.
    std::array<float, 24> smoothedScores {};
    bool scoresInitialised = false;

    // Isteresi sul cambio di tonalità mostrata
    int displayedIndex = -1;             // 0..11 = maggiori, 12..23 = minori
    int pendingIndex = -1;
    int pendingStreak = 0;
    static constexpr int streakToSwitch = 6; // ~6 frame di analisi (con hop 8192/2 a 44.1kHz ~ 0,56s totali) prima di cambiare

    float runningLevel = 0.0f;

    std::atomic<int> currentPitchClass { 0 };
    std::atomic<bool> currentIsMinor { false };
    std::atomic<float> currentConfidence { 0.0f };
    std::atomic<float> currentLevelDb { -100.0f };
    std::atomic<bool> hasValidResult { false };
};
