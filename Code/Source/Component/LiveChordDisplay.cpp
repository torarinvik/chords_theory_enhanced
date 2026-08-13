#include "Component/LiveChordDisplay.h"

#include <algorithm>
#include <cstdint>

#include "AppLocalisation.h"
#include "Theory/ChordDetector.h"

namespace component
{

namespace
{
    // Stable identity for held harmony (pitch-class mask + bass PC).
    std::uint32_t harmonyId(const std::vector<int>& heldNotes)
    {
        if (heldNotes.empty())
            return 0;
        std::uint16_t mask = 0;
        int bass = 128;
        for (const int n : heldNotes)
        {
            if (n < 0 || n > 127)
                continue;
            mask = static_cast<std::uint16_t>(mask | (1u << (((n % 12) + 12) % 12)));
            bass = std::min(bass, n);
        }
        if (bass > 127)
            return 0;
        return (static_cast<std::uint32_t>(mask) << 8)
            | static_cast<std::uint32_t>(((bass % 12) + 12) % 12);
    }
}

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

void LiveChordDisplay::applyDetection(const theory::ChordDetection& detection)
{
    const auto nextName = detection.matched ? detection.name : std::string {};
    const auto nextHas = detection.matched && !nextName.empty();
    std::string nextHint;
    if (nextHas && !detection.qualityLabel.empty()
        && detection.qualityLabel != "note"
        && detection.qualityLabel != "dyad"
        && detection.qualityLabel != "cluster"
        && detection.qualityLabel != "maj"
        && detection.qualityLabel != "catalogue")
    {
        nextHint = detection.qualityLabel;
    }

    if (nextHas == _hasChord && nextName == _displayedName && nextHint == _qualityHint
        && detection.romanNumeral == _romanHint && detection.alternateName == _alternateHint
        && std::abs(detection.confidence - _confidence) < 0.02f)
        return;

    _hasChord = nextHas;
    _displayedName = nextName;
    _qualityHint = std::move(nextHint);
    _romanHint = detection.romanNumeral;
    _alternateHint = detection.alternateName;
    _confidence = detection.confidence;
    _pendingName.clear();
    _pendingFrames = 0;
    repaint();
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
            _pendingName.clear();
            _pendingFrames = 0;
            _lastHarmonyId = 0;
            repaint();
        }
        return;
    }

    const auto held = _tracker->getHeldNotes();
    const auto id = harmonyId(held);
    const auto gen = _tracker->getGeneration();

    // Skip work when neither MIDI generation nor the PC/bass identity changed
    // (re-triggers of the same notes still bump generation — ignore those).
    if (!force && gen == _lastGeneration && id == _lastHarmonyId)
        return;

    // Same harmony identity: only generation noise (re-attack). Keep display stable.
    if (!force && id == _lastHarmonyId && id != 0)
    {
        _lastGeneration = gen;
        return;
    }

    _lastGeneration = gen;
    _lastHarmonyId = id;

    const auto detection = theory::ChordDetector::detectFromMidiNotes(held, _spellKey, _scale);
    const auto nextName = detection.matched ? detection.name : std::string {};

    // Debounce name flips on dense / ambiguous voicings: require two consecutive
    // identical new names before committing (unless clearing or forced).
    if (!force && _hasChord && !nextName.empty() && nextName != _displayedName)
    {
        // Immediate accept when confidence is clearly higher.
        if (detection.confidence >= _confidence + 0.12f || detection.confidence >= 0.85f)
        {
            applyDetection(detection);
            return;
        }

        if (nextName == _pendingName)
        {
            ++_pendingFrames;
            if (_pendingFrames >= 2)
                applyDetection(detection);
            return;
        }

        _pendingName = nextName;
        _pendingFrames = 1;
        // Keep showing the previous solid name while we wait for confirmation.
        return;
    }

    applyDetection(detection);
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

        // Slim confidence meter along the bottom edge of the pill.
        if (_confidence > 0.05f && bounds.getWidth() > 20.f)
        {
            auto meter = bounds.removeFromBottom(2.5f).reduced(6.f, 0.f);
            g.setColour(juce::Colours::black.withAlpha(0.2f));
            g.fillRoundedRectangle(meter, 1.f);
            auto fill = meter.withWidth(meter.getWidth() * _confidence);
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(0.75f));
            g.fillRoundedRectangle(fill, 1.f);
            bounds = getLocalBounds().toFloat().reduced(4.f, 2.f).withTrimmedBottom(3.f);
        }
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
