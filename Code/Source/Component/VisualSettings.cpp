#include "Component/VisualSettings.h"
#include "AppSettings.h"
#include "AppLocalisation.h"

namespace component
{

ColourSwatch::ColourSwatch(const std::string& componentId)
{
    setComponentID(componentId);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void ColourSwatch::setColour(juce::Colour colour)
{
    _colour = colour;
    repaint();
}

void ColourSwatch::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.f);
    const auto radius = juce::jmin(6.f, bounds.getHeight() * 0.35f);

    g.setColour(_colour);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.drawRoundedRectangle(bounds, radius, 1.f);

    if (_colour.getAlpha() < 0.15f || _colour.getBrightness() > 0.92f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.drawRoundedRectangle(bounds.reduced(3.f), juce::jmax(1.f, radius - 2.f), 1.f);
    }
}

void ColourSwatch::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && onClick)
        onClick();
}

VisualSettings::VisualSettings(const std::string& identifier):
    Component(identifier)
{
    _title.setFontSize(nui::Theme::SMALL);
    _title.setColor(nui::Theme::ThemeColor::DISABLED);
    _title.setJustificationType(juce::Justification::centredLeft);

    _themeLabel.setJustificationType(juce::Justification::centredLeft);
    _noteTextColourLabel.setJustificationType(juce::Justification::centredLeft);
    _chordHighlightColourLabel.setJustificationType(juce::Justification::centredLeft);
    _scaleHighlightColourLabel.setJustificationType(juce::Justification::centredLeft);
    _midiInputHighlightColourLabel.setJustificationType(juce::Justification::centredLeft);

    _themeSwitch.addOnValueChangedListener(this);
    _themeSwitch.setSelectedIndex(AppSettings::getInstance().getThemeMode() == nui::Theme::Mode::LIGHT ? 1 : 0, juce::dontSendNotification);
    _themeSwitch.setSelectedInvertedTextColor(true);
    _themeSwitch.setHeightType(nui::Theme::HeightType::THIN);
    _themeSwitch.setRounded(true);

    _noteTextColourSwatch.setColour(AppSettings::getInstance().getNoteTextColour());
    _noteTextColourSwatch.onClick = [this]
    {
        openColourPicker(ColourTarget::NoteText, _noteTextColourSwatch, AppSettings::getInstance().getNoteTextColour());
    };

    _chordHighlightColourSwatch.setColour(AppSettings::getInstance().getChordHighlightColour());
    _chordHighlightColourSwatch.onClick = [this]
    {
        openColourPicker(ColourTarget::ChordHighlight, _chordHighlightColourSwatch, AppSettings::getInstance().getChordHighlightColour());
    };

    _scaleHighlightColourSwatch.setColour(AppSettings::getInstance().getScaleHighlightColour());
    _scaleHighlightColourSwatch.onClick = [this]
    {
        openColourPicker(ColourTarget::ScaleHighlight, _scaleHighlightColourSwatch, AppSettings::getInstance().getScaleHighlightColour());
    };

    _midiInputHighlightColourSwatch.setColour(AppSettings::getInstance().getMidiInputHighlightColour());
    _midiInputHighlightColourSwatch.onClick = [this]
    {
        openColourPicker(ColourTarget::MidiInputHighlight, _midiInputHighlightColourSwatch, AppSettings::getInstance().getMidiInputHighlightColour());
    };

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    AppSettings::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1, 1, 1, 1, 1 }, { 1, 4 });

    _layout.setFixedRowHeight(0, 32.f);
    _layout.setFixedRowHeight(1, 36.f);
    _layout.setFixedRowHeight(2, 36.f);
    _layout.setFixedRowHeight(3, 36.f);
    _layout.setFixedRowHeight(4, 36.f);
    _layout.setFixedRowHeight(5, 36.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(_themeLabel, 1, 0, 1, 1);
    _layout.addComponent(_themeSwitch, 1, 1, 1, 1, 10);
    _layout.addComponent(_noteTextColourLabel, 2, 0, 1, 1);
    _layout.addComponent(_noteTextColourSwatch.getComponentID().toStdString(), _noteTextColourSwatch, 2, 1, 1, 1, 10);
    _layout.addComponent(_chordHighlightColourLabel, 3, 0, 1, 1);
    _layout.addComponent(_chordHighlightColourSwatch.getComponentID().toStdString(), _chordHighlightColourSwatch, 3, 1, 1, 1, 10);
    _layout.addComponent(_scaleHighlightColourLabel, 4, 0, 1, 1);
    _layout.addComponent(_scaleHighlightColourSwatch.getComponentID().toStdString(), _scaleHighlightColourSwatch, 4, 1, 1, 1, 10);
    _layout.addComponent(_midiInputHighlightColourLabel, 5, 0, 1, 1);
    _layout.addComponent(_midiInputHighlightColourSwatch.getComponentID().toStdString(), _midiInputHighlightColourSwatch, 5, 1, 1, 1, 10);

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

VisualSettings::~VisualSettings()
{
    if (_activeColourSelector != nullptr)
        _activeColourSelector->removeChangeListener(this);

    _themeSwitch.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
    AppSettings::getChangeBroadcaster().removeChangeListener(this);
}

void VisualSettings::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void VisualSettings::resized()
{
    Component::resized();

    _layout.resized();
}

void VisualSettings::onSelectionChanged(const std::string& componentID, int selectedIndex)
{
    if (componentID != _themeSwitch.getComponentID())
        return;

    const auto mode = selectedIndex == 1 ? nui::Theme::Mode::LIGHT : nui::Theme::Mode::DARK;
    AppSettings::getInstance().setThemeMode(mode);
    nui::Theme::setMode(mode);

    if (auto* top = getTopLevelComponent())
        top->repaint();
}

void VisualSettings::openColourPicker(ColourTarget target, ColourSwatch& swatch, juce::Colour current)
{
    _activeColourTarget = target;

    auto selector = std::make_unique<juce::ColourSelector>(
        juce::ColourSelector::showColourAtTop
            | juce::ColourSelector::showSliders
            | juce::ColourSelector::showColourspace);
    selector->setName("visual-colour-selector");
    selector->setCurrentColour(current, juce::dontSendNotification);
    selector->setColour(juce::ColourSelector::backgroundColourId,
        nui::Theme::newColor(nui::Theme::ThemeColor::PRIMARY).asJuce());
    selector->setSize(280, 320);
    selector->addChangeListener(this);

    _activeColourSelector = selector.get();

    juce::CallOutBox::launchAsynchronously(std::move(selector), swatch.getScreenBounds(), nullptr);
}

void VisualSettings::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        repaint();
        return;
    }

    if (source == &AppSettings::getChangeBroadcaster())
    {
        _noteTextColourSwatch.setColour(AppSettings::getInstance().getNoteTextColour());
        _chordHighlightColourSwatch.setColour(AppSettings::getInstance().getChordHighlightColour());
        _scaleHighlightColourSwatch.setColour(AppSettings::getInstance().getScaleHighlightColour());
        _midiInputHighlightColourSwatch.setColour(AppSettings::getInstance().getMidiInputHighlightColour());
        if (auto* top = getTopLevelComponent())
            top->repaint();
        return;
    }

    if (auto* selector = dynamic_cast<juce::ColourSelector*>(source))
    {
        const auto colour = selector->getCurrentColour();
        switch (_activeColourTarget)
        {
            case ColourTarget::ScaleHighlight:
                AppSettings::getInstance().setScaleHighlightColour(colour);
                _scaleHighlightColourSwatch.setColour(colour);
                break;
            case ColourTarget::ChordHighlight:
                AppSettings::getInstance().setChordHighlightColour(colour);
                _chordHighlightColourSwatch.setColour(colour);
                break;
            case ColourTarget::MidiInputHighlight:
                AppSettings::getInstance().setMidiInputHighlightColour(colour);
                _midiInputHighlightColourSwatch.setColour(colour);
                break;
            case ColourTarget::NoteText:
                AppSettings::getInstance().setNoteTextColour(colour);
                _noteTextColourSwatch.setColour(colour);
                break;
        }
        if (auto* top = getTopLevelComponent())
            top->repaint();
        return;
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("visual_settings_title").toStdString());
    _themeLabel.setText(juce::translate("visual_settings_theme_label").toStdString());
    _themeSwitch.setLabels(juce::translate("visual_settings_dark_theme").toStdString(), juce::translate("visual_settings_light_theme").toStdString());
    _noteTextColourLabel.setText(juce::translate("visual_settings_note_text_colour_label").toStdString());
    _chordHighlightColourLabel.setText(juce::translate("visual_settings_chord_highlight_colour_label").toStdString());
    _scaleHighlightColourLabel.setText(juce::translate("visual_settings_scale_highlight_colour_label").toStdString());
    _midiInputHighlightColourLabel.setText(juce::translate("visual_settings_midi_input_highlight_colour_label").toStdString());

    repaint();
}

}
