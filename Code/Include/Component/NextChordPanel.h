#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordSequenceContext.h"
#include "Theory/Scale.h"

namespace component
{

// Ranked, scrollable list of next-chord suggestions (triads, sus, power, sevenths).
// Drama slider morphs ranking from smooth/expected (0) to remote/colourful (1).
// Mode toggle: Theory (rule-based) vs AI (offline ChordSeqAI GRU, hybrid labels).
// Each row has:
//  - a play button (preview only — does not change "current")
//  - drag-to-sequencer / DAW (same temp-.mid mechanism as ChordCard)
//  - click on the rest of the row to make it the new current chord
class NextChordPanel : public nui::Component,
                       private juce::Slider::Listener,
                       public nelement::TextButton::OnClickListener
{
public:
    enum class SuggestionMode
    {
        Theory,
        Ai
    };

    using OnCandidateChosen = std::function<void(const theory::NextChordCandidate&)>;
    using OnCandidatePreview = std::function<void(const theory::NextChordCandidate&)>;
    using OnCandidateDragStarted = std::function<void(const theory::NextChordCandidate&)>;

    explicit NextChordPanel(const std::string& identifier);
    ~NextChordPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setKeyAndScale(theory::Key key, theory::Scale scale);
    void setCurrentChord(const theory::Chord& chord);
    // Progression chords before the current one (phrase memory for ranking). Empty clears context.
    void setSequenceContext(theory::SequenceContext sequence);
    // Atomically update current chord + sequence memory (one regenerate).
    void setCurrentChord(const theory::Chord& chord, theory::SequenceContext sequence);
    void clear();

    void setDrama01(float drama01);
    [[nodiscard]] float getDrama01() const { return _drama01; }
    [[nodiscard]] const theory::SequenceContext& getSequenceContext() const { return _sequence; }

    void setSuggestionMode(SuggestionMode mode);
    [[nodiscard]] SuggestionMode getSuggestionMode() const { return _mode; }

    void setOnCandidateChosen(OnCandidateChosen callback) { _onCandidateChosen = std::move(callback); }
    void setOnCandidatePreview(OnCandidatePreview callback) { _onCandidatePreview = std::move(callback); }
    void setOnCandidateDragStarted(OnCandidateDragStarted callback) { _onCandidateDragStarted = std::move(callback); }

    [[nodiscard]] const std::vector<theory::NextChordCandidate>& getCandidates() const { return _candidates; }
    [[nodiscard]] const std::optional<theory::Chord>& getCurrentChord() const { return _currentChord; }

    void onButtonClick(const std::string& componentID) override;

private:
    void sliderValueChanged(juce::Slider* slider) override;
    class Row : public nui::Component, public nelement::SVGButton::OnClickListener
    {
    public:
        Row(const std::string& identifier, NextChordPanel& owner, theory::NextChordCandidate candidate, int rowIndex);
        ~Row() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void onButtonClick(const std::string& componentID) override;

        void setCandidate(theory::NextChordCandidate candidate);

    private:
        NextChordPanel& _owner;
        theory::NextChordCandidate _candidate;
        int _rowIndex = 0;
        bool _dragGestureStarted = false;
        bool _mouseDownOnPlay = false;

        nelement::SVGButton _playButton;
        static constexpr float kDragStartThreshold = 6.f;
        static constexpr int kPlayButtonSize = 22;
    };

    class ListContent : public juce::Component
    {
    public:
        explicit ListContent(NextChordPanel& owner);
        void resized() override;
        void rebuildRows();

    private:
        NextChordPanel& _owner;
        std::vector<std::unique_ptr<Row>> _rows;
    };

    void regenerate();
    void rebuildList();
    void syncLabels();
    void syncModeButton();

    theory::Key _key = theory::Key::C;
    theory::Scale _scale = theory::Scale::Major;
    std::optional<theory::Chord> _currentChord;
    theory::SequenceContext _sequence;
    std::vector<theory::NextChordCandidate> _candidates;
    float _drama01 = 0.35f;
    SuggestionMode _mode = SuggestionMode::Theory;

    OnCandidateChosen _onCandidateChosen;
    OnCandidatePreview _onCandidatePreview;
    OnCandidateDragStarted _onCandidateDragStarted;

    nelement::Text _title { "next-chord-title", "", "Next chords" };
    nelement::TextButton _modeButton { "next-chord-mode", "Theory" };
    nelement::Text _currentLabel { "next-chord-current", "", "" };
    nelement::Text _dramaLabel { "next-chord-drama-label", "", "Drama" };
    nelement::Text _dramaLowLabel { "next-chord-drama-low", "", "Smooth" };
    nelement::Text _dramaHighLabel { "next-chord-drama-high", "", "Wild" };
    juce::Slider _dramaSlider;
    juce::Viewport _viewport;
    ListContent _listContent { *this };

    static constexpr int kRowHeight = 30;
    static constexpr int kPadding = 8;
    static constexpr int kScrollbarThickness = 8;
    static constexpr int kDramaRowHeight = 22;
    static constexpr int kModeButtonWidth = 56;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NextChordPanel)
};

}
