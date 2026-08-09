#include "PluginProcessor.h"
#include "PluginEditor.h"

KeyFinderAudioProcessorEditor::KeyFinderAudioProcessorEditor(KeyFinderAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(520, 660);
    setResizable(true, true);
    setResizeLimits(360, 460, 900, 1100);
    startTimerHz(30);
}

KeyFinderAudioProcessorEditor::~KeyFinderAudioProcessorEditor()
{
    stopTimer();
}

void KeyFinderAudioProcessorEditor::timerCallback()
{
    lastResult = audioProcessor.getKeyResult();

    const float targetNorm = juce::jlimit(0.0f, 1.0f, (lastResult.inputLevelDb + 60.0f) / 60.0f);
    displayedLevelNorm += (targetNorm - displayedLevelNorm) * 0.35f;

    repaint();
}

void KeyFinderAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(18, 18, 22));

    auto bounds = getLocalBounds().toFloat().reduced(16.0f);

    auto titleArea = bounds.removeFromTop(36.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("KEY FINDER", titleArea, juce::Justification::centred);

    bounds.removeFromTop(8.0f);

    auto labelArea = bounds.removeFromBottom(90.0f);
    auto vuArea = bounds.removeFromBottom(46.0f);
    bounds.removeFromBottom(8.0f);

    drawCamelotWheel(g, bounds);
    drawVuMeter(g, vuArea);
    drawKeyLabel(g, labelArea);
}

void KeyFinderAudioProcessorEditor::resized()
{
    // Il layout viene ricalcolato interamente in paint(), non servono
    // componenti figli da riposizionare.
}

void KeyFinderAudioProcessorEditor::drawCamelotWheel(juce::Graphics& g, juce::Rectangle<float> area)
{
    const float diameter = juce::jmin(area.getWidth(), area.getHeight());
    juce::Rectangle<float> wheelArea(0, 0, diameter, diameter);
    wheelArea.setCentre(area.getCentreX(), area.getCentreY());

    const auto centre = wheelArea.getCentre();
    const float outerRadius = diameter * 0.5f;
    const float midRadius = outerRadius * 0.68f;
    const float innerRadius = outerRadius * 0.36f;

    const int detectedNumber = CamelotWheel::getCamelotNumber(lastResult.pitchClass, lastResult.isMinor);
    const bool detectedIsMinor = lastResult.isMinor;

    for (int number = 1; number <= 12; ++number)
    {
        const float startAngle = juce::MathConstants<float>::twoPi * (float) (number - 1) / 12.0f
                                  - juce::MathConstants<float>::halfPi - juce::MathConstants<float>::pi / 12.0f;
        const float endAngle = startAngle + juce::MathConstants<float>::twoPi / 12.0f;

        const float hue = (float) (number - 1) / 12.0f;

        // --- Anello esterno: maggiori (B) ---
        {
            juce::Path segment;
            segment.addPieSegment(centre.x - outerRadius, centre.y - outerRadius,
                                   outerRadius * 2.0f, outerRadius * 2.0f,
                                   startAngle, endAngle,
                                   midRadius / outerRadius);

            const bool isSelected = lastResult.valid && !detectedIsMinor && number == detectedNumber;
            auto colour = juce::Colour::fromHSV(hue, 0.55f, isSelected ? 0.95f : 0.55f, 1.0f);
            g.setColour(colour);
            g.fillPath(segment);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.strokePath(segment, juce::PathStrokeType(1.0f));

            if (isSelected)
            {
                g.setColour(juce::Colours::white);
                g.strokePath(segment, juce::PathStrokeType(3.0f));
            }

            const float labelRadius = (outerRadius + midRadius) * 0.5f;
            const float midAngle = (startAngle + endAngle) * 0.5f;
            juce::Point<float> labelPos(centre.x + labelRadius * std::cos(midAngle),
                                         centre.y + labelRadius * std::sin(midAngle));
            g.setColour(isSelected ? juce::Colours::black : juce::Colours::white.withAlpha(0.9f));
            g.setFont(juce::Font(15.0f, juce::Font::bold));
            g.drawText(juce::String(number) + "B", juce::Rectangle<float>(44, 20).withCentre(labelPos),
                       juce::Justification::centred);
        }

        // --- Anello interno: minori (A) ---
        {
            juce::Path segment;
            segment.addPieSegment(centre.x - midRadius, centre.y - midRadius,
                                   midRadius * 2.0f, midRadius * 2.0f,
                                   startAngle, endAngle,
                                   innerRadius / midRadius);

            const bool isSelected = lastResult.valid && detectedIsMinor && number == detectedNumber;
            auto colour = juce::Colour::fromHSV(hue, 0.4f, isSelected ? 0.95f : 0.35f, 1.0f);
            g.setColour(colour);
            g.fillPath(segment);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.strokePath(segment, juce::PathStrokeType(1.0f));

            if (isSelected)
            {
                g.setColour(juce::Colours::white);
                g.strokePath(segment, juce::PathStrokeType(3.0f));
            }

            const float labelRadius = (midRadius + innerRadius) * 0.5f;
            const float midAngle = (startAngle + endAngle) * 0.5f;
            juce::Point<float> labelPos(centre.x + labelRadius * std::cos(midAngle),
                                         centre.y + labelRadius * std::sin(midAngle));
            g.setColour(isSelected ? juce::Colours::black : juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(juce::String(number) + "A", juce::Rectangle<float>(40, 18).withCentre(labelPos),
                       juce::Justification::centred);
        }
    }

    // Cerchio centrale
    g.setColour(juce::Colour::fromRGB(30, 30, 36));
    g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
}

void KeyFinderAudioProcessorEditor::drawVuMeter(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::Font(12.0f));
    auto labelArea = area.removeFromLeft(60.0f);
    g.drawText("INPUT", labelArea, juce::Justification::centredLeft);

    area.reduce(0.0f, 6.0f);

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(area, 4.0f);

    auto fillArea = area.reduced(3.0f);
    fillArea.setWidth(fillArea.getWidth() * displayedLevelNorm);

    juce::ColourGradient gradient(juce::Colours::limegreen, area.getX(), 0.0f,
                                   juce::Colours::red, area.getRight(), 0.0f, false);
    gradient.addColour(0.75, juce::Colours::yellow);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(fillArea, 3.0f);

    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
}

void KeyFinderAudioProcessorEditor::drawKeyLabel(juce::Graphics& g, juce::Rectangle<float> area)
{
    if (!lastResult.valid)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::Font(16.0f));
        g.drawText("In ascolto...", area, juce::Justification::centred);
        return;
    }

    auto keyNameArea = area.removeFromTop(area.getHeight() * 0.55f);
    auto confidenceArea = area;

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(30.0f, juce::Font::bold));
    const juce::String mainText = lastResult.keyInfo.camelotCode + "   ("
                                   + lastResult.keyInfo.classicalName + ")";
    g.drawText(mainText, keyNameArea, juce::Justification::centred);

    const int confidencePercent = juce::roundToInt(lastResult.confidence * 100.0f);

    auto barArea = confidenceArea.reduced(confidenceArea.getWidth() * 0.15f, 0.0f);
    auto textArea = barArea.removeFromLeft(0.0f); // segnaposto, testo sotto la barra

    auto barRow = confidenceArea.withSizeKeepingCentre(confidenceArea.getWidth() * 0.7f, 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(barRow, 3.0f);

    auto barFill = barRow.reduced(2.0f);
    barFill.setWidth(barFill.getWidth() * (lastResult.confidence));
    g.setColour(juce::Colour::fromHSV(0.33f * lastResult.confidence, 0.7f, 0.9f, 1.0f));
    g.fillRoundedRectangle(barFill, 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::Font(12.0f));
    g.drawText("Confidenza: " + juce::String(confidencePercent) + "%",
               confidenceArea.translated(0.0f, 16.0f), juce::Justification::centred);
}
