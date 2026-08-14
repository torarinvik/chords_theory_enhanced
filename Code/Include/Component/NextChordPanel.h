#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordGenerator.h"
#include "Theory/NextChordSequenceContext.h"
#include "Theory/Scale.h"

namespace component
{

// Dual-column next-chord suggestions:
//  - Left: rule-based ranking (play, Fit/Tension, Drama slider)
//  - Right: pure offline ChordSeqAI suggestions (no hybrid with theory)
// Each row supports play-preview, drag-to-sequencer/DAW, and click-to-set-current.
class NextChordPanel : public nui::Component,
                       private juce::Slider::Listener
{
public:
    enum class Column
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
    // Header search: query filters the list; All = full catalogue, Predicted = suggestion pool.
    void setSearchQuery(const std::string& query);
    void setSearchScope(theory::NextChordGenerator::Pool pool);
    void clear();

    void setDrama01(float drama01);
    [[nodiscard]] float getDrama01() const { return _drama01; }
    [[nodiscard]] const theory::SequenceContext& getSequenceContext() const { return _sequence; }

    void setOnCandidateChosen(OnCandidateChosen callback) { _onCandidateChosen = std::move(callback); }
    void setOnCandidatePreview(OnCandidatePreview callback) { _onCandidatePreview = std::move(callback); }
    void setOnCandidateDragStarted(OnCandidateDragStarted callback) { _onCandidateDragStarted = std::move(callback); }

    // Left column (symbolic ranking).
    [[nodiscard]] const std::vector<theory::NextChordCandidate>& getCandidates() const { return _theoryCandidates; }
    [[nodiscard]] const std::vector<theory::NextChordCandidate>& getTheoryCandidates() const { return _theoryCandidates; }
    // Right column (pure AI ranking).
    [[nodiscard]] const std::vector<theory::NextChordCandidate>& getAiCandidates() const { return _aiCandidates; }
    [[nodiscard]] const std::optional<theory::Chord>& getCurrentChord() const { return _currentChord; }

private:
    void sliderValueChanged(juce::Slider* slider) override;

    class Row : public nui::Component, public nelement::SVGButton::OnClickListener
    {
    public:
        Row(const std::string& identifier,
            NextChordPanel& owner,
            theory::NextChordCandidate candidate,
            int rowIndex,
            Column column);
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
        Column _column = Column::Theory;
        bool _dragGestureStarted = false;
        bool _mouseDownOnPlay = false;

        nelement::SVGButton _playButton;
        static constexpr float kDragStartThreshold = 6.f;
        static constexpr int kPlayButtonSize = 22;
    };

    class ListContent : public juce::Component
    {
    public:
        ListContent(NextChordPanel& owner, Column column);
        void resized() override;
        void rebuildRows(const std::vector<theory::NextChordCandidate>& candidates);

    private:
        NextChordPanel& _owner;
        Column _column;
        std::vector<std::unique_ptr<Row>> _rows;
    };

    void regenerate();
    void rebuildLists();
    void syncLabels();

    theory::Key _key = theory::Key::C;
    theory::Scale _scale = theory::Scale::Major;
    std::optional<theory::Chord> _currentChord;
    theory::SequenceContext _sequence;
    std::string _searchQuery;
    theory::NextChordGenerator::Pool _searchPool = theory::NextChordGenerator::Pool::All;
    std::vector<theory::NextChordCandidate> _theoryCandidates;
    std::vector<theory::NextChordCandidate> _aiCandidates;
    float _drama01 = 0.35f;

    OnCandidateChosen _onCandidateChosen;
    OnCandidatePreview _onCandidatePreview;
    OnCandidateDragStarted _onCandidateDragStarted;

    nelement::Text _title { "next-chord-title", "", "Next chords" };
    nelement::Text _currentLabel { "next-chord-current", "", "" };
    nelement::Text _dramaLabel { "next-chord-drama-label", "", "Drama" };
    nelement::Text _dramaLowLabel { "next-chord-drama-low", "", "Smooth" };
    nelement::Text _dramaHighLabel { "next-chord-drama-high", "", "Wild" };
    juce::Slider _dramaSlider;
    nelement::Text _theoryColumnLabel { "next-chord-theory-col", "", "Theory" };
    nelement::Text _aiColumnLabel { "next-chord-ai-col", "", "AI" };
    nelement::Text _aiEmptyHint { "next-chord-ai-empty", "", "" };
    juce::Viewport _theoryViewport;
    juce::Viewport _aiViewport;
    ListContent _theoryListContent { *this, Column::Theory };
    ListContent _aiListContent { *this, Column::Ai };

    static constexpr int kRowHeight = 30;
    static constexpr int kPadding = 8;
    static constexpr int kScrollbarThickness = 8;
    static constexpr int kDramaRowHeight = 22;
    static constexpr int kColumnHeaderHeight = 16;
    static constexpr int kColumnGap = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NextChordPanel)
};

}
