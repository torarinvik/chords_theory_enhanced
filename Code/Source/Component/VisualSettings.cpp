#include "Component/VisualSettings.h"
#include "AppSettings.h"
#include "AppLocalisation.h"

namespace component
{

NoteTextColourSwatch::NoteTextColourSwatch(const std::string& componentId)
{
    setComponentID(componentId);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void NoteTextColourSwatch::setColour(juce::Colour colour)
{
    _colour = colour;
    repaint();
}

void NoteTextColourSwatch::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.f);
    const auto radius = juce::jmin(6.f, bounds.getHeight() * 0.35f);

    g.setColour(_colour);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.drawRoundedRectangle(bounds, radius, 1.f);

    // Checker peek for near-transparent / very light colours so the chip never looks empty.
    if (_colour.getAlpha() < 0.15f || _colour.getBrightness() > 0.92f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.drawRoundedRectangle(bounds.reduced(3.f), juce::jmax(1.f, radius - 2.f), 1.f);
    }
}

void NoteTextColourSwatch::mouseUp(const juce::MouseEvent& event)
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

    _themeSwitch.addOnValueChangedListener(this);
    _themeSwitch.setSelectedIndex(AppSettings::getInstance().getThemeMode() == nui::Theme::Mode::LIGHT ? 1 : 0, juce::dontSendNotification);
    _themeSwitch.setSelectedInvertedTextColor(true);
    _themeSwitch.setHeightType(nui::Theme::HeightType::THIN);
    _themeSwitch.setRounded(true);

    _noteTextColourSwatch.setColour(AppSettings::getInstance().getNoteTextColour());
    _noteTextColourSwatch.onClick = [this] { openNoteTextColourPicker(); };

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    AppSettings::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1, 1 }, { 1, 4 });

    _layout.setFixedRowHeight(0, 32.f);
    _layout.setFixedRowHeight(1, 36.f);
    _layout.setFixedRowHeight(2, 36.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(_themeLabel, 1, 0, 1, 1);
    _layout.addComponent(_themeSwitch, 1, 1, 1, 1, 10);
    _layout.addComponent(_noteTextColourLabel, 2, 0, 1, 1);
    // Swatch is a plain juce::Component (not nui::Component) - use the id overload.
    _layout.addComponent(_noteTextColourSwatch.getComponentID().toStdString(), _noteTextColourSwatch, 2, 1, 1, 1, 10);

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

void VisualSettings::openNoteTextColourPicker()
{
    auto selector = std::make_unique<juce::ColourSelector>(
        juce::ColourSelector::showColourAtTop
            | juce::ColourSelector::showSliders
            | juce::ColourSelector::showColourspace);
    selector->setName("note-text-colour-selector");
    selector->setCurrentColour(AppSettings::getInstance().getNoteTextColour(), juce::dontSendNotification);
    selector->setColour(juce::ColourSelector::backgroundColourId,
        nui::Theme::newColor(nui::Theme::ThemeColor::PRIMARY).asJuce());
    selector->setSize(280, 320);
    selector->addChangeListener(this);

    _activeColourSelector = selector.get();

    juce::CallOutBox::launchAsynchronously(std::move(selector),
        _noteTextColourSwatch.getScreenBounds(), nullptr);
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
        if (auto* top = getTopLevelComponent())
            top->repaint();
        return;
    }

    if (auto* selector = dynamic_cast<juce::ColourSelector*>(source))
    {
        AppSettings::getInstance().setNoteTextColour(selector->getCurrentColour());
        _noteTextColourSwatch.setColour(selector->getCurrentColour());
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

    repaint();
}

}
