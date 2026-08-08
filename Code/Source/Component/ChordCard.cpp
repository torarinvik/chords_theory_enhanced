#include "Component/ChordCard.h"

#include <algorithm>
#include <unordered_map>

#include "AppLocalisation.h"

namespace component
{

namespace
{
    constexpr float kDragStartThreshold = 6.f;
    constexpr int kVoicingButtonSize = 18;

    std::string baseSymbol(const std::string& symbol)
    {
        const auto slash = symbol.find('/');
        return slash == std::string::npos ? symbol : symbol.substr(0, slash);
    }

    // Root-position chords stay in popularity order; each one's inversions follow it immediately
    // (C, C/E, C/G, Cmaj7, …) so slash voicings are visible without scrolling past every extension.
    std::vector<theory::Chord> orderVoicingsForDisplay(std::vector<theory::Chord> voicings)
    {
        const auto total = voicings.size();
        std::vector<theory::Chord> roots;
        std::unordered_map<std::string, std::vector<theory::Chord>> inversionsByBase;
        roots.reserve(total);

        for (auto& chord : voicings)
        {
            if (chord.symbol.find('/') != std::string::npos)
                inversionsByBase[baseSymbol(chord.symbol)].push_back(std::move(chord));
            else
                roots.push_back(std::move(chord));
        }

        std::vector<theory::Chord> ordered;
        ordered.reserve(total);

        for (auto& root : roots)
        {
            const auto base = root.symbol;
            ordered.push_back(std::move(root));

            if (const auto it = inversionsByBase.find(base); it != inversionsByBase.end())
            {
                for (auto& inversion : it->second)
                    ordered.push_back(std::move(inversion));
                inversionsByBase.erase(it);
            }
        }

        // Slash chords whose base wasn't present as a root-position entry (unexpected).
        for (auto& [_, inversions] : inversionsByBase)
            for (auto& inversion : inversions)
                ordered.push_back(std::move(inversion));

        return ordered;
    }
}

ChordCard::ChordCard(const std::string& identifier, theory::Degree degree):
    Component(identifier),
    _degree(degree),
    _degreeLabel(identifier + "-degree", "", theory::getDegreeLabel(degree)),
    _chordNameLabel(identifier + "-name", "", ""),
    _voicingButton(identifier + "-voicing", nui::Icons::getArrowDown())
{
    _degreeLabel.setFontSize(nui::Theme::SMALL);
    _degreeLabel.setColor(nui::Theme::ThemeColor::DISABLED);
    _degreeLabel.setJustificationType(juce::Justification::centred);
    _degreeLabel.setInterceptsMouseClicks(false, false); // decorative - clicks must reach the card

    _chordNameLabel.setFontSize(nui::Theme::LABEL);
    _chordNameLabel.setFontWeight(nui::Theme::FontWeight::MEDIUM);
    _chordNameLabel.setJustificationType(juce::Justification::centred);
    _chordNameLabel.setInterceptsMouseClicks(false, false); // decorative - clicks must reach the card

    _voicingButton.setIconSize(12.f);
    _voicingButton.addOnClickListener(this);
    _voicingButton.setHelpText(juce::translate("chord_card_voicing_button_tooltip").toStdString());
    addChildComponent(_voicingButton); // visible only when 2+ voicings are available

    displayBorder(nui::Theme::ThemeColor::ACCENT, 1.f, nui::Theme::getBorderRadius());
    displayBackground(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(.2f), nui::Theme::getBorderRadius());

    setTooltip(juce::translate("chord_card_tooltip").toStdString());
    setTooltipEnabled(true);

    _layout.setGap(2.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 2 }, { 1 });
    _layout.addComponent(_degreeLabel, 0, 0, 1, 1);
    _layout.addComponent(_chordNameLabel, 1, 0, 1, 1);

    setPadding(4.f);

    nui::Theme::getChangeBroadcaster().addChangeListener(this);
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
}

ChordCard::~ChordCard()
{
    nui::Theme::getChangeBroadcaster().removeChangeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
    _voicingButton.removeListener(this);
}

void ChordCard::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ChordCard::resized()
{
    Component::resized();

    _layout.resized();

    // Float the voicing affordance in the top-right so labels stay centred and card-body clicks
    // (preview / drag) still hit the card outside the button bounds.
    _voicingButton.setBounds(getWidth() - kVoicingButtonSize - 2, 2, kVoicingButtonSize, kVoicingButtonSize);
    _voicingButton.toFront(false);
}

void ChordCard::setChord(const theory::Chord& chord)
{
    _chord = chord;
    refreshLabels();
}

void ChordCard::refreshLabels()
{
    _chordNameLabel.setText(_chord.readableName);
    repaint();
}

void ChordCard::setAvailableVoicings(std::vector<theory::Chord> voicings)
{
    _availableVoicings = orderVoicingsForDisplay(std::move(voicings));
    updateVoicingButtonVisibility();
}

void ChordCard::updateVoicingButtonVisibility()
{
    // No point offering a picker when there's nothing to switch to.
    _voicingButton.setVisible(_availableVoicings.size() > 1);
}

void ChordCard::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ChordCard::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ChordCard::mouseDown(const juce::MouseEvent&)
{
    _dragGestureStarted = false;
}

void ChordCard::mouseDrag(const juce::MouseEvent& event)
{
    if (_dragGestureStarted)
        return;

    if (static_cast<float>(event.getDistanceFromDragStart()) < kDragStartThreshold)
        return;

    _dragGestureStarted = true;

    for (auto* listener : _listeners)
        listener->onChordDragStarted(_degree, _chord);
}

void ChordCard::mouseUp(const juce::MouseEvent& event)
{
    if (_dragGestureStarted)
    {
        _dragGestureStarted = false;
        return;
    }

    // Card body: preview only. Voicing banner is opened exclusively via _voicingButton.
    for (auto* listener : _listeners)
        listener->onChordPreviewRequested(_degree, _chord);

    juce::ignoreUnused(event);
}

void ChordCard::onButtonClick(const std::string& componentID)
{
    if (componentID != _voicingButton.getComponentID())
        return;

    requestVoicingSelector();
}

void ChordCard::requestVoicingSelector()
{
    if (_availableVoicings.size() <= 1)
        return;

    for (auto* listener : _listeners)
        listener->onVoicingSelectorRequested(_degree, _availableVoicings, _chord.symbol);
}

void ChordCard::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        repaint();
        return;
    }

    if (source == &AppLocalisation::getChangeBroadcaster())
    {
        setTooltip(juce::translate("chord_card_tooltip").toStdString());
        _voicingButton.setHelpText(juce::translate("chord_card_voicing_button_tooltip").toStdString());
        repaint();
    }
}

}
