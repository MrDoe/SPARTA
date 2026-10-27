/*
 ==============================================================================

  This file is part of SPARTA; a suite of spatial audio plug-ins.
  Copyright (c) 2026 - SPARTA contributors.

  SPARTA is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  SPARTA is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SPARTA.  If not, see <http://www.gnu.org/licenses/>.

 ==============================================================================
*/

#include "InspectorPanel.h"
#include "ActorLibrary.h"
#include "StudioTheme.h"

#include "../../../resources/ParameterManager.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr int headerHeight = 44;
    constexpr int sidePad = 12;

    /* A row of buttons that shares the available width evenly. */
    class ButtonStrip : public juce::Component
    {
    public:
        void add (juce::Button& button)
        {
            addAndMakeVisible (button);
            buttons.add (&button);
        }

        void resized() override
        {
            const int n = buttons.size();
            if (n <= 0)
                return;

            constexpr int gap = 5;
            const int totalGap = gap * (n - 1);
            const int buttonWidth = juce::jmax (20, (getWidth() - totalGap) / n);
            int x = 0;

            for (int i = 0; i < n; ++i)
            {
                const int w = (i == n - 1) ? juce::jmax (20, getWidth() - x)
                                           : buttonWidth;
                buttons[i]->setBounds (x, 0, w, getHeight());
                x += w + gap;
            }
        }

    private:
        juce::Array<juce::Button*> buttons;
    };

    struct MaterialPreset
    {
        const char* name;
        float absorption;
        const char* description;
    };

    const MaterialPreset materialPresets[] =
    {
        { "Live",     0.15f, "Hard, reflective surfaces (approx. 0.15 absorption)" },
        { "Balanced", 0.45f, "Typical furnished room (approx. 0.45 absorption)" },
        { "Treated",  0.75f, "Acoustic treatment (approx. 0.75 absorption)" },
        { "Dead",     0.95f, "Nearly anechoic walls (approx. 0.95 absorption)" }
    };

    juce::String objectName (PluginProcessor& processor, bool isReceiver, int index)
    {
        juce::String name;
        {
            const juce::ScopedLock sl (processor.getSceneLock());
            name = processor.getSceneModel().getActor (isReceiver, index).name;
        }

        juce::String title = juce::String (isReceiver ? "R" : "S")
                           + juce::String (index + 1);
        if (name.isNotEmpty())
            title += "  " + name;
        return title;
    }
}

/* ---------------------------------------------------------------------------
   The scrollable inspector content. It is rebuilt whenever the selection
   changes, which keeps the parameter attachments and the actor controls in
   sync without a wall of manual show/hide logic.
   --------------------------------------------------------------------------- */
class InspectorPanel::Content : public juce::Component
{
public:
    Content (PluginProcessor& p, const studio::Selection& sel)
        : processor (p), selection (sel)
    {
        setOpaque (true);

        isCheckpoint = selection.isCheckpoint();
        hasObject = selection.isActor() || isCheckpoint;
        isReceiver = isCheckpoint ? selection.checkpointIsReceiver
                                  : selection.isReceiver();
        index = juce::jlimit (0, studio::maxSceneObjects - 1, selection.index);

        buildRoomSection();
        buildMaterialsSection();

        if (hasObject)
        {
            buildActorSection();
            buildPathSection();
            if (isCheckpoint)
                buildCheckpointSection();
        }
        else
        {
            addSection ("Object");
            addNote ("No object selected. Click a source or listener in the "
                     "canvas, or pick one from the source bar.");
        }

        buildCanvasSection();
        buildOutputSection();
    }

    ~Content() override = default;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (studio::Colours::panel);

        for (size_t i = 0; i < rows.size() && i < rowBounds.size(); ++i)
        {
            const auto& row = rows[i];
            const auto r = rowBounds[i];

            switch (row.kind)
            {
                case Row::Kind::section:
                {
                    g.setColour (row.accent.withAlpha (0.95f));
                    g.fillRoundedRectangle ((float) sidePad,
                                            (float) r.getCentreY() - 7.0f,
                                            2.0f, 14.0f, 1.0f);

                    g.setColour (studio::Colours::textSecondary);
                    g.setFont (studio::uiFont (10.5f, true));
                    g.drawText (row.text.toUpperCase(),
                                r.withTrimmedLeft (sidePad + 9)
                                 .withTrimmedRight (sidePad),
                                juce::Justification::centredLeft, false);
                    break;
                }

                case Row::Kind::control:
                {
                    if (row.labelWidth > 0 && row.text.isNotEmpty())
                    {
                        g.setColour (studio::Colours::textSecondary);
                        g.setFont (studio::uiFont (11.0f));
                        g.drawText (row.text,
                                    r.withTrimmedLeft (sidePad)
                                     .withWidth (row.labelWidth - 6),
                                    juce::Justification::centredLeft, true);
                    }
                    break;
                }

                case Row::Kind::value:
                {
                    g.setColour (studio::Colours::textMuted);
                    g.setFont (studio::uiFont (11.0f));
                    g.drawText (row.text,
                                r.withTrimmedLeft (sidePad)
                                 .withWidth (juce::jmax (0, row.labelWidth - 6)),
                                juce::Justification::centredLeft, true);
                    break;
                }

                case Row::Kind::note:
                {
                    g.setColour (studio::Colours::textMuted);
                    g.setFont (studio::uiFont (10.5f));
                    g.drawFittedText (row.text,
                                      r.withTrimmedLeft (sidePad)
                                       .withTrimmedRight (sidePad),
                                      juce::Justification::topLeft,
                                      juce::jmax (1, row.height / 15), 1.0f);
                    break;
                }

                case Row::Kind::separator:
                {
                    g.setColour (studio::Colours::stroke);
                    g.drawHorizontalLine (r.getCentreY(), (float) sidePad,
                                          (float) getWidth() - (float) sidePad);
                    break;
                }
            }
        }
    }

    void resized() override
    {
        rowBounds.clear();
        rowBounds.reserve (rows.size());

        int y = 6;
        for (const auto& row : rows)
        {
            auto r = juce::Rectangle<int> (0, y, getWidth(), row.height);
            rowBounds.push_back (r);

            const int controlHeight = juce::jmin (24, row.height);
            auto controlArea = r.withTrimmedLeft (sidePad + row.labelWidth)
                                .withTrimmedRight (sidePad);

            if (row.control != nullptr && controlArea.getWidth() > 8)
                row.control->setBounds (controlArea.withSizeKeepingCentre (
                                            controlArea.getWidth(), controlHeight));

            if (row.valueLabel != nullptr && controlArea.getWidth() > 8)
                row.valueLabel->setBounds (controlArea.withSizeKeepingCentre (
                                               controlArea.getWidth(),
                                               juce::jmin (22, row.height)));

            y = r.getBottom();
        }
    }

    int getPreferredHeight() const
    {
        int total = 12;
        for (const auto& row : rows)
            total += row.height;
        return total;
    }

    void refreshValues()
    {
        if (! hasObject)
            return;

        const bool isPpq = processor.isTimeInPpq();

        size_t numKeyframes = 0;
        bool pathEnabled = false;
        bool pathLoop = false;
        double pathStart = 0.0, pathEnd = 0.0;

        {
            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            const PathData& path = currentPath();
            numKeyframes = path.keyframes.size();
            pathEnabled = path.enabled;
            pathLoop = path.loop;
            pathStart = path.startTime;
            pathEnd = path.endTime;
        }

        if (pathCountValue != nullptr)
            pathCountValue->setText (juce::String ((int) numKeyframes),
                                     juce::dontSendNotification);

        if (pathRangeValue != nullptr)
            pathRangeValue->setText (studio::formatTime (pathStart, isPpq)
                                        + " - "
                                        + studio::formatTime (pathEnd, isPpq),
                                     juce::dontSendNotification);

        pathEnabledToggle.setToggleState (pathEnabled, juce::dontSendNotification);
        pathEnabledToggle.setEnabled (numKeyframes > 0);

        pathLoopToggle.setToggleState (pathLoop, juce::dontSendNotification);
        pathLoopToggle.setEnabled (numKeyframes > 1);

        const double sliderMax = juce::jmax (600.0, pathEnd + 60.0);
        if (sliderMax > pathStartSlider.getMaximum())
        {
            pathStartSlider.setRange (0.0, sliderMax, 0.01);
            pathEndSlider.setRange (0.0, sliderMax, 0.01);
        }

        if (std::abs (pathStartSlider.getValue() - pathStart) > 1.0e-6)
            pathStartSlider.setValue (pathStart, juce::dontSendNotification);

        if (std::abs (pathEndSlider.getValue() - pathEnd) > 1.0e-6)
            pathEndSlider.setValue (pathEnd, juce::dontSendNotification);

        if (isCheckpoint && checkpointTimeValue != nullptr)
        {
            juce::String timeText = "-";

            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            const PathData& path = currentPath();
            if (selection.keyframe >= 0
                && selection.keyframe < (int) path.keyframes.size())
            {
                const auto& keyframe = path.keyframes[(size_t) selection.keyframe];
                timeText = studio::formatTime (path.startTime + keyframe.timeSeconds,
                                               isPpq);

                if (! editingCheckpointSliders)
                {
                    checkpointXSlider.setValue (keyframe.x, juce::dontSendNotification);
                    checkpointYSlider.setValue (keyframe.y, juce::dontSendNotification);
                    checkpointZSlider.setValue (keyframe.z, juce::dontSendNotification);
                    checkpointStopSlider.setValue (keyframe.stopTime,
                                                   juce::dontSendNotification);
                }
            }

            checkpointTimeValue->setText (timeText, juce::dontSendNotification);
        }
    }

    std::function<void()> onEdited;
    std::function<void()> onFrameRequested;
    std::function<void()> onStructureChanged;
    std::function<void (studio::Selection)> onSelectionRequested;

private:
    enum class RowKind
    {
        section,
        control,
        value,
        note,
        separator
    };

    struct Row
    {
        using Kind = RowKind;

        Kind kind = Kind::control;
        juce::String text;
        juce::Component* control = nullptr;
        juce::Label* valueLabel = nullptr;
        int height = 30;
        int labelWidth = 88;
        juce::Colour accent = studio::Colours::accent;
    };

    PluginProcessor& processor;
    studio::Selection selection;

    bool isCheckpoint = false;
    bool hasObject = false;
    bool isReceiver = false;
    int index = 0;
    bool editingCheckpointSliders = false;

    std::vector<Row> rows;
    std::vector<juce::Rectangle<int>> rowBounds;

    /* Attached parameter controls. */
    std::vector<std::unique_ptr<SliderWithAttachment>> attachedSliders;
    std::vector<std::unique_ptr<ComboBoxWithAttachment>> attachedCombos;
    std::vector<std::unique_ptr<ToggleButtonWithAttachment>> attachedToggles;

    /* Read-only values. */
    juce::OwnedArray<juce::Label> labels;
    juce::Label* pathCountValue = nullptr;
    juce::Label* pathRangeValue = nullptr;
    juce::Label* checkpointTimeValue = nullptr;

    /* Actor casting controls (scene metadata, not parameters). */
    juce::ComboBox actorKindCombo;
    juce::TextEditor actorNameEditor;
    juce::Slider yawSlider;
    juce::Slider scaleSlider;

    /* Path controls. */
    juce::ToggleButton pathEnabledToggle;
    juce::ToggleButton pathLoopToggle;
    juce::Slider pathStartSlider;
    juce::Slider pathEndSlider;
    juce::TextButton clearPathButton { "Clear path" };

    /* Checkpoint controls. */
    juce::Slider checkpointXSlider;
    juce::Slider checkpointYSlider;
    juce::Slider checkpointZSlider;
    juce::Slider checkpointStopSlider;
    juce::TextButton deleteCheckpointButton { "Delete node" };

    /* Canvas preferences. */
    juce::ToggleButton showGridToggle;
    juce::ToggleButton showTimesToggle;
    juce::ToggleButton showLabelsToggle;
    juce::ToggleButton snapToggle;

    /* Material presets. */
    ButtonStrip presetStrip;
    juce::TextButton presetButtons[4];

    juce::TextButton frameButton { "Frame in canvas" };

    /* ----------------------------------------------------------------------- */

    PathData& currentPath()
    {
        return isReceiver ? processor.getPathBank().getReceiverPath (index)
                          : processor.getPathBank().getSourcePath (index);
    }

    const PathData& currentPath() const
    {
        return isReceiver ? processor.getPathBank().getReceiverPath (index)
                          : processor.getPathBank().getSourcePath (index);
    }

    void editActor (const std::function<void (studio::ActorInfo&)>& fn,
                    bool notifyHost)
    {
        {
            const juce::ScopedLock sl (processor.getSceneLock());
            fn (processor.getSceneModel().getActor (isReceiver, index));
        }

        processor.markSceneDirty (notifyHost);

        if (onEdited)
            onEdited();
    }

    void editPath (const std::function<void (PathData&)>& fn, bool notifyHost)
    {
        {
            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            fn (currentPath());
        }

        processor.markPathDirty (notifyHost);

        if (onEdited)
            onEdited();
    }

    void editPreferences (const std::function<void (studio::UiPreferences&)>& fn)
    {
        {
            const juce::ScopedLock sl (processor.getSceneLock());
            fn (processor.getSceneModel().getUiPreferences());
        }

        processor.markSceneDirty();

        if (onEdited)
            onEdited();
    }

    void addSection (const juce::String& title,
                     juce::Colour accent = studio::Colours::accent)
    {
        Row row;
        row.kind = Row::Kind::section;
        row.text = title;
        row.height = 34;
        row.accent = accent;
        rows.push_back (row);
    }

    void addControl (const juce::String& label, juce::Component& control,
                     int labelWidth = 88, int height = 30)
    {
        addAndMakeVisible (control);

        Row row;
        row.kind = Row::Kind::control;
        row.text = label;
        row.control = &control;
        row.height = height;
        row.labelWidth = labelWidth;
        rows.push_back (row);
    }

    void addValue (const juce::String& label, const juce::String& value,
                   juce::Label** optionalOut = nullptr)
    {
        auto* valueLabel = labels.add (new juce::Label());
        valueLabel->setText (value, juce::dontSendNotification);
        valueLabel->setJustificationType (juce::Justification::centredRight);
        valueLabel->setFont (studio::uiFont (10.5f));
        valueLabel->setColour (juce::Label::textColourId, studio::Colours::text);
        valueLabel->setInterceptsMouseClicks (false, false);
        valueLabel->setBorderSize (juce::BorderSize<int> (0));
        addAndMakeVisible (valueLabel);

        if (optionalOut != nullptr)
            *optionalOut = valueLabel;

        Row row;
        row.kind = Row::Kind::value;
        row.text = label;
        row.valueLabel = valueLabel;
        row.height = 24;
        row.labelWidth = 88;
        rows.push_back (row);
    }

    void addNote (const juce::String& text)
    {
        Row row;
        row.kind = Row::Kind::note;
        row.text = text;
        row.height = 36;
        rows.push_back (row);
    }

    void addSeparator()
    {
        Row row;
        row.kind = Row::Kind::separator;
        row.height = 11;
        rows.push_back (row);
    }

    SliderWithAttachment& makeSlider (const juce::String& parameterID,
                                      int textBoxWidth = 58)
    {
        auto slider = std::make_unique<SliderWithAttachment> (processor.parameters,
                                                              parameterID);
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false,
                                 textBoxWidth, 20);
        slider->setColour (juce::Slider::textBoxTextColourId,
                           studio::Colours::text);
        slider->setColour (juce::Slider::textBoxBackgroundColourId,
                           studio::Colours::surface);
        slider->setColour (juce::Slider::textBoxOutlineColourId,
                           studio::Colours::strokeStrong);

        auto* raw = slider.get();
        slider->onValueChange = [this] { if (onEdited) onEdited(); };
        attachedSliders.push_back (std::move (slider));
        return *raw;
    }

    ComboBoxWithAttachment& makeCombo (const juce::String& parameterID)
    {
        auto combo = std::make_unique<ComboBoxWithAttachment> (processor.parameters,
                                                               parameterID);
        combo->setJustificationType (juce::Justification::centredLeft);
        combo->onChange = [this] { if (onEdited) onEdited(); };

        auto* raw = combo.get();
        attachedCombos.push_back (std::move (combo));
        return *raw;
    }

    ToggleButtonWithAttachment& makeToggle (const juce::String& parameterID,
                                            const juce::String& text)
    {
        auto toggle = std::make_unique<ToggleButtonWithAttachment> (
            processor.parameters, parameterID);
        toggle->setButtonText (text);
        toggle->onClick = [this] { if (onEdited) onEdited(); };

        auto* raw = toggle.get();
        attachedToggles.push_back (std::move (toggle));
        return *raw;
    }

    void configureCustomSlider (juce::Slider& slider, double min, double max,
                                double step, double value,
                                const juce::String& suffix,
                                int textBoxWidth = 56)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxWidth, 20);
        slider.setRange (min, max, step);
        slider.setValue (value, juce::dontSendNotification);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::textBoxTextColourId, studio::Colours::text);
        slider.setColour (juce::Slider::textBoxBackgroundColourId,
                          studio::Colours::surface);
        slider.setColour (juce::Slider::textBoxOutlineColourId,
                          studio::Colours::strokeStrong);
    }

    juce::String roomAxisParameterID (char axis) const
    {
        juce::String id (isReceiver ? "receiver" : "source");
        id << juce::String::charToString ((juce_wchar) axis);
        id << index;
        return id;
    }

    /* ------------------------------------------------------------------ */

    void buildRoomSection()
    {
        addSection ("Room");

        addControl ("Width", makeSlider ("roomX"));
        addControl ("Depth", makeSlider ("roomY"));
        addControl ("Height", makeSlider ("roomZ"));

        addControl ("Sources", makeSlider ("numSources", 40));
        addControl ("Listeners", makeSlider ("numReceivers", 40));

        addNote ("The image-source model uses these dimensions. 1 source and "
                 "1 listener are the minimum.");
    }

    void buildMaterialsSection()
    {
        addSection ("Materials");

        addControl ("Wall X+", makeSlider ("wallAbsCoeff_pX", 46));
        addControl ("Wall X-", makeSlider ("wallAbsCoeff_nX", 46));
        addControl ("Wall Y+", makeSlider ("wallAbsCoeff_pY", 46));
        addControl ("Wall Y-", makeSlider ("wallAbsCoeff_nY", 46));
        addControl ("Ceiling", makeSlider ("wallAbsCoeff_pZ", 46));
        addControl ("Floor", makeSlider ("wallAbsCoeff_nZ", 46));

        addNote ("Absorption coefficient: 0 = fully reflective, "
                 "1 = fully absorbing.");

        for (int i = 0; i < 4; ++i)
        {
            auto& button = presetButtons[i];
            button.setButtonText (materialPresets[(size_t) i].name);
            button.setTooltip (materialPresets[(size_t) i].description);
            button.onClick = [this, i]
            {
                const float value = materialPresets[(size_t) i].absorption;
                const char* ids[6] = { "wallAbsCoeff_pX", "wallAbsCoeff_nX",
                                       "wallAbsCoeff_pY", "wallAbsCoeff_nY",
                                       "wallAbsCoeff_pZ", "wallAbsCoeff_nZ" };
                for (const auto* id : ids)
                    processor.setParameterValue (id, value);

                if (onEdited)
                    onEdited();
            };

            presetStrip.add (button);
        }

        addControl ("Presets", presetStrip, 0, 30);
    }

    void buildActorSection()
    {
        studio::ActorInfo info;
        {
            const juce::ScopedLock sl (processor.getSceneLock());
            info = processor.getSceneModel().getActor (isReceiver, index);
        }

        const auto accent = getPathColour (isReceiver, index);
        addSection ("Actor  " + objectName (processor, isReceiver, index), accent);

        actorKindCombo.clear (juce::dontSendNotification);
        for (auto kind : studio::allActorKinds())
            actorKindCombo.addItem (studio::toString (kind), (int) kind + 1);
        actorKindCombo.setSelectedId ((int) info.kind + 1,
                                      juce::dontSendNotification);
        actorKindCombo.onChange = [this]
        {
            const auto kind = (studio::ActorKind) juce::jlimit (
                0, (int) studio::ActorKind::numKinds - 1,
                actorKindCombo.getSelectedId() - 1);

            editActor ([kind] (studio::ActorInfo& actor) { actor.kind = kind; },
                       true);
        };
        addControl ("Model", actorKindCombo);

        actorNameEditor.setText (info.name, false);
        actorNameEditor.setTextToShowWhenEmpty ("Unnamed",
                                                studio::Colours::textMuted);
        actorNameEditor.setFont (studio::uiFont (11.5f));
        actorNameEditor.setColour (juce::TextEditor::backgroundColourId,
                                   studio::Colours::surface);
        actorNameEditor.setColour (juce::TextEditor::textColourId,
                                   studio::Colours::text);
        actorNameEditor.setColour (juce::TextEditor::outlineColourId,
                                   studio::Colours::strokeStrong);
        actorNameEditor.setColour (juce::TextEditor::focusedOutlineColourId,
                                   studio::Colours::accent);
        actorNameEditor.setColour (juce::TextEditor::highlightColourId,
                                   studio::Colours::accentDim);
        actorNameEditor.onReturnKey = [this] { commitActorName(); };
        actorNameEditor.onFocusLost = [this] { commitActorName(); };

        Row nameRow;
        nameRow.kind = Row::Kind::control;
        nameRow.text = "Name";
        nameRow.control = &actorNameEditor;
        nameRow.height = 30;
        nameRow.labelWidth = 88;
        addAndMakeVisible (actorNameEditor);
        rows.push_back (nameRow);

        configureCustomSlider (yawSlider, -180.0, 180.0, 1.0, info.yawDegrees,
                               " deg", 50);
        yawSlider.onValueChange = [this]
        {
            const float value = (float) yawSlider.getValue();
            editActor ([value] (studio::ActorInfo& actor) { actor.yawDegrees = value; },
                       false);
        };
        yawSlider.onDragEnd = [this] { processor.markSceneDirty(); };
        addControl ("Facing", yawSlider);

        configureCustomSlider (scaleSlider, 0.4, 2.5, 0.05, info.scale,
                               " x", 50);
        scaleSlider.onValueChange = [this]
        {
            const float value = (float) scaleSlider.getValue();
            editActor ([value] (studio::ActorInfo& actor) { actor.scale = value; },
                       false);
        };
        scaleSlider.onDragEnd = [this] { processor.markSceneDirty(); };
        addControl ("Scale", scaleSlider);

        addControl ("Position X", makeSlider (roomAxisParameterID ('X')));
        addControl ("Position Y", makeSlider (roomAxisParameterID ('Y')));
        addControl ("Position Z", makeSlider (roomAxisParameterID ('Z')));

        addValue ("Directivity", "Omnidirectional");
        addNote ("Directivity and Doppler are not modelled by the current "
                 "image-source engine; the acoustic point is omnidirectional.");

        frameButton.onClick = [this]
        {
            if (onFrameRequested)
                onFrameRequested();
        };
        addControl ("", frameButton, 0, 30);
    }

    void commitActorName()
    {
        const juce::String name = actorNameEditor.getText().trim();
        editActor ([name] (studio::ActorInfo& actor) { actor.name = name; }, true);
    }

    void buildPathSection()
    {
        bool enabled = false;
        bool loop = false;
        size_t count = 0;
        double start = 0.0, end = 0.0;

        {
            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            const PathData& path = currentPath();
            enabled = path.enabled;
            loop = path.loop;
            count = path.keyframes.size();
            start = path.startTime;
            end = path.endTime;
        }

        addSection ("Motion path", getPathColour (isReceiver, index));

        pathEnabledToggle.setButtonText ("Path drives this object");
        pathEnabledToggle.setToggleState (enabled, juce::dontSendNotification);
        pathEnabledToggle.setEnabled (count > 0);
        pathEnabledToggle.onClick = [this]
        {
            const bool shouldEnable = pathEnabledToggle.getToggleState();
            editPath ([shouldEnable] (PathData& path)
                      {
                          path.enabled = shouldEnable && ! path.keyframes.empty();
                      },
                      true);

            refreshValues();
        };
        addControl ("", pathEnabledToggle, 0, 28);

        pathLoopToggle.setButtonText ("Loop path");
        pathLoopToggle.setToggleState (loop, juce::dontSendNotification);
        pathLoopToggle.setEnabled (count > 1);
        pathLoopToggle.onClick = [this]
        {
            const bool shouldLoop = pathLoopToggle.getToggleState();
            editPath ([shouldLoop] (PathData& path) { path.loop = shouldLoop; },
                      true);
        };
        addControl ("", pathLoopToggle, 0, 28);

        const double maxTime = juce::jmax (600.0, end + 60.0);
        const juce::String timeSuffix = " " + processor.getTimeUnitLabel();

        configureCustomSlider (pathStartSlider, 0.0, maxTime, 0.01, start,
                               timeSuffix, 62);
        configureCustomSlider (pathEndSlider, 0.0, maxTime, 0.01, end,
                               timeSuffix, 62);
        pathStartSlider.setTooltip ("Shift the whole path window in time. "
                                    "Double-click the value to type an exact time.");
        pathEndSlider.setTooltip ("Stretch the path to the new end time. "
                                  "Checkpoints keep their relative timing.");

        /* Moving Start translates the whole window (and with it every
           checkpoint) without stretching the motion; moving End keeps the
           start fixed and scales the keyframe timing to the new duration. */
        auto applyStart = [this]
        {
            const double startValue = pathStartSlider.getValue();
            double endValue = startValue + 10.0;

            editPath ([startValue, &endValue] (PathData& path)
                      {
                          const double duration = juce::jmax (0.1, path.duration());
                          path.startTime = startValue;
                          path.endTime = startValue + duration;
                          endValue = path.endTime;
                      },
                      false);

            if (endValue > pathEndSlider.getMaximum())
                pathEndSlider.setRange (0.0, endValue + 60.0, 0.01);

            pathEndSlider.setValue (endValue, juce::dontSendNotification);
        };

        auto applyEnd = [this]
        {
            const double startValue = pathStartSlider.getValue();
            double endValue = pathEndSlider.getValue();

            if (endValue <= startValue + 0.05)
            {
                endValue = startValue + 0.1;
                if (endValue > pathEndSlider.getMaximum())
                    pathEndSlider.setRange (0.0, endValue + 1.0, 0.01);

                pathEndSlider.setValue (endValue, juce::dontSendNotification);
            }

            editPath ([startValue, endValue] (PathData& path)
                      {
                          const double oldDuration = path.endTime - path.startTime;
                          path.startTime = startValue;
                          path.endTime = endValue;

                          if (path.keyframes.size() >= 2)
                              PathData::scaleTimes (path, oldDuration,
                                                    endValue - startValue);
                      },
                      false);
        };

        pathStartSlider.onValueChange = applyStart;
        pathEndSlider.onValueChange = applyEnd;
        pathStartSlider.onDragEnd = [this]
        {
            processor.markPathDirty();
            if (onEdited)
                onEdited();
        };
        pathEndSlider.onDragEnd = [this]
        {
            processor.markPathDirty();
            if (onEdited)
                onEdited();
        };

        addControl ("Start", pathStartSlider);
        addControl ("End", pathEndSlider);

        addValue ("Keyframes", juce::String ((int) count), &pathCountValue);
        addValue ("Range", studio::formatTime (start, processor.isTimeInPpq())
                             + " - "
                             + studio::formatTime (end, processor.isTimeInPpq()),
                  &pathRangeValue);

        clearPathButton.setTooltip ("Remove every checkpoint and stop the path "
                                    "from driving the object.");
        clearPathButton.setEnabled (count > 0 || enabled || loop);
        clearPathButton.onClick = [this]
        {
            bool changed = false;
            editPath ([&changed] (PathData& path)
                      {
                          if (! path.keyframes.empty() || path.enabled || path.loop)
                          {
                              path.keyframes.clear();
                              path.enabled = false;
                              path.loop = false;
                              changed = true;
                          }
                      },
                      true);

            if (! changed)
                return;

            if (onSelectionRequested)
                onSelectionRequested (isReceiver
                                          ? studio::Selection::receiver (index)
                                          : studio::Selection::source (index));

            if (onStructureChanged)
                onStructureChanged();
        };
        addControl ("", clearPathButton, 0, 30);

        if (count == 0)
            addNote ("Draw a spline in the canvas with the Draw Spline tool, "
                     "or generate a primitive from the tool rail.");
        else
            addNote ("Start shifts the whole path window, End stretches the "
                     "motion. Checkpoint times stay relative to the start.");
    }

    void buildCheckpointSection()
    {
        Keyframe keyframe;
        size_t count = 0;
        double start = 0.0;
        bool valid = false;

        {
            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            const PathData& path = currentPath();
            count = path.keyframes.size();
            start = path.startTime;
            if (selection.keyframe >= 0
                && selection.keyframe < (int) path.keyframes.size())
            {
                keyframe = path.keyframes[(size_t) selection.keyframe];
                valid = true;
            }
        }

        addSection ("Checkpoint " + juce::String (selection.keyframe + 1)
                      + " of " + juce::String ((int) count),
                    getPathColour (isReceiver, index));

        if (! valid)
        {
            addNote ("This checkpoint no longer exists. Select another node in "
                     "the canvas or the timeline.");
            return;
        }

        addValue ("Time",
                  studio::formatTime (start + keyframe.timeSeconds,
                                      processor.isTimeInPpq()),
                  &checkpointTimeValue);

        const float roomWidth = processor.getParameterFloat ("roomX");
        const float roomDepth = processor.getParameterFloat ("roomY");
        const float roomHeight = processor.getParameterFloat ("roomZ");

        configureCustomSlider (checkpointXSlider, 0.0, juce::jmax (0.1f, roomWidth), 0.01, keyframe.x, " m");
        configureCustomSlider (checkpointYSlider, 0.0, juce::jmax (0.1f, roomDepth), 0.01, keyframe.y, " m");
        configureCustomSlider (checkpointZSlider, 0.0, juce::jmax (0.1f, roomHeight), 0.01, keyframe.z, " m");

        auto writeAxis = [this] (int axis, double value, bool notifyHost)
        {
            editPath ([this, axis, value] (PathData& path)
                      {
                          if (selection.keyframe < 0
                              || selection.keyframe >= (int) path.keyframes.size())
                              return;

                          auto& keyframe = path.keyframes[(size_t) selection.keyframe];
                          const float roomX = processor.getParameterFloat ("roomX");
                          const float roomY = processor.getParameterFloat ("roomY");
                          const float roomZ = processor.getParameterFloat ("roomZ");
                          const float v = (float) value;

                          if (axis == 0)      keyframe.x = juce::jlimit (0.0f, roomX, v);
                          else if (axis == 1) keyframe.y = juce::jlimit (0.0f, roomY, v);
                          else                keyframe.z = juce::jlimit (0.0f, roomZ, v);

                          path.recomputeDefaultTangent (selection.keyframe);
                      },
                      notifyHost);
        };

        checkpointXSlider.onValueChange = [this, writeAxis]
        {
            writeAxis (0, checkpointXSlider.getValue(), false);
        };
        checkpointXSlider.onDragStart = [this] { editingCheckpointSliders = true; };
        checkpointXSlider.onDragEnd = [this]
        {
            editingCheckpointSliders = false;
            processor.markPathDirty();
        };

        checkpointYSlider.onValueChange = [this, writeAxis]
        {
            writeAxis (1, checkpointYSlider.getValue(), false);
        };
        checkpointYSlider.onDragStart = [this] { editingCheckpointSliders = true; };
        checkpointYSlider.onDragEnd = [this]
        {
            editingCheckpointSliders = false;
            processor.markPathDirty();
        };

        checkpointZSlider.onValueChange = [this, writeAxis]
        {
            writeAxis (2, checkpointZSlider.getValue(), false);
        };
        checkpointZSlider.onDragStart = [this] { editingCheckpointSliders = true; };
        checkpointZSlider.onDragEnd = [this]
        {
            editingCheckpointSliders = false;
            processor.markPathDirty();
        };

        addControl ("Node X", checkpointXSlider);
        addControl ("Node Y", checkpointYSlider);
        addControl ("Node Z", checkpointZSlider);

        configureCustomSlider (checkpointStopSlider, 0.0, 30.0, 0.05,
                               keyframe.stopTime, " s");

        checkpointStopSlider.onValueChange = [this]
        {
            const double newStop = checkpointStopSlider.getValue();

            editPath ([this, newStop] (PathData& path)
                      {
                          if (selection.keyframe < 0
                              || selection.keyframe >= (int) path.keyframes.size())
                              return;

                          auto& keyframe = path.keyframes[(size_t) selection.keyframe];
                          const double delta = newStop - (double) keyframe.stopTime;
                          keyframe.stopTime = (float) newStop;

                          if (std::abs (delta) > 1.0e-9)
                          {
                              for (int j = selection.keyframe + 1;
                                   j < (int) path.keyframes.size(); ++j)
                                  path.keyframes[(size_t) j].timeSeconds += delta;

                              path.endTime = juce::jmax (path.startTime + 0.1,
                                                         path.endTime + delta);
                          }
                      },
                      false);
        };
        checkpointStopSlider.onDragStart = [this]
        {
            editingCheckpointSliders = true;
        };
        checkpointStopSlider.onDragEnd = [this]
        {
            editingCheckpointSliders = false;
            processor.markPathDirty();
            if (onEdited)
                onEdited();
        };

        addControl ("Pause", checkpointStopSlider);

        deleteCheckpointButton.setTooltip ("Remove this checkpoint from the "
                                           "path (Delete).");
        deleteCheckpointButton.onClick = [this]
        {
            bool removed = false;

            editPath ([this, &removed] (PathData& path)
                      {
                          if (selection.keyframe < 0
                              || selection.keyframe >= (int) path.keyframes.size())
                              return;

                          path.keyframes.erase (path.keyframes.begin()
                                                + selection.keyframe);
                          removed = true;

                          if (path.keyframes.empty())
                          {
                              path.enabled = false;
                              path.loop = false;
                          }
                      },
                      true);

            if (! removed)
                return;

            if (onSelectionRequested)
                onSelectionRequested (isReceiver
                                          ? studio::Selection::receiver (index)
                                          : studio::Selection::source (index));

            if (onStructureChanged)
                onStructureChanged();
        };
        addControl ("", deleteCheckpointButton, 0, 30);

        addNote ("Node positions reset their curve tangents so the spline "
                 "stays smooth. Drag the diamond in the timeline to retime it.");
    }

    void buildCanvasSection()
    {
        studio::UiPreferences preferences;
        {
            const juce::ScopedLock sl (processor.getSceneLock());
            preferences = processor.getSceneModel().getUiPreferences();
        }

        addSection ("Canvas");

        showGridToggle.setButtonText ("Floor grid");
        showGridToggle.setToggleState (preferences.showGrid,
                                       juce::dontSendNotification);
        showGridToggle.onClick = [this]
        {
            const bool state = showGridToggle.getToggleState();
            editPreferences ([state] (studio::UiPreferences& prefs)
                             { prefs.showGrid = state; });
        };
        addControl ("", showGridToggle, 0, 28);

        showTimesToggle.setButtonText ("Checkpoint times");
        showTimesToggle.setToggleState (preferences.showTimes,
                                        juce::dontSendNotification);
        showTimesToggle.onClick = [this]
        {
            const bool state = showTimesToggle.getToggleState();
            editPreferences ([state] (studio::UiPreferences& prefs)
                             { prefs.showTimes = state; });
        };
        addControl ("", showTimesToggle, 0, 28);

        showLabelsToggle.setButtonText ("Actor labels");
        showLabelsToggle.setToggleState (preferences.showLabels,
                                         juce::dontSendNotification);
        showLabelsToggle.onClick = [this]
        {
            const bool state = showLabelsToggle.getToggleState();
            editPreferences ([state] (studio::UiPreferences& prefs)
                             { prefs.showLabels = state; });
        };
        addControl ("", showLabelsToggle, 0, 28);

        snapToggle.setButtonText ("Snap to 0.25 m");
        snapToggle.setToggleState (preferences.snap, juce::dontSendNotification);
        snapToggle.onClick = [this]
        {
            const bool state = snapToggle.getToggleState();
            editPreferences ([state] (studio::UiPreferences& prefs)
                             { prefs.snap = state; });
        };
        addControl ("", snapToggle, 0, 28);
    }

    void buildOutputSection()
    {
        addSection ("Output");

        addControl ("HOA order", makeCombo ("outputOrder"));
        addControl ("Channel order", makeCombo ("channelOrder"));
        addControl ("Normalisation", makeCombo ("normType"));

        addSeparator();

        addControl ("Reflections",
                    makeToggle ("enableReflections", "Image-source reflections"),
                    0, 30);

        auto& reflectionOrder = makeSlider ("maxReflectionOrder", 40);
        reflectionOrder.setNumDecimalPlacesToDisplay (0);
        addControl ("Max order", reflectionOrder);

        addNote ("Output order, channel order and normalisation are applied "
                 "to the encoded ambisonic bus.");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};

/* ------------------------------------------------------------------------- */

InspectorPanel::InspectorPanel (PluginProcessor& p)
    : processor (p)
{
    setOpaque (true);

    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (9);
    viewport.setViewedComponent (nullptr, false);
    addAndMakeVisible (viewport);

    rebuild();
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (studio::Colours::panel);

    auto header = getLocalBounds().removeFromTop (headerHeight);
    g.setColour (studio::Colours::bgRaised);
    g.fillRect (header);
    g.setColour (studio::Colours::stroke);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setColour (studio::Colours::textSecondary);
    g.setFont (studio::uiFont (9.5f, true));
    g.drawText ("ACOUSTIC INSPECTOR",
                header.reduced (sidePad, 0).withHeight (18).translated (0, 5),
                juce::Justification::centredLeft, false);

    g.setColour (studio::Colours::text);
    g.setFont (studio::uiFont (12.5f, true));
    g.drawText (getSelectionTitle(),
                header.reduced (sidePad, 0).withTrimmedTop (19)
                      .withHeight (20),
                juce::Justification::centredLeft, true);
}

void InspectorPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (headerHeight);
    viewport.setBounds (area);

    if (content != nullptr)
    {
        const int width = juce::jmax (160, viewport.getWidth() - 10);
        content->setSize (width, content->getPreferredHeight());
    }
}

void InspectorPanel::setSelection (const studio::Selection& newSelection)
{
    if (selection == newSelection)
        return;

    selection = newSelection;
    rebuild();
}

void InspectorPanel::refresh()
{
    if (content != nullptr)
        content->refreshValues();
}

void InspectorPanel::rebuild()
{
    const int scrollPosition = viewport.getViewPositionY();

    content.reset();
    viewport.setViewedComponent (nullptr, false);

    content = std::make_unique<Content> (processor, selection);
    content->onEdited = [this]
    {
        repaint();
        if (onEdited)
            onEdited();
    };
    content->onFrameRequested = [this]
    {
        if (onFrameRequested)
            onFrameRequested();
    };
    content->onStructureChanged = [this]
    {
        /* The handler runs from inside a button/control callback, so the
           rebuild has to wait until the current message has finished. */
        juce::Component::SafePointer<InspectorPanel> safe (this);

        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr)
                return;

            safe->rebuild();

            if (safe->onStructureChanged)
                safe->onStructureChanged();
        });
    };
    content->onSelectionRequested = [this] (studio::Selection newSelection)
    {
        juce::Component::SafePointer<InspectorPanel> safe (this);
        auto callback = onSelectionRequested;

        juce::MessageManager::callAsync ([safe, newSelection, callback]
        {
            if (safe != nullptr && callback)
                callback (newSelection);
        });
    };

    viewport.setViewedComponent (content.get(), false);

    const int width = juce::jmax (160, viewport.getWidth() - 10);
    content->setSize (width, content->getPreferredHeight());
    viewport.setViewPosition (0, scrollPosition);

    resized();
    repaint();
}

juce::String InspectorPanel::getSelectionTitle() const
{
    if (selection.isNone())
        return "Room & engine";

    const bool isReceiver = selection.isCheckpoint() ? selection.checkpointIsReceiver
                                                     : selection.isReceiver();
    const int index = juce::jlimit (0, studio::maxSceneObjects - 1, selection.index);

    juce::String title = objectName (processor, isReceiver, index);

    if (selection.isCheckpoint())
        title += "  /  Node " + juce::String (selection.keyframe + 1);

    return title;
}
