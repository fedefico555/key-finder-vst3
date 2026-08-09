#include "CamelotWheel.h"

namespace
{
    // Indice = pitch class (0=C ... 11=B). Valore = numero posizione (1-12) sulla ruota Camelot.
    const int majorCamelotNumbers[12] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 };
    const int minorCamelotNumbers[12] = { 5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10 };
}

const juce::StringArray& CamelotWheel::getNoteNames()
{
    static const juce::StringArray names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return names;
}

int CamelotWheel::getCamelotNumber(int pitchClass, bool isMinor)
{
    pitchClass = ((pitchClass % 12) + 12) % 12;
    return isMinor ? minorCamelotNumbers[pitchClass] : majorCamelotNumbers[pitchClass];
}

CamelotWheel::KeyInfo CamelotWheel::getKeyInfo(int pitchClass, bool isMinor)
{
    KeyInfo info;
    info.pitchClass = ((pitchClass % 12) + 12) % 12;
    info.isMinor = isMinor;

    const int number = getCamelotNumber(info.pitchClass, isMinor);
    info.camelotCode = juce::String(number) + (isMinor ? "A" : "B");

    const auto& names = getNoteNames();
    info.classicalName = names[info.pitchClass] + (isMinor ? " minor" : " major");

    return info;
}

bool CamelotWheel::areCompatible(const juce::String& codeA, const juce::String& codeB)
{
    if (codeA.isEmpty() || codeB.isEmpty())
        return false;

    auto parse = [](const juce::String& code, int& number, juce::juce_wchar& letter)
    {
        number = code.retainCharacters("0123456789").getIntValue();
        auto letters = code.retainCharacters("ABab").toUpperCase();
        letter = letters.isNotEmpty() ? letters[0] : 'B';
    };

    int numA = 0, numB = 0;
    juce::juce_wchar letA = 'B', letB = 'B';
    parse(codeA, numA, letA);
    parse(codeB, numB, letB);

    if (numA == numB)
        return true; // stesso numero (relativo maggiore/minore o stessa tonalità)

    const int diff = std::abs(numA - numB);
    const int wrapped = juce::jmin(diff, 12 - diff);
    return wrapped == 1 && letA == letB; // adiacente sulla ruota, stesso modo
}
