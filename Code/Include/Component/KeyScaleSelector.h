#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace component
{

// Header controls: a search field + Chord/Scale mode switch to the left of the Key and Scale
// comboboxes. Search behaviour is wired later (mode is stored now so UI can drive it).
class KeyScaleSelector : public nui::Component,
                         public nelement::ComboBox::OnValueChangedListener,
                         public nelement::TwoWaySwitch::OnValueChangedListener,
                         public nelement::TextInput::OnValueChangedListener
{
public:
    enum class SearchMode
    {
        Chord = 0,
        Scale = 1,
    };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void onKeyScaleChanged(theory::Key key, theory::Scale scale) = 0;

        // Fired when the search query or Chord/Scale mode changes (search results not applied yet).
        virtual void onSearchChanged(const std::string& query, SearchMode mode)
        {
            juce::ignoreUnused(query, mode);
        }
    };

    explicit KeyScaleSelector(const std::string& identifier);
    ~KeyScaleSelector() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    [[nodiscard]] theory::Key getKey() const { return _currentKey; }
    [[nodiscard]] theory::Scale getScale() const { return _currentScale; }

    [[nodiscard]] std::string getSearchQuery() const { return _searchInput.getText(); }
    [[nodiscard]] SearchMode getSearchMode() const { return _searchMode; }

    // Sets the pickers' state without notifying listeners - used to restore saved session state.
    void setKeyAndScale(theory::Key key, theory::Scale scale);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onSelectionChanged(const std::string& componentID, int selectedId) override;
    void onValueChanged(const std::string& componentID, const std::string& newValue) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void notifyKeyScaleListeners();
    void notifySearchListeners();

    nelement::TextInput _searchInput { "key-scale-selector-search" };
    nelement::TwoWaySwitch _searchModeSwitch {
        "key-scale-selector-search-mode",
        juce::translate("key_scale_selector_search_chord").toStdString(),
        juce::translate("key_scale_selector_search_scale").toStdString()
    };

    nelement::Text _keyLabel { "key-scale-selector-key-label" };
    nelement::ComboBox _keyPicker { "key-scale-selector-key-picker" };
    nelement::Text _scaleLabel { "key-scale-selector-scale-label" };
    nelement::ComboBox _scalePicker { "key-scale-selector-scale-picker" };

    theory::Key _currentKey = theory::Key::C;
    theory::Scale _currentScale = theory::Scale::Major;
    SearchMode _searchMode = SearchMode::Chord;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyScaleSelector)
};

}
