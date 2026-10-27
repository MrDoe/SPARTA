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

#pragma once

#include <JuceHeader.h>
#include <memory>

#include "../PluginProcessor.h"
#include "InspectorPanel.h"
#include "SelectionState.h"
#include "SourceBar.h"
#include "StudioLookAndFeel.h"
#include "StudioTheme.h"
#include "Viewport3D.h"

class pathTimelineView;

/* Full-window studio shell: top bar, tool rail, 3D canvas, inspector,
   read-only mixer strip and the path timeline dock. */
class StudioShell : public juce::Component,
                    private juce::Timer,
                    private juce::FilenameComponentListener
{
public:
    explicit StudioShell (PluginProcessor& processor);
    ~StudioShell() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void setFocusMode (bool shouldBeFocused);
    bool isFocusMode() const { return focusMode; }

    static constexpr int defaultWidth = 1600;
    static constexpr int defaultHeight = 950;

private:
    enum class Warning
    {
        none,
        inputChannels,
        outputChannels,
        frameSize
    };

    PluginProcessor& processor;
    void* hAmbi = nullptr;

    StudioLookAndFeel lookAndFeel;
    Viewport3D viewport;
    InspectorPanel inspector;
    SourceBar sourceBar;
    std::unique_ptr<pathTimelineView> timeline;

    juce::FilenameComponent trackChooser { "Track", juce::File(), true, false,
                                           false, "*.wav;*.aiff;*.aif;*.mp3",
                                           juce::String(), "Load track file..." };

    studio::IconButton focusButton { studio::Icon::focus,
                                     "Focus mode: hide the inspector and docks" };
    studio::IconButton fullscreenButton { studio::Icon::fullscreen,
                                          "Toggle fullscreen" };
    studio::IconButton settingsButton { studio::Icon::settings,
                                        "Studio settings" };
#if JucePlugin_Build_Standalone
    studio::IconButton playButton { studio::Icon::play,
                                    "Play: animate actors along their paths" };
    studio::IconButton stopButton { studio::Icon::stop,
                                    "Stop and reset the path preview" };
#endif

    juce::OwnedArray<juce::Button> railButtons;
    juce::Button* primitivesButton = nullptr;
    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

    studio::Selection selection;
    Warning currentWarning = Warning::none;

    double previewTime = 0.0;
    bool previewActive = false;
    bool manualPreview = false;
#if JucePlugin_Build_Standalone
    bool internalPlaying = false;
#endif
    bool focusMode = false;
    bool inspectorVisible = true;
    bool timelineVisible = true;

    juce::String lastSummary;
    juce::String lastOutputSummary;

    juce::Rectangle<int> topBarArea;
    juce::Rectangle<int> warningArea;
    juce::Rectangle<int> railArea;
    juce::Rectangle<int> canvasArea;
    juce::Rectangle<int> inspectorArea;
    juce::Rectangle<int> sourceBarArea;
    juce::Rectangle<int> timelineDockArea;
    juce::Rectangle<int> timelineHeaderArea;
    juce::Rectangle<int> brandArea;
    juce::Rectangle<int> summaryArea;
    juce::Rectangle<int> timeArea;

    void timerCallback() override;
    void filenameComponentChanged (juce::FilenameComponent*) override;

    void buildRail();
    void setTool (Viewport3D::Tool tool);
    void updateRailSelection();
    void applySelection (const studio::Selection& newSelection,
                         bool updateViewport);
    void setPreview (double time, bool active, bool manual = false);
    void syncRenderOptions();
    void updateWarning();
    void toggleFullscreen();
    bool isFullScreenMode() const;
    void setFullScreenMode (bool shouldBeFullScreen);
    void showSettingsMenu();
    void showPrimitivesMenu();
    void refreshSummary();
    juce::String buildSummary() const;
    juce::String buildOutputSummary() const;
    static juce::String warningText (Warning warning);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioShell)
};
