#pragma once

#include <optional>
#include <unordered_map>

#include <nierika_dsp/nierika_dsp.h>

#include "PluginProcessor.h"
#include "Component/ChordDegreeBrowser.h"
#include "Component/KeyScaleSelector.h"
#include "Component/NextChordPanel.h"
#include "Component/ProgressionEditor.h"
#include "Component/ScaleSuggestionPanel.h"
#include "Component/SettingsWindow.h"
#include "Component/SynthEditor.h"
#include "Component/VoicingSelector.h"
#include "Theory/Chord.h"
#include "Theory/Degree.h"
#include "Theory/ProgressionSlot.h"

class AppLayout final : public nlayout::AppLayout,
                         public nelement::SVGButton::OnClickListener,
                         public component::KeyScaleSelector::Listener,
                         public component::ChordDegreeBrowser::Listener,
                         public component::ProgressionEditor::Listener,
                         public component::VoicingSelector::Listener,
                         public nui::Section::OnPanelChangedListener
{
public:
    AppLayout(ndsp::ParameterManager& parameterManager, PluginAudioProcessor& audioProcessor);
    ~AppLayout() override;

    void resized() override;

    void onButtonClick(const std::string& componentID) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void onKeyScaleChanged(theory::Key key, theory::Scale scale) override;
    void onSearchChanged(const std::string& query,
                         component::KeyScaleSelector::SearchMode mode,
                         component::KeyScaleSelector::SearchScope scope) override;
    void onChordChanged(theory::Degree degree, const theory::Chord& newChord) override;
    void onChordDragStarted(theory::Degree degree, const theory::Chord& chord) override;
    void onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord) override;
    void onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol) override;
    void onVoicingSelectorClosed() override;

    void onChordFileDropped(double startBeat, const juce::String& filePath) override;
    void onProgressionDragStarted() override;
    void onContentChanged() override;
    void onChordBlockPreviewRequested(const std::vector<int>& midiNotes) override;

    // _mainSection's own panel-switch mechanism (GridLayout::setVisible()) unconditionally shows
    // every component registered in a panel the moment it becomes active again - including
    // _voicingSelector, which has its own independent open/closed state unrelated to which tab is
    // showing. Re-asserts that real state whenever the Chords tab becomes active again, so closing
    // the voicing selector then switching to Synth and back doesn't silently reopen it.
    void onPanelChanged(const std::string& newPanelID) override;

    // Shared by both onChordPreviewRequested overrides above.
    void previewChord(const theory::Chord& chord);

    // Auditions on the internal synth and emits host MIDI for DAW routing.
    // Picks the smoothest inversion vs. the last previewed / current chord when available.
    void playChordToSynthAndHost(const theory::Chord& chord);

    // Reference harmony for smooth-inversion preview (last played, else next-chord current).
    [[nodiscard]] theory::Chord previewReferenceChord() const;

    // Updates the next-chord panel from a newly chosen "current" chord (and rebuilds sequence memory).
    // When pinCurrent is true, progression edits will not auto-retarget current to the last block.
    void setCurrentChordForSuggestions(const theory::Chord& chord, bool pinCurrent = true);

    // Follows the progression timeline: current = last chord block, history = everything before it.
    // Clears any browser pin. No-op if the progression is empty.
    void syncNextChordFromProgressionTail();

    // Rebuilds phrase-memory from the progression sequencer without changing the current chord.
    void refreshNextChordSequenceContext();

    // Shows next-chord vs scale-suggestion panels from the header Chord/Scale search mode.
    void updateSuggestionPanelVisibility();

    // Pushes current key/scale/chord + search query/scope into the scale-suggestion panel.
    void refreshScaleSuggestions();

    // Feeds progression timeline + last auditioned chords into the live chord expert.
    void refreshLiveChordExpertContext();

    // Re-derives the voicing selector's arrow-target x from the currently open degree's card -
    // called after the layout changes for any reason (new degree opened, or a resize while
    // already open). No-op while the selector is closed.
    void updateVoicingSelectorArrow();

    void setVoicingVisibility(bool isVisible);

    // Rebuilds the "ChordsTheoryState" child of _parameterManager.getState().state from the
    // current UI state - called after any change so the state is always current even if the
    // editor later closes (getStateInformation serializes the whole tree, including this child,
    // regardless of whether an editor exists at save time).
    void syncStateToValueTree();

    // Reads the "ChordsTheoryState" child back out (if present - absent on a fresh/never-saved
    // instance) and applies it to the UI. Called once, at construction.
    void restoreStateFromValueTree();

    PluginAudioProcessor& _audioProcessor;

    nelement::SVGButton _settings;
    component::KeyScaleSelector _keyScaleSelector;
    component::ChordDegreeBrowser _chordBrowser;
    component::NextChordPanel _nextChordPanel { "next-chord-panel" };
    component::ScaleSuggestionPanel _scaleSuggestionPanel { "scale-suggestion-panel" };
    component::VoicingSelector _voicingSelector { "voicing-selector" };
    component::ProgressionEditor _progressionEditor;
    component::SynthEditor _synthEditor;

    // Owns the "Chords"/"Synth" tab switcher - everything above except _settings/_keyScaleSelector
    // (row 0, always visible) lives inside one of its two panels. A *new*, nested Section, not a
    // reuse of this class's own inherited nlayout::AppLayout/Section grid (which is what row 0
    // itself lives in) - see AppLayout.cpp for why tabbing the root directly would also hide row 0.
    nui::Section _mainSection;

    nlayout::WindowsManager _windowsManager;

    // Carries the exact Chord (and a ProgressionSlot for lane/preset metadata) across an OS-level
    // file drag so MidiEditor drops don't need to re-resolve against the browser — required for
    // chromatic next-triad suggestions that have no degree in the current key/scale.
    struct InFlightChordDrag
    {
        theory::Chord chord;
        theory::ProgressionSlot sourceSlot;
    };

    // tempFilePath -> chord payload, populated just before performExternalDragDropOfFiles,
    // consulted by onChordFileDropped() for internal sequencer drops.
    std::unordered_map<juce::String, InFlightChordDrag> _inFlightChordDrags;

    // Which degree the voicing selector is currently showing, if open - used to re-derive the
    // arrow-target x on resize and to know what to clear when the key/scale changes underneath it.
    std::optional<theory::Degree> _openVoicingDegree;

    // When false (default after progression edits), next-chord current tracks the last block on
    // the piano-roll. When true, the user pinned a current chord from the browser / next list.
    bool _nextChordCurrentPinned = false;

    // Last auditioned harmony (actual inversion played) — next play chooses the smoothest
    // inversion relative to this when set.
    std::optional<theory::Chord> _lastPreviewChord;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppLayout)
};
