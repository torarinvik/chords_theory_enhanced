#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/NextScaleGenerator.h"
#include "Theory/Scale.h"
#include "Theory/ScaleSuggestion.h"

namespace component
{

// Scrollable list of ranked scale suggestions (parallel modes, relative keys, chord-fit).
// Click a row to apply that key+scale; search query / All vs Predicted scope filter the list.
class ScaleSuggestionPanel : public nui::Component
{
public:
    using OnScaleChosen = std::function<void(theory::Key key, theory::Scale scale)>;

    explicit ScaleSuggestionPanel(const std::string& identifier);
    ~ScaleSuggestionPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void setKeyAndScale(theory::Key key, theory::Scale scale);
    void setCurrentChord(const std::optional<theory::Chord>& chord);
    void setSearchQuery(const std::string& query);
    void setSearchScope(theory::NextScaleGenerator::Pool pool);

    void setOnScaleChosen(OnScaleChosen callback) { _onScaleChosen = std::move(callback); }

    [[nodiscard]] const std::vector<theory::ScaleSuggestion>& getSuggestions() const { return _suggestions; }

private:
    class Row : public nui::Component
    {
    public:
        Row(const std::string& identifier, ScaleSuggestionPanel& owner, theory::ScaleSuggestion suggestion, int rowIndex);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void setSuggestion(theory::ScaleSuggestion suggestion);

    private:
        ScaleSuggestionPanel& _owner;
        theory::ScaleSuggestion _suggestion;
        int _rowIndex = 0;
        bool _dragGestureStarted = false;
        static constexpr float kDragStartThreshold = 6.f;
    };

    class ListContent : public juce::Component
    {
    public:
        explicit ListContent(ScaleSuggestionPanel& owner);
        void resized() override;
        void rebuildRows(const std::vector<theory::ScaleSuggestion>& suggestions);

    private:
        ScaleSuggestionPanel& _owner;
        std::vector<std::unique_ptr<Row>> _rows;
    };

    void regenerate();
    void syncLabels();

    theory::Key _key = theory::Key::C;
    theory::Scale _scale = theory::Scale::Major;
    std::optional<theory::Chord> _currentChord;
    std::string _query;
    theory::NextScaleGenerator::Pool _pool = theory::NextScaleGenerator::Pool::All;
    std::vector<theory::ScaleSuggestion> _suggestions;

    OnScaleChosen _onScaleChosen;

    nelement::Text _title { "scale-suggestion-title", "", "Scale suggestions" };
    nelement::Text _subtitle { "scale-suggestion-subtitle", "", "" };
    nelement::Text _emptyHint { "scale-suggestion-empty", "", "" };
    juce::Viewport _viewport;
    ListContent _listContent { *this };

    static constexpr int kRowHeight = 30;
    static constexpr int kPadding = 8;
    static constexpr int kScrollbarThickness = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScaleSuggestionPanel)
};

}
