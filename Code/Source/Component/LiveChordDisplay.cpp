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
    refreshFromTracker(true);
}

void LiveChordDisplay::setScale(theory::Scale scale)
{
    if (_scale == scale)
        return;
    _scale = scale;
    refreshFromTracker(true);
}

void LiveChordDisplay::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    if (_spellKey == key && _scale == scale)
        return;
    _spellKey = key;
    _scale = scale;
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
            _qualityHint.clear();
            _romanHint.clear();
            _alternateHint.clear();
            _holdFrames = 0;
            repaint();
        }
        return;
    }

    const auto gen = _tracker->getGeneration();
    if (!force && gen == _lastGeneration)
    {
        if (_holdFrames > 0)
        {
            --_holdFrames;
            if (_holdFrames == 0 && !_heldName.empty() && _heldName != _displayedName)
            {
                // Hold expired — already showing new name.
            }
        }
        return;
    }
    _lastGeneration = gen;

    const auto held = _tracker->getHeldNotes();
    const auto detection = theory::ChordDetector::detectFromMidiNotes(held, _spellKey, _scale);

    auto nextName = detection.matched ? detection.name : std::string {};
    const auto nextHas = detection.matched && !nextName.empty();
    std::string nextHint;
    if (nextHas && !detection.qualityLabel.empty()
        && detection.qualityLabel != "note"
        && detection.qualityLabel != "dyad"
        && detection.qualityLabel != "cluster"
        && detection.qualityLabel != "maj")
    {
        nextHint = detection.qualityLabel;
    }
    const auto nextRoman = detection.romanNumeral;
    const auto nextAlt = detection.alternateName;
    const auto nextConf = detection.confidence;

    // Hysteresis: if we already show a solid name and the new reading is shaky and different,
    // keep the old name for a few frames so the label does not flicker between twins (C6/Am7).
    if (!force && _hasChord && nextHas && nextName != _displayedName
        && _confidence >= 0.65f && nextConf < 0.55f)
    {
        _holdFrames = 4;
        _heldName = nextName;
        // Keep painting the previous name; still refresh soft hints if empty.
        return;
    }

    if (nextHas == _hasChord && nextName == _displayedName && nextHint == _qualityHint
        && nextRoman == _romanHint && nextAlt == _alternateHint
        && std::abs(nextConf - _confidence) < 0.02f)
        return;

    _hasChord = nextHas;
    _displayedName = nextName;
    _qualityHint = std::move(nextHint);
    _romanHint = nextRoman;
    _alternateHint = nextAlt;
    _confidence = nextConf;
    _holdFrames = 0;
    _heldName.clear();
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

    if (showChord)
    {
        const float alpha = juce::jlimit(0.08f, 0.22f, 0.08f + _confidence * 0.14f);
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(alpha));
        g.fillRoundedRectangle(bounds, 6.f);
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce()
            .withAlpha(0.25f + _confidence * 0.25f));
        g.drawRoundedRectangle(bounds, 6.f, 1.f);
    }

    if (showChord && bounds.getHeight() >= 26.f && bounds.getWidth() >= 120.f
        && (!_qualityHint.empty() || !_romanHint.empty() || !_alternateHint.empty()))
    {
        auto nameArea = bounds;
        auto side = nameArea.removeFromRight(juce::jmin(90.f, bounds.getWidth() * 0.38f));

        g.setFont(nui::Theme::newFont(nui::Theme::BOLD, nui::Theme::HEADING));
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
        g.drawText(text, nameArea.reduced(6.f, 0.f), juce::Justification::centred, true);

        auto top = side.removeFromTop(side.getHeight() * 0.55f);
        auto bottom = side;
        g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

        if (!_romanHint.empty())
        {
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce());
            g.drawText(juce::String(_romanHint), top.reduced(2.f, 0.f),
                juce::Justification::centredRight, true);
        }
        else if (!_qualityHint.empty())
        {
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(0.9f));
            g.drawText(juce::String(_qualityHint), top.reduced(2.f, 0.f),
                juce::Justification::centredRight, true);
        }

        if (!_alternateHint.empty() && _alternateHint != _displayedName)
        {
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::DISABLED).asJuce());
            g.drawText(juce::String(_alternateHint), bottom.reduced(2.f, 0.f),
                juce::Justification::centredRight, true);
        }
        else if (!_qualityHint.empty() && !_romanHint.empty())
        {
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::DISABLED).asJuce());
            g.drawText(juce::String(_qualityHint), bottom.reduced(2.f, 0.f),
                juce::Justification::centredRight, true);
        }
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
