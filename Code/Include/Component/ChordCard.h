#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace component
{

// One clickable/draggable chord card: shows a scale degree's currently selected voicing. A single
// click previews the chord (see Listener::onChordPreviewRequested); the small voicing button opens
// the inline voicing selector (see Listener::onVoicingSelectorRequested and
// component::VoicingSelector) - this card never opens anything itself, it just reports the
// gesture and its own current data; drag reports the gesture to its Listener, which owns writing
// the temp MIDI file and performing the actual OS-level drag (see MidiExporter / AppLayout) -
// this widget only knows about chord data and UI, never about MIDI files or audio.
class ChordCard : public nui::Component,
                   public nelement::SVGButton::OnClickListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // The user picked a different voicing via the voicing selector for this card's degree.
        virtual void onChordChanged(theory::Degree degree, const theory::Chord& newChord) = 0;

        // The user started dragging this card - past the minimum-distance threshold, so this
        // never fires for a plain click.
        virtual void onChordDragStarted(theory::Degree degree, const theory::Chord& chord) = 0;

        // The user clicked (not dragged) the card body - preview only; does not open the picker.
        virtual void onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord) = 0;

        // The user clicked this card's voicing button - the owner shows/refreshes the inline
        // voicing selector for this degree's availableVoicings/currentSymbol. Never fires if this
        // card has fewer than two voicings to choose between.
        virtual void onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol) = 0;
    };

    ChordCard(const std::string& identifier, theory::Degree degree);
    ~ChordCard() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setChord(const theory::Chord& chord);
    [[nodiscard]] const theory::Chord& getChord() const { return _chord; }
    [[nodiscard]] theory::Degree getDegree() const { return _degree; }

    // Voicings for the selector: root-position forms keep popularity order; each form's
    // inversions (slash chords like "C/E") are placed immediately after it so they aren't buried
    // at the end of a long extension list. The voicing button is shown only when there are 2+.
    void setAvailableVoicings(std::vector<theory::Chord> voicings);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void onButtonClick(const std::string& componentID) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refreshLabels();
    void updateVoicingButtonVisibility();
    void requestVoicingSelector();

    theory::Degree _degree;
    theory::Chord _chord;
    std::vector<theory::Chord> _availableVoicings;

    nelement::Text _degreeLabel;
    nelement::Text _chordNameLabel;
    nelement::SVGButton _voicingButton;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    bool _dragGestureStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordCard)
};

}
