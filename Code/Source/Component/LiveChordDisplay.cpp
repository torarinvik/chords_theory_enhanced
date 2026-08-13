#include "Component/LiveChordDisplay.h"

#include <algorithm>
#include <cstdint>

#include "AppLocalisation.h"
#include "AppSettings.h"

namespace component
{

namespace
{
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
    syncStyleFromSettings();
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    AppSettings::getChangeBroadcaster().addChangeListener(this);

    if (_tracker != nullptr)
        startTimerHz(30);
}

LiveChordDisplay::~LiveChordDisplay()
{
    stopTimer();
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
    AppSettings::getChangeBroadcaster().removeChangeListener(this);
}

void LiveChordDisplay::setSpellKey(theory::Key key)
{
    if (_context.key == key)
        return;
    _context.key = key;
    refreshFromTracker(true);
}

void LiveChordDisplay::setScale(theory::Scale scale)
{
    if (_context.scale == scale)
        return;
    _context.scale = scale;
    refreshFromTracker(true);
}

void LiveChordDisplay::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    if (_context.key == key && _context.scale == scale)
        return;
    _context.key = key;
    _context.scale = scale;
    refreshFromTracker(true);
}

void LiveChordDisplay::setExpertContext(theory::ChordExpertContext context)
{
    // Trust the caller's key/scale (AppLayout seeds session pickers, then overrides with any
    // scale attached to the active progression chord). Only style is forced from settings.
    context.style = AppSettings::getInstance().getChordNamingStyle();
    _context = std::move(context);
    refreshFromTracker(true);
}

void LiveChordDisplay::syncStyleFromSettings()
{
    _context.style = AppSettings::getInstance().getChordNamingStyle();
}

void LiveChordDisplay::syncEmptyLabel()
{
    _emptyLabel = juce::translate("live_chord_display_empty").toStdString();
}

void LiveChordDisplay::timerCallback()
{
    refreshFromTracker(false);
}

void LiveChordDisplay::applyResult(const theory::ChordExpertResult& result)
{
    const auto& det = result.detection;
    const auto nextName = det.matched ? det.name : std::string {};
    const auto nextHas = det.matched && !nextName.empty();

    std::string nextHint;
    if (nextHas && !det.qualityLabel.empty()
        && det.qualityLabel != "note"
        && det.qualityLabel != "dyad"
        && det.qualityLabel != "cluster"
        && det.qualityLabel != "maj"
        && det.qualityLabel != "catalogue"
        && det.qualityLabel != "rootless")
    {
        nextHint = det.qualityLabel;
    }

    std::string nextAlt = det.alternateName;
    if (nextAlt.empty() && !result.alternatives.empty())
        nextAlt = result.alternatives.front();

    if (nextHas == _hasChord && nextName == _displayedName && nextHint == _qualityHint
        && det.romanNumeral == _romanHint && nextAlt == _alternateHint
        && result.explanation == _explanation
        && std::abs(det.confidence - _confidence) < 0.02f)
        return;

    _hasChord = nextHas;
    _displayedName = nextName;
    _qualityHint = std::move(nextHint);
    _romanHint = det.romanNumeral;
    _alternateHint = nextAlt;
    _explanation = result.explanation;
    _confidence = det.confidence;
    _pendingName.clear();
    _pendingFrames = 0;

    // Full narrative on hover.
    if (_hasChord)
    {
        juce::String tip = _displayedName;
        if (!_romanHint.empty())
            tip << "  (" << _romanHint << ")";
        if (!_explanation.empty())
            tip << "\n" << _explanation;
        if (!_alternateHint.empty() && _alternateHint != _displayedName)
            tip << "\n" << juce::translate("live_chord_display_also") << " " << _alternateHint;
        setTooltip(tip.toStdString());
    }
    else
    {
        setTooltip({});
    }

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
            _explanation.clear();
            _pendingName.clear();
            _pendingFrames = 0;
            _lastHarmonyId = 0;
            setTooltip({});
            repaint();
        }
        return;
    }

    const auto held = _tracker->getHeldNotes();
    const auto id = harmonyId(held);
    const auto gen = _tracker->getGeneration();

    if (!force && gen == _lastGeneration && id == _lastHarmonyId)
        return;

    if (!force && id == _lastHarmonyId && id != 0)
    {
        _lastGeneration = gen;
        return;
    }

    _lastGeneration = gen;
    _lastHarmonyId = id;

    // Keep style in sync with settings (in case settings changed without force path).
    _context.style = AppSettings::getInstance().getChordNamingStyle();

    const auto expert = theory::ChordExpert::analyse(held, _context);
    const auto nextName = expert.detection.matched ? expert.detection.name : std::string {};

    if (!force && _hasChord && !nextName.empty() && nextName != _displayedName)
    {
        if (expert.detection.confidence >= _confidence + 0.12f
            || expert.detection.confidence >= 0.85f
            || expert.usedProgressionContext)
        {
            applyResult(expert);
            return;
        }

        if (nextName == _pendingName)
        {
            ++_pendingFrames;
            if (_pendingFrames >= 2)
                applyResult(expert);
            return;
        }

        _pendingName = nextName;
        _pendingFrames = 1;
        return;
    }

    applyResult(expert);
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

    if (!showChord)
    {
        g.setFont(nui::Theme::newFont(nui::Theme::BOLD, nui::Theme::SMALL));
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::DISABLED).asJuce());
        g.drawText(text, bounds.reduced(8.f, 0.f), juce::Justification::centred, true);
        return;
    }

    // Always paint absolute name + roman inline (e.g. "Am - vi") so the function is obvious.
    // Use ASCII separators only — the theme font mangles U+00B7 middle-dot into "Å".
    auto content = bounds.reduced(8.f, 0.f);
    g.setFont(nui::Theme::newFont(nui::Theme::BOLD, nui::Theme::HEADING));
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());

    if (!_romanHint.empty())
    {
        const auto nameW = content.getWidth() * 0.55f;
        auto nameArea = content.removeFromLeft(nameW);
        g.drawText(text, nameArea, juce::Justification::centredLeft, true);
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce());
        g.drawText("- " + juce::String(_romanHint), content, juce::Justification::centredLeft, true);
    }
    else
    {
        g.drawText(text, content, juce::Justification::centred, true);
    }
}

void LiveChordDisplay::resized()
{
    Component::resized();
}

void LiveChordDisplay::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &AppSettings::getChangeBroadcaster())
    {
        syncStyleFromSettings();
        refreshFromTracker(true);
        return;
    }

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
