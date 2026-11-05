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
#include <vector>

#include "../PluginProcessor.h"
#include "SceneRenderer.h"
#include "SelectionState.h"

/* Interactive 3D canvas: orbit/pan/zoom camera, actor selection and floor
   dragging, acoustic-height editing, view presets and source placement. */
class Viewport3D : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    enum class Tool
    {
        select = 0,
        addSource,
        drawSpline,
        primitives,
        orbit,
        fly
    };

    enum class Primitive
    {
        line = 0,
        circle,
        figureEight
    };

    explicit Viewport3D (PluginProcessor& processor);
    ~Viewport3D() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

    void setTool (Tool newTool);
    Tool getTool() const { return tool; }

    void setSelection (const studio::Selection& newSelection, bool notify = true);
    const studio::Selection& getSelection() const { return selection; }

    void setPreviewTime (double time, bool active);
    void setRenderOptions (const studio::RenderOptions& options);
    const studio::RenderOptions& getRenderOptions() const { return options; }

    void setViewPreset (int preset);
    int getViewPreset() const { return activePreset; }

    void frameRoom();
    void frameSelection();

    /* Replaces the selected object's path with a generated shape, or clears
       it. Returns false when no usable object is selected. */
    bool createPrimitive (Primitive primitive);
    void clearSelectedPath();

    void refresh() { repaint(); }

    std::function<void (studio::Selection)> onSelectionChanged;
    std::function<void()> onSourceAdded;
    std::function<void()> onPathEdited;
    /* Fired after a direct parameter edit in the canvas (actor drag or
       keyboard nudge) so the shell can record an undo step. */
    std::function<void()> onEditCommitted;
    std::function<void (Tool)> onToolChanged;

private:
    PluginProcessor& processor;
    studio::Camera camera;
    studio::RenderOptions options;
    studio::Selection selection;
    Tool tool = Tool::select;
    int activePreset = 0;

    struct DragState
    {
        bool actor = false;
        bool checkpoint = false;
        bool checkpointChanged = false;
        bool handle = false;
        bool handleIsIn = false;
        bool handleChanged = false;
        bool vertical = false;
        bool orbit = false;
        bool pan = false;
        bool fly = false;
        studio::Selection selection;
        studio::Vec3 startPosition;
        juce::Point<float> lastPosition;
        float startZ = 0.0f;
        float planeZ = 0.0f;
        bool gesturesStarted = false;
    };
    DragState drag;
    bool framedOnce = false;

    std::vector<std::unique_ptr<juce::Button>> viewButtons;
    juce::TextButton fitButton { "FIT" };

    void updatePillStates();
    void setToolInternal (Tool newTool, bool notify);
    void beginParameterGestures (bool isReceiver, int index);
    void endParameterGestures();
    void nudgeSelection (float dx, float dy, float dz);
    void addSourceAt (juce::Point<float> screenPosition);
    void addCheckpointAt (juce::Point<float> screenPosition, bool onFloor);
    void deleteSelectedCheckpoint();
    void notifyPathEdited();
    void drawHint (juce::Graphics& g) const;
    juce::String getToolHint() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Viewport3D)
};
