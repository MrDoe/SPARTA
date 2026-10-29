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

#include "StudioShell.h"

#include "../pathTimelineView.h"

namespace
{
    constexpr int topBarHeight = 48;
    constexpr int warningHeight = 22;
    constexpr int railWidth = 64;
    constexpr int inspectorWidth = 302;
    constexpr int sourceBarHeight = SourceBar::preferredHeight;
    constexpr int timelineDockHeight = 198;
    constexpr int timelineHeaderHeight = 28;

    /* One undo step per gesture. Continuous edits keep refreshing the timer,
       so a whole slider drag or node drag is captured as a single step. */
    constexpr juce::uint32 undoCoalesceMs = 180;
    constexpr size_t maxUndoSteps = 64;

    class ToolRailButton : public juce::Button
    {
    public:
        ToolRailButton (const juce::String& labelText, studio::Icon iconToUse)
            : juce::Button (labelText), label (labelText), icon (iconToUse)
        {
        }

        void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
        {
            const bool selected = (bool) getProperties().getWithDefault ("toolSelected",
                                                                         false);
            auto r = getLocalBounds().toFloat().reduced (1.0f);

            if (selected)
            {
                g.setColour (studio::Colours::accentDim);
                g.fillRoundedRectangle (r, 5.0f);
                g.setColour (studio::Colours::accent);
                g.fillRoundedRectangle (r.withWidth (2.0f), 1.0f);
            }
            else if (isHovered || isDown)
            {
                g.setColour (studio::Colours::surface.withAlpha (0.70f));
                g.fillRoundedRectangle (r, 5.0f);
            }

            const auto iconColour = isEnabled()
                                        ? (selected ? studio::Colours::accent
                                                    : studio::Colours::textSecondary)
                                        : studio::Colours::textDisabled;

            const float iconX = r.getCentreX() - 9.0f;
            const float iconY = r.getY() + 7.0f;
            g.setColour (iconColour);
            g.strokePath (studio::makeIcon (icon, 18.0f),
                          juce::PathStrokeType (1.5f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded),
                          juce::AffineTransform::translation (iconX, iconY));

            g.setColour (isEnabled()
                             ? (selected ? studio::Colours::text
                                         : studio::Colours::textMuted)
                             : studio::Colours::textDisabled);
            g.setFont (studio::uiFont (8.5f, selected));
            g.drawFittedText (label,
                              getLocalBounds().withTrimmedTop (28)
                                              .withTrimmedBottom (2)
                                              .reduced (2, 0),
                              juce::Justification::centredTop, 2, 0.8f);

            if (hasKeyboardFocus (false))
            {
                g.setColour (studio::Colours::accent);
                g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);
            }
        }

    private:
        juce::String label;
        studio::Icon icon;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolRailButton)
    };
}

void StudioShell::EditorUndoHistory::reset (PluginProcessor& processor)
{
    undoStack.clear();
    redoStack.clear();
    lastState = processor.captureEditorState();
}

void StudioShell::EditorUndoHistory::sync (PluginProcessor& processor)
{
    lastState = processor.captureEditorState();
}

void StudioShell::EditorUndoHistory::checkpoint (PluginProcessor& processor)
{
    auto current = processor.captureEditorState();

    if (lastState.isValid() && current.isEquivalentTo (lastState))
        return;

    if (lastState.isValid())
    {
        undoStack.push_back (lastState);
        if (undoStack.size() > maxUndoSteps)
            undoStack.erase (undoStack.begin());

        /* A fresh edit invalidates the redo branch. */
        redoStack.clear();
    }

    lastState = current;
}

bool StudioShell::EditorUndoHistory::undo (PluginProcessor& processor)
{
    auto current = processor.captureEditorState();

    /* Capture an edit that has not reached the coalescing timer yet. */
    if (lastState.isValid() && ! current.isEquivalentTo (lastState))
    {
        undoStack.push_back (lastState);
        if (undoStack.size() > maxUndoSteps)
            undoStack.erase (undoStack.begin());
        redoStack.clear();
        lastState = current;
    }

    if (undoStack.empty())
        return false;

    auto target = undoStack.back();
    undoStack.pop_back();

    redoStack.push_back (current);
    if (redoStack.size() > maxUndoSteps)
        redoStack.erase (redoStack.begin());

    processor.restoreEditorState (target);
    lastState = processor.captureEditorState();
    return true;
}

bool StudioShell::EditorUndoHistory::redo (PluginProcessor& processor)
{
    auto current = processor.captureEditorState();

    /* If an edit happened after the undo, the redo branch is gone. */
    if (lastState.isValid() && ! current.isEquivalentTo (lastState))
    {
        checkpoint (processor);
        return false;
    }

    if (redoStack.empty())
        return false;

    auto target = redoStack.back();
    redoStack.pop_back();

    undoStack.push_back (current);
    if (undoStack.size() > maxUndoSteps)
        undoStack.erase (undoStack.begin());

    processor.restoreEditorState (target);
    lastState = processor.captureEditorState();
    return true;
}

StudioShell::StudioShell (PluginProcessor& p)
    : processor (p),
      viewport (p),
      inspector (p),
      sourceBar (p)
{
    hAmbi = processor.getFXHandle();

    /* Restore the text scale before any of the panel contents are rebuilt. */
    {
        const juce::ScopedLock sl (processor.getSceneLock());
        studio::uiFontScale() = juce::jlimit (
            0.8f, 1.35f,
            processor.getSceneModel().getUiPreferences().fontScale);
    }

    setOpaque (true);
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (viewport);
    addAndMakeVisible (inspector);
    addAndMakeVisible (sourceBar);

    timeline = std::make_unique<pathTimelineView> (p);
    addAndMakeVisible (timeline.get());
    timeline->onPreviewRequested = [this] (double t) { setPreview (t, true, true); };
    timeline->onSelectionChanged = [this] (studio::Selection newSelection)
    {
        applySelection (newSelection, true);
    };
    timeline->onPathEdited = [this]
    {
        inspector.refresh();
        viewport.refresh();
        repaint();
        noteEdit();
    };

    trackChooser.addListener (this);
    trackChooser.setTooltip ("Load an audio file to show its waveform in the "
                             "timeline.");
    addAndMakeVisible (trackChooser);

    const auto savedTrack = processor.getTrackWaveformSnapshot();
    if (savedTrack.filePath.isNotEmpty())
        trackChooser.setCurrentFile (juce::File (savedTrack.filePath), false,
                                     juce::dontSendNotification);

    focusButton.onClick = [this]
    {
        setFocusMode (! focusMode);
        viewport.grabKeyboardFocus();
    };
    addAndMakeVisible (focusButton);

    fullscreenButton.onClick = [this] { toggleFullscreen(); };
    addAndMakeVisible (fullscreenButton);

    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

#if JucePlugin_Build_Standalone
    playButton.setToggleState (false, juce::dontSendNotification);
    playButton.onClick = [this]
    {
        if (internalPlaying)
        {
            internalPlaying = false;
            playButton.setToggleState (false, juce::dontSendNotification);
        }
        else
        {
            internalPlaying = true;
            playButton.setToggleState (true, juce::dontSendNotification);
            if (previewTime <= 0.0)
                previewTime = processor.getCurrentHostTime();
            setPreview (previewTime, true, true);
        }
    };
    addAndMakeVisible (playButton);

    stopButton.onClick = [this]
    {
        internalPlaying = false;
        playButton.setToggleState (false, juce::dontSendNotification);
        setPreview (0.0, false, true);
        manualPreview = false;
    };
    addAndMakeVisible (stopButton);
#endif

    buildRail();

    viewport.onSelectionChanged = [this] (studio::Selection newSelection)
    {
        applySelection (newSelection, false);
    };
    viewport.onSourceAdded = [this]
    {
        sourceBar.refresh();
        applySelection (viewport.getSelection(), false);
        refreshSummary();
        noteEdit();
    };
    viewport.onEditCommitted = [this] { noteEdit(); };
    viewport.onToolChanged = [this] (Viewport3D::Tool) { updateRailSelection(); };

    viewport.onPathEdited = [this]
    {
        inspector.rebuild();
        sourceBar.refresh();
        viewport.refresh();
        if (timeline != nullptr)
            timeline->repaint();

        refreshSummary();
        repaint();
        noteEdit();
    };

    inspector.onEdited = [this]
    {
        syncRenderOptions();
        sourceBar.refresh();
        viewport.refresh();
        refreshSummary();
        repaint();
        noteEdit();
    };
    inspector.onFrameRequested = [this] { viewport.frameSelection(); };
    inspector.onStructureChanged = [this]
    {
        sourceBar.refresh();
        viewport.refresh();
        refreshSummary();
        repaint();
        noteEdit();
    };
    inspector.onSelectionRequested = [this] (studio::Selection newSelection)
    {
        applySelection (newSelection, true);
    };

    sourceBar.onSelectionChanged = [this] (studio::Selection newSelection)
    {
        applySelection (newSelection, true);
    };
    sourceBar.onSceneChanged = [this]
    {
        viewport.refresh();
        inspector.refresh();
        refreshSummary();
        repaint();
        noteEdit();
    };

    if (processor.getParameterInt ("numSources") > 0)
        applySelection (studio::Selection::source (0), true);
    else
        applySelection (studio::Selection::none(), true);

    lastSummary = buildSummary();
    lastOutputSummary = buildOutputSummary();

    updateWarning();
    syncRenderOptions();
    updateRailSelection();

    undoHistory.reset (processor);
    lastStateRestoreCounter = processor.getStateRestoreCounter();

    /* The inspector content was constructed before the saved font scale was
       applied, so rebuild it once with the restored scale. */
    if (std::abs (studio::uiFontScale() - 1.0f) > 1.0e-3f)
        inspector.rebuild();

    startTimer (80);
}

StudioShell::~StudioShell()
{
    stopTimer();
    trackChooser.removeListener (this);
    setLookAndFeel (nullptr);
}

/* ------------------------------------------------------------------------- */

void StudioShell::paint (juce::Graphics& g)
{
    g.fillAll (studio::Colours::bg);

    /* Top bar */
    g.setColour (studio::Colours::bgRaised);
    g.fillRect (topBarArea);
    g.setColour (studio::Colours::stroke);
    g.drawHorizontalLine (topBarArea.getBottom() - 1, 0.0f, (float) getWidth());

    if (! brandArea.isEmpty())
    {
        g.setColour (studio::Colours::textMuted);
        g.setFont (studio::uiFont (9.0f, true));
        g.drawText ("SPARTA", brandArea.getX(), brandArea.getY() + 7,
                    54, 14, juce::Justification::centredLeft, false);

        g.setColour (studio::Colours::strokeStrong);
        g.drawVerticalLine (brandArea.getX() + 56,
                            (float) brandArea.getCentreY() - 9.0f,
                            (float) brandArea.getCentreY() + 9.0f);

        g.setColour (studio::Colours::time);
        g.setFont (studio::uiFont (13.0f, true));
        g.drawText ("AmbiRoomSim",
                    brandArea.getX() + 66, brandArea.getY() + 5,
                    brandArea.getWidth() - 66, 20,
                    juce::Justification::centredLeft, false);

        g.setColour (studio::Colours::textMuted);
        g.setFont (studio::uiFont (9.0f));
        g.drawText ("3D spatial studio  ·  v" + juce::String (JucePlugin_VersionString),
                    brandArea.getX() + 66, brandArea.getY() + 24,
                    brandArea.getWidth() - 66, 12,
                    juce::Justification::centredLeft, true);
    }

    if (summaryArea.getWidth() > 120)
    {
        g.setColour (studio::Colours::textSecondary);
        g.setFont (studio::uiFont (10.0f));
        g.drawText (lastSummary, summaryArea.withHeight (24).translated (0, 4),
                    juce::Justification::centredLeft, true);

        g.setColour (studio::Colours::textMuted);
        g.setFont (studio::uiFont (9.5f));
        g.drawText (lastOutputSummary,
                    summaryArea.withTrimmedTop (24).withHeight (16),
                    juce::Justification::centredLeft, true);

        if (focusMode)
        {
            const float chipWidth = 64.0f;
            juce::Rectangle<float> chip ((float) summaryArea.getRight() - chipWidth,
                                         (float) summaryArea.getCentreY() - 8.0f,
                                         chipWidth, 16.0f);
            studio::drawChip (g, chip, "FOCUS", studio::Colours::accentDim,
                      studio::Colours::accent.withAlpha (0.9f), studio::Colours::text, 9.0f);
        }
    }

    if (! timeArea.isEmpty())
    {
        const bool preview = previewActive;
        g.setColour (preview ? studio::Colours::time : studio::Colours::textMuted);
        g.setFont (studio::uiFont (8.5f, true));
        g.drawText (preview ? "PREVIEW" : "HOST",
                    timeArea.getX(), timeArea.getY() + 9,
                    timeArea.getWidth(), 12,
                    juce::Justification::centredRight, false);

        g.setColour (preview ? studio::Colours::time : studio::Colours::text);
        g.setFont (studio::uiFont (12.0f, true));
        g.drawText (studio::formatTime (preview ? previewTime
                                        : processor.getCurrentHostTime(),
                                processor.isTimeInPpq()),
                    timeArea.getX(), timeArea.getY() + 21,
                    timeArea.getWidth(), 18,
                    juce::Justification::centredRight, false);
    }

    /* Warning strip */
    if (currentWarning != Warning::none && ! warningArea.isEmpty())
    {
        g.setColour (studio::Colours::warning.withAlpha (0.14f));
        g.fillRect (warningArea);
        g.setColour (studio::Colours::warning);
        g.fillRect (warningArea.withWidth (3));

        g.setFont (studio::uiFont (10.5f, true));
        g.drawText (warningText (currentWarning),
                    warningArea.withTrimmedLeft (14).withTrimmedRight (12),
                    juce::Justification::centredLeft, true);
    }

    /* Tool rail */
    if (! railArea.isEmpty())
    {
        g.setColour (studio::Colours::bgRaised);
        g.fillRect (railArea);
        g.setColour (studio::Colours::stroke);
        g.drawVerticalLine (railArea.getRight() - 1,
                            (float) railArea.getY(),
                            (float) railArea.getBottom());
    }

    /* Timeline dock chrome (the timeline component itself paints below the
       header). */
    if (! timelineDockArea.isEmpty() && timelineVisible && ! focusMode)
    {
        g.setColour (studio::Colours::panel);
        g.fillRect (timelineDockArea);
        g.setColour (studio::Colours::stroke);
        g.drawHorizontalLine (timelineDockArea.getY(), 0.0f, (float) getWidth());

        g.setColour (studio::Colours::bgRaised);
        g.fillRect (timelineHeaderArea);
        g.setColour (studio::Colours::stroke);
        g.drawHorizontalLine (timelineHeaderArea.getBottom() - 1,
                              0.0f, (float) getWidth());

        g.setColour (studio::Colours::textSecondary);
        g.setFont (studio::uiFont (9.5f, true));
        g.drawText ("TIMELINE & MOTION EDITOR",
                    timelineHeaderArea.getX() + 12,
                    timelineHeaderArea.getY() + 2,
                    230, 22,
                    juce::Justification::centredLeft, false);

        g.setColour (studio::Colours::textMuted);
        g.setFont (studio::uiFont (9.0f));
        g.drawText ("Click to preview  ·  drag diamonds to retime",
                    timelineHeaderArea.getX() + 248,
                    timelineHeaderArea.getY() + 2,
                    juce::jmax (0, trackChooser.getX()
                                      - timelineHeaderArea.getX() - 416),
                    22,
                    juce::Justification::centredLeft, true);

        if (trackChooser.isVisible())
        {
            const auto previewRect = juce::Rectangle<int> (
                juce::jmax (timelineHeaderArea.getX() + 240,
                            trackChooser.getX() - 168),
                timelineHeaderArea.getY() + 3, 156, 20);

            g.setColour (studio::Colours::textMuted);
            g.setFont (studio::uiFont (9.0f, true));
            g.drawText ("PREVIEW",
                        previewRect.getX(), previewRect.getY(),
                        52, previewRect.getHeight(),
                        juce::Justification::centredLeft, false);

            g.setColour (previewActive ? studio::Colours::time : studio::Colours::textSecondary);
            g.setFont (studio::uiFont (10.0f, true));
            g.drawText (previewActive ? studio::formatTime (previewTime,
                                                    processor.isTimeInPpq())
                                      : juce::String ("off"),
                        previewRect.getX() + 50, previewRect.getY(),
                        previewRect.getWidth() - 50, previewRect.getHeight(),
                        juce::Justification::centredRight, false);
        }
    }
}

void StudioShell::resized()
{
    auto area = getLocalBounds();

    topBarArea = area.removeFromTop (topBarHeight);

    /* Top bar children. */
    fullscreenButton.setBounds (topBarArea.getRight() - 12 - 30,
                                topBarArea.getY() + 9, 30, 30);
    focusButton.setBounds (fullscreenButton.getX() - 6 - 30,
                           topBarArea.getY() + 9, 30, 30);
#if JucePlugin_Build_Standalone
    playButton.setBounds (focusButton.getX() - 6 - 30,
                          topBarArea.getY() + 9, 30, 30);
    stopButton.setBounds (playButton.getX() - 6 - 30,
                         topBarArea.getY() + 9, 30, 30);

    timeArea = { stopButton.getX() - 138, topBarArea.getY(),
                 126, topBarHeight };
#else
    /* No transport buttons when the standalone wrapper is not built. */
    timeArea = { focusButton.getX() - 138, topBarArea.getY(),
                 126, topBarHeight };
#endif
    brandArea = { 12, topBarArea.getY(), 262, topBarHeight };
    summaryArea = { brandArea.getRight() + 6, topBarArea.getY(),
                    juce::jmax (0, timeArea.getX() - brandArea.getRight() - 18),
                    topBarHeight };

    if (currentWarning != Warning::none)
        warningArea = area.removeFromTop (warningHeight);
    else
        warningArea = {};

    /* The timeline spans the full width, like a DAW bottom dock. */
    if (timelineVisible && ! focusMode)
        timelineDockArea = area.removeFromBottom (timelineDockHeight);
    else
        timelineDockArea = {};

    railArea = area.removeFromLeft (railWidth);

    if (inspectorVisible && ! focusMode)
        inspectorArea = area.removeFromRight (inspectorWidth);
    else
        inspectorArea = {};

    if (! focusMode)
        sourceBarArea = area.removeFromBottom (sourceBarHeight);
    else
        sourceBarArea = {};

    canvasArea = area;

    /* Rail buttons. */
    int railY = railArea.getY() + 10;
    const int railX = railArea.getX() + 8;
    for (auto* button : railButtons)
    {
        button->setBounds (railX, railY, railWidth - 16, 48);
        railY += 52;
    }

    settingsButton.setBounds (railX, railArea.getBottom() - 54,
                              railWidth - 16, 44);

    viewport.setBounds (canvasArea);
    inspector.setBounds (inspectorArea);
    sourceBar.setBounds (sourceBarArea);
    inspector.setVisible (! inspectorArea.isEmpty());
    sourceBar.setVisible (! sourceBarArea.isEmpty());

    if (! timelineDockArea.isEmpty())
    {
        timelineHeaderArea = timelineDockArea.removeFromTop (timelineHeaderHeight);
        timeline->setBounds (timelineDockArea);
        timeline->setVisible (true);

        const int chooserWidth = juce::jlimit (160, 280,
                                               timelineHeaderArea.getWidth() / 4);
        trackChooser.setBounds (timelineHeaderArea.getRight() - chooserWidth - 12,
                                timelineHeaderArea.getY() + 3,
                                chooserWidth, 21);
        trackChooser.setVisible (true);
    }
    else
    {
        timelineHeaderArea = {};
        timeline->setVisible (false);
        trackChooser.setVisible (false);
    }

    repaint();
}

bool StudioShell::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();

    if (mods.isCommandDown() && ! mods.isAltDown())
    {
        const int code = key.getKeyCode();

        if (code == 'Z' || code == 'z')
        {
            if (mods.isShiftDown())
                redo();
            else
                undo();

            return true;
        }

        if (code == 'Y' || code == 'y')
        {
            redo();
            return true;
        }
    }

    if (key == juce::KeyPress (juce::KeyPress::escapeKey))
    {
        if (isFullScreenMode())
        {
            setFullScreenMode (false);
            return true;
        }

        if (focusMode)
        {
            setFocusMode (false);
            return true;
        }

        applySelection (studio::Selection::none(), true);
        return true;
    }

    const auto character = key.getTextCharacter();
    if (character == 'p' || character == 'P')
    {
        setPreview (previewTime > 0.0 ? previewTime
                                      : processor.getCurrentHostTime(),
                    ! previewActive, true);
        return true;
    }

    if (character == 'v' || character == 'V')
    {
        setTool (Viewport3D::Tool::select);
        return true;
    }

    if (character == 'a' || character == 'A')
    {
        setTool (Viewport3D::Tool::addSource);
        return true;
    }

    if (character == 'b' || character == 'B')
    {
        setTool (Viewport3D::Tool::drawSpline);
        return true;
    }

    if (character == 'o' || character == 'O')
    {
        setTool (Viewport3D::Tool::orbit);
        return true;
    }

    if (character == 'y' || character == 'Y')
    {
        setTool (Viewport3D::Tool::fly);
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------------- */

void StudioShell::setFocusMode (bool shouldBeFocused)
{
    if (focusMode == shouldBeFocused)
        return;

    focusMode = shouldBeFocused;
    focusButton.setSelected (focusMode);
    resized();

    if (focusMode)
        viewport.grabKeyboardFocus();
}

void StudioShell::buildRail()
{
    auto addButton = [this] (studio::Icon icon, const juce::String& label,
                             Viewport3D::Tool tool, const juce::String& tooltip,
                             bool enabled) -> juce::Button*
    {
        auto* button = new ToolRailButton (label, icon);
        button->getProperties().set ("tool", (int) tool);
        button->setTooltip (tooltip);
        button->setEnabled (enabled);
        button->onClick = [this, button]
        {
            setTool ((Viewport3D::Tool) (int) button->getProperties()["tool"]);
        };

        addAndMakeVisible (button);
        railButtons.add (button);
        return button;
    };

    addButton (studio::Icon::cursor, "Select", Viewport3D::Tool::select,
               "Select and move sources/listeners (V)", true);
    addButton (studio::Icon::plus, "Add Source", Viewport3D::Tool::addSource,
               "Click the floor in the canvas to place a source (A)", true);
    addButton (studio::Icon::bezier, "Draw Spline", Viewport3D::Tool::drawSpline,
               "Click in the canvas to add checkpoints to the selected path (B)", true);
    primitivesButton = addButton (studio::Icon::shapes, "Primitives",
                                  Viewport3D::Tool::primitives,
                                  "Generate a line, circle or figure-eight path "
                                  "for the selected object", true);
    primitivesButton->onClick = [this] { showPrimitivesMenu(); };

    addButton (studio::Icon::orbit, "Orbit", Viewport3D::Tool::orbit,
               "Drag to orbit the camera (O)", true);
    addButton (studio::Icon::fly, "Fly-through", Viewport3D::Tool::fly,
               "Drag to look, W A S D / Q E to fly (Y)", true);
}

void StudioShell::setTool (Viewport3D::Tool tool)
{
    viewport.setTool (tool);
    updateRailSelection();
    viewport.grabKeyboardFocus();
}

void StudioShell::updateRailSelection()
{
    for (auto* button : railButtons)
    {
        const auto tool = (Viewport3D::Tool) (int) button->getProperties()["tool"];
        button->getProperties().set ("toolSelected", tool == viewport.getTool());
        button->repaint();
    }
}

void StudioShell::applySelection (const studio::Selection& newSelection,
                                  bool updateViewport)
{
    selection = newSelection;

    if (updateViewport)
        viewport.setSelection (selection, false);

    inspector.setSelection (selection);
    sourceBar.setSelection (selection);

    if (timeline != nullptr)
        timeline->setSelection (selection);

    repaint (topBarArea);
}

void StudioShell::setPreview (double time, bool active, bool manual)
{
    previewTime = time;
    previewActive = active;
    if (manual)
        manualPreview = active;

    viewport.setPreviewTime (previewTime, previewActive);
    timeline->setPreviewTime (previewTime, previewActive);

    viewport.refresh();
    repaint (topBarArea);
    repaint (timelineHeaderArea);
}

void StudioShell::syncRenderOptions()
{
    studio::UiPreferences preferences;
    {
        const juce::ScopedLock sl (processor.getSceneLock());
        preferences = processor.getSceneModel().getUiPreferences();
    }

    auto options = viewport.getRenderOptions();

    if (options.showGrid != preferences.showGrid
        || options.showTimes != preferences.showTimes
        || options.showLabels != preferences.showLabels
        || options.snap != preferences.snap)
    {
        options.showGrid = preferences.showGrid;
        options.showTimes = preferences.showTimes;
        options.showLabels = preferences.showLabels;
        options.snap = preferences.snap;
        viewport.setRenderOptions (options);
    }
}

void StudioShell::updateWarning()
{
    if (hAmbi == nullptr)
        return;

    Warning warning = Warning::none;

    if (processor.getCurrentNumInputs() < ambi_roomsim_getNumSources (hAmbi))
        warning = Warning::inputChannels;
    else if (processor.getCurrentNumOutputs() < ambi_roomsim_getNSHrequired (hAmbi))
        warning = Warning::outputChannels;
    else if (processor.getCurrentBlockSize() != ambi_roomsim_getFrameSize())
        warning = Warning::frameSize;

    if (warning == currentWarning)
        return;

    currentWarning = warning;
    resized();
    repaint();
}

void StudioShell::toggleFullscreen()
{
    if (isFullScreenMode())
    {
        setFullScreenMode (false);
        return;
    }

    if (getPeer() == nullptr)
    {
        setFocusMode (true);
        return;
    }

    setFullScreenMode (true);

    /* Hosts that do not support a true fullscreen editor fall back to the
       focus layout, which gives the canvas the whole window. */
    if (! isFullScreenMode())
        setFocusMode (true);
}

bool StudioShell::isFullScreenMode() const
{
    if (auto* window = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()))
        return window->isFullScreen();

    if (auto* peer = getPeer())
        return peer->isFullScreen();

    return false;
}

void StudioShell::setFullScreenMode (bool shouldBeFullScreen)
{
    if (auto* window = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()))
    {
        window->setFullScreen (shouldBeFullScreen);
        return;
    }

    if (auto* peer = getPeer())
        peer->setFullScreen (shouldBeFullScreen);
}

void StudioShell::showPrimitivesMenu()
{
    const auto target = viewport.getSelection();
    const bool hasTarget = target.isActor() || target.isCheckpoint();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Generate path");
    menu.addItem (1, "Line across room", hasTarget);
    menu.addItem (2, "Circle (loop)", hasTarget);
    menu.addItem (3, "Figure eight (loop)", hasTarget);
    menu.addSeparator();
    menu.addItem (4, "Clear path", hasTarget);

    const juce::Component::SafePointer<StudioShell> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (primitivesButton != nullptr
                                                      ? primitivesButton
                                                      : &settingsButton),
                        [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result >= 1 && result <= 3)
        {
            safe->viewport.createPrimitive (
                (Viewport3D::Primitive) (result - 1));
        }
        else if (result == 4)
        {
            safe->viewport.clearSelectedPath();
        }

        safe->viewport.grabKeyboardFocus();
    });
}

void StudioShell::showSettingsMenu()
{
    /* Make any just-finished edit visible to the undo entries in this menu. */
    flushUndoCheckpoint();

    float currentScale = 1.0f;
    {
        const juce::ScopedLock sl (processor.getSceneLock());
        currentScale = processor.getSceneModel().getUiPreferences().fontScale;
    }

    juce::PopupMenu fontMenu;
    const float fontScales[] = { 0.9f, 1.0f, 1.1f, 1.2f, 1.3f };
    const char* fontLabels[] = { "90%", "100%", "110%", "120%", "130%" };
    for (int i = 0; i < 5; ++i)
        fontMenu.addItem (20 + i, fontLabels[i], true,
                          std::abs (currentScale - fontScales[i]) < 0.001f);

    juce::PopupMenu menu;
    menu.addItem (1, "Show inspector", true, inspectorVisible);
    menu.addItem (2, "Show timeline", true, timelineVisible);
    menu.addSeparator();
    menu.addItem (3, "Time: seconds", true, ! processor.isTimeInPpq());
    menu.addItem (4, "Time: bars (PPQ)", true, processor.isTimeInPpq());
    menu.addSeparator();
    menu.addItem (5, "Frame room");
    menu.addItem (6, "Focus mode", true, focusMode);
    menu.addItem (7, "Fullscreen", true, isFullScreenMode());
    menu.addSeparator();
    menu.addItem (8, "Undo (Ctrl+Z)", undoHistory.canUndo());
    menu.addItem (9, "Redo (Ctrl+Shift+Z)", undoHistory.canRedo());
    menu.addSeparator();
    menu.addSubMenu ("Font size", fontMenu);

    const juce::Component::SafePointer<StudioShell> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        switch (result)
        {
            case 1:
                safe->inspectorVisible = ! safe->inspectorVisible;
                safe->resized();
                break;

            case 2:
                safe->timelineVisible = ! safe->timelineVisible;
                safe->resized();
                break;

            case 3:
            case 4:
                safe->processor.setParameterValue ("pathTimeMode",
                                                   result == 3 ? 0 : 1);
                safe->inspector.rebuild();
                safe->viewport.refresh();
                safe->timeline->repaint();
                safe->repaint();
                safe->noteEdit();
                break;

            case 5:
                safe->viewport.frameRoom();
                break;

            case 6:
                safe->setFocusMode (! safe->focusMode);
                break;

            case 7:
                safe->toggleFullscreen();
                break;

            case 8:
                safe->undo();
                break;

            case 9:
                safe->redo();
                break;

            default:
                if (result >= 20 && result <= 24)
                {
                    static const float scales[] = { 0.9f, 1.0f, 1.1f, 1.2f, 1.3f };
                    safe->setFontScale (scales[result - 20]);
                }
                break;
        }

        safe->viewport.grabKeyboardFocus();
    });
}

void StudioShell::noteEdit()
{
    undoCheckpointPending = true;
    lastEditTimeMs = juce::Time::getMillisecondCounter();
}

void StudioShell::flushUndoCheckpoint()
{
    if (! undoCheckpointPending)
        return;

    undoCheckpointPending = false;
    undoHistory.checkpoint (processor);
}

void StudioShell::undo()
{
    flushUndoCheckpoint();

    if (undoHistory.undo (processor))
        applyRestoredState();
}

void StudioShell::redo()
{
    flushUndoCheckpoint();

    if (undoHistory.redo (processor))
        applyRestoredState();
}

void StudioShell::applyRestoredState()
{
    studio::UiPreferences preferences;
    {
        const juce::ScopedLock sl (processor.getSceneLock());
        preferences = processor.getSceneModel().getUiPreferences();
    }

    studio::uiFontScale() = juce::jlimit (0.8f, 1.35f, preferences.fontScale);

    syncRenderOptions();
    inspector.rebuild();
    sourceBar.refresh();
    viewport.refresh();

    if (timeline != nullptr)
        timeline->repaint();

    refreshSummary();
    sanitizeSelection();
    repaint();
}

void StudioShell::handleHostStateRestore()
{
    const int counter = processor.getStateRestoreCounter();

    if (counter == lastStateRestoreCounter)
        return;

    lastStateRestoreCounter = counter;
    undoCheckpointPending = false;

    /* An external state change starts a new history; undoing across a host
       session load or host undo would be surprising and could resurrect
       stale parameter values. */
    undoHistory.reset (processor);
    applyRestoredState();
}

void StudioShell::sanitizeSelection()
{
    if (selection.isNone())
        return;

    const bool selectionIsReceiver = selection.isReceiver()
        || (selection.isCheckpoint() && selection.checkpointIsReceiver);
    const int limit = selectionIsReceiver ? processor.getParameterInt ("numReceivers")
                                          : processor.getParameterInt ("numSources");

    if (limit <= 0)
    {
        applySelection (studio::Selection::none(), true);
        return;
    }

    if (selection.index >= limit)
    {
        applySelection (selectionIsReceiver
                            ? studio::Selection::receiver (limit - 1)
                            : studio::Selection::source (limit - 1),
                        true);
        return;
    }

    if (! selection.isCheckpoint())
        return;

    size_t count = 0;
    {
        const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
        const PathData& path = selectionIsReceiver
            ? processor.getPathBank().getReceiverPath (selection.index)
            : processor.getPathBank().getSourcePath (selection.index);
        count = path.keyframes.size();
    }

    if (selection.keyframe < 0 || selection.keyframe >= (int) count)
    {
        applySelection (selectionIsReceiver
                            ? studio::Selection::receiver (selection.index)
                            : studio::Selection::source (selection.index),
                        true);
    }
}

void StudioShell::setFontScale (float scale)
{
    scale = juce::jlimit (0.8f, 1.35f, scale);

    /* Font size is a UI preference, not a document edit: keep it out of the
       undo history by synchronising the baseline after applying it. */
    flushUndoCheckpoint();

    {
        const juce::ScopedLock sl (processor.getSceneLock());
        processor.getSceneModel().getUiPreferences().fontScale = scale;
    }

    processor.markSceneDirty();
    studio::uiFontScale() = scale;

    inspector.rebuild();
    sourceBar.refresh();
    viewport.refresh();

    if (timeline != nullptr)
        timeline->repaint();

    resized();
    repaint();

    undoHistory.sync (processor);
}

juce::String StudioShell::buildSummary() const
{
    auto& p = const_cast<PluginProcessor&> (processor);

    const float width = p.getParameterFloat ("roomX");
    const float depth = p.getParameterFloat ("roomY");
    const float height = p.getParameterFloat ("roomZ");
    const int sources = p.getParameterInt ("numSources");
    const int receivers = p.getParameterInt ("numReceivers");

    juce::String summary;
    summary << "Room " << juce::String (width, 1) << " x "
            << juce::String (depth, 1) << " x "
            << juce::String (height, 1) << " m"
            << "   /   " << sources << (sources == 1 ? " source" : " sources")
            << "   /   " << receivers << (receivers == 1 ? " listener" : " listeners");
    return summary;
}

juce::String StudioShell::buildOutputSummary() const
{
    auto* orderParameter = dynamic_cast<juce::AudioParameterChoice*> (
        const_cast<PluginProcessor&> (processor).parameters.getParameter ("outputOrder"));

    const juce::String order = orderParameter != nullptr
                                   ? orderParameter->getCurrentChoiceName()
                                   : juce::String ("3rd order");

    juce::String summary;
    summary << order << " HOA";

    if (processor.getParameterBool ("enableReflections"))
        summary << "  /  image-source reflections, order "
                << processor.getParameterInt ("maxReflectionOrder");
    else
        summary << "  /  reflections off";

    return summary;
}

void StudioShell::refreshSummary()
{
    const auto summary = buildSummary();
    const auto output = buildOutputSummary();

    if (summary != lastSummary || output != lastOutputSummary)
    {
        lastSummary = summary;
        lastOutputSummary = output;
        repaint (topBarArea);
    }
}

juce::String StudioShell::warningText (Warning warning)
{
    switch (warning)
    {
        case Warning::inputChannels:
            return "Not enough input channels for the current number of sources.";
        case Warning::outputChannels:
            return "Not enough output channels for the selected Ambisonic order.";
        case Warning::frameSize:
            return "The host block size does not match the processing frame size.";
        case Warning::none:
        default:
            return {};
    }
}

void StudioShell::timerCallback()
{
    handleHostStateRestore();

    if (undoCheckpointPending
        && juce::Time::getMillisecondCounter() - lastEditTimeMs >= undoCoalesceMs)
        flushUndoCheckpoint();

    updateWarning();

    const double now = processor.getCurrentHostTime();

#if JucePlugin_Build_Standalone
    /* Plugin-internal preview playback: advance the playhead at 1x. */
    if (internalPlaying)
    {
        previewTime += 0.08;
        setPreview (previewTime, true, false);
    }
#endif

    /* Follow host transport when it is running and the user is not
       manually scrubbing/previewing. */
    if (processor.isHostPlaying())
    {
        manualPreview = false;
        setPreview (now, true, false);
    }
    else if (! processor.isHostPlaying() && ! manualPreview && previewActive
             && ! internalPlaying)
    {
        /* Host stopped and we were auto-following: revert to base poses. */
        setPreview (now, false, false);
    }

    if (timeline != nullptr)
        timeline->setScrubTime (now);

    inspector.refresh();
    sourceBar.refresh();
    syncRenderOptions();
    refreshSummary();

    /* Keep the canvas alive while a path is being animated. */
    if (previewActive || processor.isHostPlaying() || internalPlaying)
        viewport.refresh();

    repaint (timeArea);
    repaint (topBarArea);
    if (timelineVisible && ! focusMode)
        repaint (timelineHeaderArea);

    const bool fullscreen = isFullScreenMode();
    if (fullscreenButton.isSelected() != fullscreen)
        fullscreenButton.setSelected (fullscreen);

    if (focusButton.isSelected() != focusMode)
        focusButton.setSelected (focusMode);

    if (selection.isNone())
        return;

    const bool selectionIsReceiver = selection.isReceiver()
        || (selection.isCheckpoint() && selection.checkpointIsReceiver);
    const int limit = selectionIsReceiver ? processor.getParameterInt ("numReceivers")
                                          : processor.getParameterInt ("numSources");

    if (selection.index < limit)
        return;

    const int clampedIndex = juce::jmax (0, limit - 1);
    applySelection (selectionIsReceiver
                        ? studio::Selection::receiver (clampedIndex)
                        : studio::Selection::source (clampedIndex),
                    true);
}

void StudioShell::filenameComponentChanged (juce::FilenameComponent*)
{
    const juce::File file = trackChooser.getCurrentFile();

    if (file.getFullPathName().isEmpty())
        processor.clearTrackFile();
    else
        processor.loadTrackFile (file);

    if (timeline != nullptr)
        timeline->repaint();
}
