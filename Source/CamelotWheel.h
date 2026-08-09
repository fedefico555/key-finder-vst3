#pragma once
#include <JuceHeader.h>

/**
    Gestisce la mappatura tra tonalità (pitch class + modo maggiore/minore)
    e notazione Camelot (es. "8A", "5B"), usata dai DJ per il mixing armonico.
*/
class CamelotWheel
{
public:
    struct KeyInfo
    {
        int pitchClass = 0;      // 0 = C, 1 = C#/Db, ... 11 = B
        bool isMinor = false;
        juce::String camelotCode;   // es. "8A"
        juce::String classicalName; // es. "A minor"
    };

    // pitchClass: 0=C, 1=C#, 2=D, ... 11=B  (secondo convenzione MIDI mod 12)
    static KeyInfo getKeyInfo(int pitchClass, bool isMinor);

    // Numero di posizione sulla ruota (1-12) per un dato pitchClass/modo
    static int getCamelotNumber(int pitchClass, bool isMinor);

    // Ritorna true se due codici Camelot sono "compatibili" per il mixing
    // (stesso numero, numero adiacente, o stesso numero lettera diversa)
    static bool areCompatible(const juce::String& codeA, const juce::String& codeB);

    static const juce::StringArray& getNoteNames();
};
