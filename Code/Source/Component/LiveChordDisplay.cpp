#include "Component/LiveChordDisplay.h"

#include "AppLocalisation.h"
#include "Theory/ChordDetector.h"

namespace component
{

LiveChordDisplay::LiveChordDisplay(const std::string& identifier,
                                   audio::InputMidiNoteTracker* inputMidiNoteTracker):
    Component(identifier),
    _tracker(inputMidiNoteTracker)
{
    setInterceptsMouseClicks(false, false);
    syncEmptyLabel();
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    if (_tracker != nullptr)
        startTimerHz(30);
}

LiveChordDisplay::~LiveChordDisplay()
{
    stopTimer();
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void LiveChordDisplay::setSpellKey(theory::Key key)
{
    if (_spellKey == key)
        return;
    _spellKey = key;
    // Re-spell currently held notes without waiting for a MIDI generation bump.
    refreshFromTracker(true);
}

void LiveChordDisplay::syncEmptyLabel()
{
    _emptyLabel = juce::translate("live_chord_display_empty").toStdString();
}

void LiveChordDisplay::timerCallback()
{
    refreshFromTracker(false);
}

void LiveChordDisplay::refreshFromTracker(bool force)
{
    if (_tracker == nullptr)
    {
        if (_hasChord || !_displayedName.empty())
        {
            _hasChord = false;
            _displayedName.clear();
            repaint();
        }
        return;
    }

    const auto gen = _tracker->getGeneration();
    if (!force && gen == _lastGeneration)
        return;
    _lastGeneration = gen;

    const auto held = _tracker->getHeldNotes();
    const auto detection = theory::ChordDetector::detectFromMidiNotes(held, _spellKey);

    const auto nextName = detection.matched ? detection.name : std::string {};
    const auto nextHas = detection.matched && !nextName.empty();
    // Hint only when the symbol is non-trivial (skip plain major triad "").
    std::string nextHint;
    if (nextHas && !detection.qualityLabel.empty()
        && detection.qualityLabel != "note"
        && detection.qualityLabel != "dyad"
        && detection.qualityLabel != "cluster"
        && detection.qualityLabel != "maj")
    {
        nextHint = detection.qualityLabel;
    }

    if (nextHas == _hasChord && nextName == _displayedName && nextHint == _qualityHint)
        return;

    _hasChord = nextHas;
    _displayedName = nextName;
    _qualityHint = std::move(nextHint);
    repaint();
}

void LiveChordDisplay::paint(juce::Graphics& g)
{
    Component::paint(g);

    auto bounds = getLocalBounds().toFloat().reduced(4.f, 2.f);
    if (bounds.getWidth() < 8.f || bounds.getHeight() < 8.f)
        return;

    const bool showChord = _hasChord && !_displayedName.empty();
    const auto text = showChord ? juce::String(_displayedName) : juce::String(_emptyLabel);

    // Soft pill behind an active detection so it reads as a live readout.
    if (showChord)
    {
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(0.14f));
        g.fillRoundedRectangle(bounds, 6.f);
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(0.35f));
        g.drawRoundedRectangle(bounds, 6.f, 1.f);
    }

    if (showChord && !_qualityHint.empty() && bounds.getHeight() >= 28.f)
    {
        auto nameArea = bounds;
        auto hintArea = nameArea.removeFromRight(juce::jmin(72.f, bounds.getWidth() * 0.35f));
        g.setFont(nui::Theme::newFont(nui::Theme::BOLD, nui::Theme::HEADING));
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
        g.drawText(text, nameArea.reduced(8.f, 0.f), juce::Justification::centred, true);

        g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(0.9f));
        g.drawText(juce::String(_qualityHint), hintArea.reduced(4.f, 0.f),
            juce::Justification::centredRight, true);
    }
    else
    {
        g.setFont(nui::Theme::newFont(nui::Theme::BOLD, showChord ? nui::Theme::HEADING : nui::Theme::SMALL));
        g.setColour(showChord
            ? nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce()
            : nui::Theme::newColor(nui::Theme::ThemeColor::DISABLED).asJuce());
        g.drawText(text, bounds.reduced(8.f, 0.f), juce::Justification::centred, true);
    }
}

void LiveChordDisplay::resized()
{
    Component::resized();
}

void LiveChordDisplay::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &AppLocalisation::getChangeBroadcaster())
    {
        syncEmptyLabel();
        repaint();
        return;
    }

    if (source == &nui::Theme::getChangeBroadcaster())
        repaint();
}

}
