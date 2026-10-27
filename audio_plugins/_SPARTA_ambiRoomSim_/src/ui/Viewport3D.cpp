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

#include "Viewport3D.h"
#include "StudioTheme.h"

#include <algorithm>
#include <cmath>

namespace
{

class ViewPillButton : public juce::Button
{
public:
    explicit ViewPillButton (const juce::String& text)
        : juce::Button (text)
    {
        setWantsKeyboardFocus (true);
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool selected = (bool) getProperties().getWithDefault ("pillSelected", false);

        const auto fill = selected ? studio::Colours::accentDim
                                   : ((isHovered || isDown)
                                          ? studio::Colours::surface.withAlpha (0.92f)
                                          : studio::Colours::bg.withAlpha (0.62f));

        g.setColour (fill);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (selected ? studio::Colours::accent.withAlpha (0.9f)
                              : studio::Colours::strokeStrong);
        g.drawRoundedRectangle (r, 4.0f, selected ? 1.5f : 1.0f);

        g.setColour (selected ? studio::Colours::text
                              : studio::Colours::textSecondary);
        g.setFont (studio::uiFont (10.5f, selected));
        g.drawText (getButtonText(), r, juce::Justification::centred, false);

        if (hasKeyboardFocus (false))
        {
            g.setColour (studio::Colours::accent);
            g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 1.0f);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViewPillButton)
};

float snapped (float value, bool snapEnabled, float step)
{
    if (! snapEnabled)
        return value;
    return std::round (value / step) * step;
}

juce::String positionParameterID (bool isReceiver, int index, char axis)
{
    juce::String id (isReceiver ? "receiver" : "source");
    id << index;
    id << juce::String::charToString ((juce_wchar) axis);
    return id;
}

void setPositionParameter (PluginProcessor& processor, bool isReceiver,
                           int index, char axis, float value)
{
    processor.setParameterValue (positionParameterID (isReceiver, index, axis), value);
}

float getPositionParameter (PluginProcessor& processor, bool isReceiver,
                            int index, char axis)
{
    return processor.getParameterFloat (positionParameterID (isReceiver, index, axis));
}

} // namespace

Viewport3D::Viewport3D (PluginProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setTooltip ("3D spatial canvas: LMB select/drag, Shift-drag height, "
                "Alt-drag orbit, MMB pan, wheel zoom, F frame selection.");

    const char* names[4] = { "Perspective", "Top", "Front", "Side" };
    const char* tips[4] = { "Perspective view (1)", "Top orthographic view (2)",
                            "Front orthographic view (3)", "Side orthographic view (4)" };

    for (int i = 0; i < 4; ++i)
    {
        auto button = std::unique_ptr<juce::Button> (new ViewPillButton (names[i]));
        button->setTooltip (tips[i]);
        button->onClick = [this, i]
        {
            setViewPreset (i);
            grabKeyboardFocus();
        };
        addAndMakeVisible (button.get());
        viewButtons.push_back (std::move (button));
    }

    fitButton.setTooltip ("Frame the whole room (F)");
    fitButton.onClick = [this]
    {
        frameRoom();
        grabKeyboardFocus();
    };
    addAndMakeVisible (fitButton);

    camera.target = { processor.getParameterFloat ("roomX") * 0.5f,
                      processor.getParameterFloat ("roomY") * 0.5f,
                      processor.getParameterFloat ("roomZ") * 0.45f };
    camera.distance = 16.0f;

    updatePillStates();
}

Viewport3D::~Viewport3D() = default;

void Viewport3D::resized()
{
    int x = 12;
    const int widths[4] = { 86, 44, 48, 44 };

    for (int i = 0; i < (int) viewButtons.size(); ++i)
    {
        viewButtons[(size_t) i]->setBounds (x, 12, widths[i], 24);
        x += widths[i] + 4;
    }

    fitButton.setBounds (juce::jmax (12, getWidth() - 60), getHeight() - 36, 46, 24);

    if (! framedOnce && getWidth() > 80 && getHeight() > 80)
    {
        framedOnce = true;
        studio::SceneRenderer::frameRoom (camera, processor, getLocalBounds());
    }
}

void Viewport3D::paint (juce::Graphics& g)
{
    studio::SceneRenderer::paint (g, getLocalBounds(), processor, camera, options, selection);
    drawHint (g);

    if (hasKeyboardFocus (false))
    {
        g.setColour (studio::Colours::accent.withAlpha (0.35f));
        g.drawRect (getLocalBounds(), 1);
    }
}

void Viewport3D::setTool (Tool newTool)
{
    setToolInternal (newTool, false);
}

void Viewport3D::setToolInternal (Tool newTool, bool notify)
{
    if (tool == newTool)
        return;

    tool = newTool;
    repaint();

    if (notify && onToolChanged)
        onToolChanged (tool);
}

void Viewport3D::setSelection (const studio::Selection& newSelection, bool notify)
{
    if (selection == newSelection)
    {
        repaint();
        return;
    }

    selection = newSelection;
    repaint();

    if (notify && onSelectionChanged)
        onSelectionChanged (selection);
}

void Viewport3D::setPreviewTime (double time, bool active)
{
    options.previewTime = time;
    options.previewActive = active;
    repaint();
}

void Viewport3D::setRenderOptions (const studio::RenderOptions& newOptions)
{
    options = newOptions;
    repaint();
}

void Viewport3D::setViewPreset (int preset)
{
    camera.setup (getLocalBounds().toFloat());
    studio::SceneRenderer::setViewPreset (camera, preset, processor, getLocalBounds());
    camera.setup (getLocalBounds().toFloat());
    activePreset = preset;
    updatePillStates();
    repaint();
}

void Viewport3D::frameRoom()
{
    studio::SceneRenderer::frameRoom (camera, processor, getLocalBounds());
    camera.setup (getLocalBounds().toFloat());
    updatePillStates();
    repaint();
}

void Viewport3D::frameSelection()
{
    if (selection.isNone())
        frameRoom();
    else
        studio::SceneRenderer::frameSelection (camera, processor, selection, getLocalBounds());

    camera.setup (getLocalBounds().toFloat());
    if (! camera.orthographic)
        activePreset = 0;
    updatePillStates();
    repaint();
}

void Viewport3D::updatePillStates()
{
    for (int i = 0; i < (int) viewButtons.size(); ++i)
    {
        viewButtons[(size_t) i]->getProperties().set ("pillSelected", i == activePreset);
        viewButtons[(size_t) i]->repaint();
    }
}

void Viewport3D::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    camera.setup (getLocalBounds().toFloat());

    drag = DragState();
    drag.lastPosition = e.position;

    if (e.mods.isPopupMenu() || e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        const bool pan = e.mods.isMiddleButtonDown() && ! e.mods.isAltDown();
        drag.pan = pan;
        drag.orbit = ! pan;
        return;
    }

    if (tool == Tool::orbit)
    {
        drag.orbit = true;
        return;
    }

    if (tool == Tool::fly)
    {
        drag.fly = true;
        return;
    }

    if (tool == Tool::addSource)
    {
        addSourceAt (e.position);
        return;
    }

    if (tool == Tool::drawSpline)
    {
        addCheckpointAt (e.position, e.mods.isShiftDown());
        return;
    }

    if (tool == Tool::primitives)
        return;

    if (tool == Tool::select)
    {
        /* Checkpoints are drawn on top of the actor; pick them first so a
           node can be dragged even when it sits on the object's acoustic
           point. */
        if (options.showPaths)
        {
            const auto pickedNode = studio::SceneRenderer::pickCheckpoint (
                getLocalBounds(), processor, camera, e.position, selection);

            if (pickedNode.isCheckpoint())
            {
                /* Keep the inspector, source bar and timeline in sync with a
                   checkpoint picked directly in the canvas. */
                setSelection (pickedNode, true);

                Keyframe keyframe;
                bool valid = false;
                {
                    const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
                    const PathData& path = pickedNode.checkpointIsReceiver
                                               ? processor.getPathBank().getReceiverPath (pickedNode.index)
                                               : processor.getPathBank().getSourcePath (pickedNode.index);
                    if (pickedNode.keyframe >= 0
                        && pickedNode.keyframe < (int) path.keyframes.size())
                    {
                        keyframe = path.keyframes[(size_t) pickedNode.keyframe];
                        valid = true;
                    }
                }

                if (valid)
                {
                    drag.checkpoint = true;
                    drag.selection = pickedNode;
                    drag.vertical = e.mods.isShiftDown();
                    drag.planeZ = keyframe.z;
                    drag.startPosition = { keyframe.x, keyframe.y, keyframe.z };
                    drag.startZ = keyframe.z;
                }

                return;
            }
        }

        const auto picked = studio::SceneRenderer::pickActor (getLocalBounds(), processor,
                                                      camera, options, e.position);
        setSelection (picked, true);

        if (! picked.isActor())
            return;

        const bool isReceiver = picked.isReceiver();
        const int index = picked.index;
        bool pathEnabled = false;
        {
            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            const PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                              : processor.getPathBank().getSourcePath (index);
            pathEnabled = path.enabled;
        }

        if (pathEnabled)
            return; /* the path drives this object; move it in the timeline */

        drag.actor = true;
        drag.selection = picked;
        drag.vertical = e.mods.isShiftDown();
        drag.startPosition = { getPositionParameter (processor, isReceiver, index, 'X'),
                               getPositionParameter (processor, isReceiver, index, 'Y'),
                               getPositionParameter (processor, isReceiver, index, 'Z') };
        drag.startZ = drag.startPosition.z;
        beginParameterGestures (isReceiver, index);
    }
}

void Viewport3D::mouseDrag (const juce::MouseEvent& e)
{
    camera.setup (getLocalBounds().toFloat());

    const auto delta = e.position - drag.lastPosition;
    drag.lastPosition = e.position;

    if (drag.orbit)
    {
        camera.orbit (delta.x, delta.y);
        activePreset = -1;
        updatePillStates();
        repaint();
        return;
    }

    if (drag.pan)
    {
        camera.pan (delta.x, delta.y);
        activePreset = -1;
        updatePillStates();
        repaint();
        return;
    }

    if (drag.fly)
    {
        camera.yaw -= delta.x * 0.009f;
        camera.pitch = juce::jlimit (-1.45f, 1.45f,
                                     camera.pitch + delta.y * 0.009f);
        activePreset = -1;
        updatePillStates();
        repaint();
        return;
    }

    if (drag.checkpoint && drag.selection.isCheckpoint())
    {
        const bool isReceiver = drag.selection.checkpointIsReceiver;
        const int index = drag.selection.index;
        const int keyframeIndex = drag.selection.keyframe;
        const float roomX = processor.getParameterFloat ("roomX");
        const float roomY = processor.getParameterFloat ("roomY");
        const float roomZ = processor.getParameterFloat ("roomZ");
        bool changed = false;

        {
            const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
            PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                        : processor.getPathBank().getSourcePath (index);

            if (keyframeIndex >= 0 && keyframeIndex < (int) path.keyframes.size())
            {
                auto& keyframe = path.keyframes[(size_t) keyframeIndex];

                if (drag.vertical)
                {
                    const float viewDepth = juce::jmax (0.5f,
                                                        camera.toView (drag.startPosition).z);
                    const float dz = -(float) e.getDistanceFromDragStartY()
                                   * camera.worldPerPixel (viewDepth);
                    float z = snapped (drag.startZ + dz, options.snap, 0.25f);
                    keyframe.z = juce::jlimit (0.0f, roomZ, z);
                }
                else
                {
                    studio::Vec3 planePoint;
                    if (camera.rayToPlane (e.position, drag.planeZ, planePoint))
                    {
                        const float x = juce::jlimit (0.0f, roomX,
                                                      snapped (planePoint.x, options.snap, 0.25f));
                        const float y = juce::jlimit (0.0f, roomY,
                                                      snapped (planePoint.y, options.snap, 0.25f));
                        keyframe.x = x;
                        keyframe.y = y;
                    }
                }

                path.recomputeDefaultTangent (keyframeIndex);
                changed = true;
            }
        }

        if (changed)
        {
            drag.checkpointChanged = true;
            processor.markPathDirty (false);
            repaint();
        }

        return;
    }

    if (! drag.actor || ! drag.selection.isActor())
        return;

    const bool isReceiver = drag.selection.isReceiver();
    const int index = drag.selection.index;

    if (drag.vertical)
    {
        const float viewDepth = juce::jmax (0.5f, camera.toView (drag.startPosition).z);
        const float dz = -(float) e.getDistanceFromDragStartY() * camera.worldPerPixel (viewDepth);
        float z = drag.startZ + dz;
        if (options.snap)
            z = snapped (z, true, 0.25f);
        z = juce::jlimit (0.0f, processor.getParameterFloat ("roomZ"), z);
        setPositionParameter (processor, isReceiver, index, 'Z', z);
    }
    else
    {
        studio::Vec3 floorPoint;
        if (camera.rayToPlane (e.position, 0.0f, floorPoint))
        {
            float x = snapped (floorPoint.x, options.snap, 0.25f);
            float y = snapped (floorPoint.y, options.snap, 0.25f);
            x = juce::jlimit (0.0f, processor.getParameterFloat ("roomX"), x);
            y = juce::jlimit (0.0f, processor.getParameterFloat ("roomY"), y);
            setPositionParameter (processor, isReceiver, index, 'X', x);
            setPositionParameter (processor, isReceiver, index, 'Y', y);
        }
    }

    repaint();
}

void Viewport3D::mouseUp (const juce::MouseEvent&)
{
    endParameterGestures();

    if (drag.checkpointChanged)
    {
        processor.markPathDirty();
        notifyPathEdited();
    }

    drag = DragState();
}

void Viewport3D::mouseDoubleClick (const juce::MouseEvent& e)
{
    camera.setup (getLocalBounds().toFloat());

    if (options.showPaths)
    {
        const auto pickedNode = studio::SceneRenderer::pickCheckpoint (
            getLocalBounds(), processor, camera, e.position, selection);
        if (pickedNode.isCheckpoint())
        {
            setSelection (pickedNode, true);
            frameSelection();
            return;
        }
    }

    const auto picked = studio::SceneRenderer::pickActor (getLocalBounds(), processor,
                                                  camera, options, e.position);
    if (picked.isActor())
    {
        setSelection (picked, true);
        frameSelection();
    }
}

void Viewport3D::mouseWheelMove (const juce::MouseEvent&,
                                 const juce::MouseWheelDetails& wheel)
{
    camera.setup (getLocalBounds().toFloat());
    const float factor = wheel.deltaY > 0.0f ? 0.88f : 1.13f;

    if (tool == Tool::fly)
    {
        camera.target += camera.forward * (wheel.deltaY > 0.0f ? 0.45f : -0.45f);
        activePreset = -1;
    }
    else
    {
        camera.zoom (factor);
        activePreset = -1;
    }

    updatePillStates();
    repaint();
}

bool Viewport3D::keyPressed (const juce::KeyPress& key)
{
    camera.setup (getLocalBounds().toFloat());
    const auto character = key.getTextCharacter();
    const int code = key.getKeyCode();
    const bool shift = key.getModifiers().isShiftDown();

    if (character == 'v' || character == 'V')
    {
        setToolInternal (Tool::select, true);
        return true;
    }

    if (character == 'a' || character == 'A')
    {
        setToolInternal (Tool::addSource, true);
        return true;
    }

    if (character == 'b' || character == 'B')
    {
        setToolInternal (Tool::drawSpline, true);
        return true;
    }

    if (character == 'o' || character == 'O')
    {
        setToolInternal (Tool::orbit, true);
        return true;
    }

    if (character == 'y' || character == 'Y')
    {
        setToolInternal (Tool::fly, true);
        return true;
    }

    if (code == juce::KeyPress::deleteKey
        || code == juce::KeyPress::backspaceKey)
    {
        deleteSelectedCheckpoint();
        return true;
    }

    if (code == juce::KeyPress::escapeKey)
    {
        setSelection (studio::Selection::none(), true);
        return true;
    }

    if (character == 'f' || character == 'F')
    {
        frameSelection();
        return true;
    }

    if (character >= '1' && character <= '4')
    {
        setViewPreset ((int) (character - '1'));
        return true;
    }

    if (tool == Tool::fly)
    {
        const float step = shift ? 1.2f : 0.35f;
        bool moved = true;

        if (character == 'w' || character == 'W')      camera.target += camera.forward * step;
        else if (character == 's' || character == 'S') camera.target -= camera.forward * step;
        else if (character == 'd' || character == 'D') camera.target += camera.right * step;
        else if (character == 'a' || character == 'A') camera.target -= camera.right * step;
        else if (character == 'e' || character == 'E') camera.target += studio::Vec3 (0.0f, 0.0f, step);
        else if (character == 'q' || character == 'Q') camera.target -= studio::Vec3 (0.0f, 0.0f, step);
        else moved = false;

        if (moved)
        {
            activePreset = -1;
            updatePillStates();
            repaint();
            return true;
        }
    }

    const float nudge = shift ? 0.5f : 0.05f;
    if (code == juce::KeyPress::leftKey)   { nudgeSelection (-nudge, 0.0f, 0.0f); return true; }
    if (code == juce::KeyPress::rightKey)  { nudgeSelection ( nudge, 0.0f, 0.0f); return true; }
    if (code == juce::KeyPress::upKey)     { nudgeSelection (0.0f,  nudge, 0.0f); return true; }
    if (code == juce::KeyPress::downKey)   { nudgeSelection (0.0f, -nudge, 0.0f); return true; }
    if (code == juce::KeyPress::pageUpKey) { nudgeSelection (0.0f, 0.0f,  nudge); return true; }
    if (code == juce::KeyPress::pageDownKey) { nudgeSelection (0.0f, 0.0f, -nudge); return true; }

    return false;
}

void Viewport3D::nudgeSelection (float dx, float dy, float dz)
{
    if (! selection.isActor())
        return;

    const bool isReceiver = selection.isReceiver();
    const int index = selection.index;
    const float roomX = processor.getParameterFloat ("roomX");
    const float roomY = processor.getParameterFloat ("roomY");
    const float roomZ = processor.getParameterFloat ("roomZ");

    const float x = juce::jlimit (0.0f, roomX, getPositionParameter (processor, isReceiver, index, 'X') + dx);
    const float y = juce::jlimit (0.0f, roomY, getPositionParameter (processor, isReceiver, index, 'Y') + dy);
    const float z = juce::jlimit (0.0f, roomZ, getPositionParameter (processor, isReceiver, index, 'Z') + dz);

    setPositionParameter (processor, isReceiver, index, 'X', x);
    setPositionParameter (processor, isReceiver, index, 'Y', y);
    setPositionParameter (processor, isReceiver, index, 'Z', z);
    repaint();
}

void Viewport3D::addSourceAt (juce::Point<float> screenPosition)
{
    const int numSources = processor.getParameterInt ("numSources");
    if (numSources >= 16)
        return;

    studio::Vec3 floorPoint;
    if (! camera.rayToPlane (screenPosition, 0.0f, floorPoint))
        return;

    const float roomX = processor.getParameterFloat ("roomX");
    const float roomY = processor.getParameterFloat ("roomY");
    const float roomZ = processor.getParameterFloat ("roomZ");

    float x = snapped (floorPoint.x, options.snap, 0.25f);
    float y = snapped (floorPoint.y, options.snap, 0.25f);
    x = juce::jlimit (0.0f, roomX, x);
    y = juce::jlimit (0.0f, roomY, y);

    processor.setParameterValue ("numSources", numSources + 1);
    setPositionParameter (processor, false, numSources, 'X', x);
    setPositionParameter (processor, false, numSources, 'Y', y);
    setPositionParameter (processor, false, numSources, 'Z', juce::jmin (1.4f, roomZ));

    setSelection (studio::Selection::source (numSources));

    if (onSourceAdded)
        onSourceAdded();
}

void Viewport3D::addCheckpointAt (juce::Point<float> screenPosition, bool onFloor)
{
    studio::Selection target = selection;

    if (! target.isActor() && ! target.isCheckpoint())
    {
        target = studio::SceneRenderer::pickActor (getLocalBounds(), processor,
                                                   camera, options, screenPosition);
        if (! target.isActor())
            return;
    }

    const bool isReceiver = target.isCheckpoint() ? target.checkpointIsReceiver
                                                  : target.isReceiver();
    const int index = juce::jlimit (0, 15, target.index);

    camera.setup (getLocalBounds().toFloat());

    float planeZ = 0.0f;
    bool hasNode = false;

    {
        const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
        const PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                          : processor.getPathBank().getSourcePath (index);
        if (! path.keyframes.empty())
        {
            planeZ = path.keyframes.back().z;
            hasNode = true;
        }
    }

    if (! onFloor && ! hasNode)
    {
        studio::Vec3 actorPosition;
        studio::SceneRenderer::getActorPosition (processor, isReceiver, index,
                                                 options, actorPosition);
        planeZ = actorPosition.z;
    }

    studio::Vec3 planePoint;
    if (! camera.rayToPlane (screenPosition, onFloor ? 0.0f : planeZ, planePoint))
        return;

    const float roomX = processor.getParameterFloat ("roomX");
    const float roomY = processor.getParameterFloat ("roomY");
    const float roomZ = processor.getParameterFloat ("roomZ");

    const float x = juce::jlimit (0.0f, roomX,
                                  snapped (planePoint.x, options.snap, 0.25f));
    const float y = juce::jlimit (0.0f, roomY,
                                  snapped (planePoint.y, options.snap, 0.25f));
    const float z = juce::jlimit (0.0f, roomZ,
                                  onFloor ? 0.0f : planeZ);

    int insertedIndex = -1;

    {
        const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
        PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                    : processor.getPathBank().getSourcePath (index);

        if ((int) path.keyframes.size() >= MAX_KEYFRAMES_PER_PATH)
            return;

        const bool firstNode = path.keyframes.empty();
        const double now = options.previewActive ? options.previewTime
                                                 : processor.getCurrentHostTime();
        double candidateTime = 0.0;

        if (firstNode)
        {
            /* Start a new path at the current transport position. This keeps
               the first checkpoint and the following ones close together when
               the user draws while playback is running. */
            if (path.startTime == 0.0
                && std::abs (path.endTime - 10.0) < 0.001
                && now > 1.0)
            {
                path.startTime = now;
                path.endTime = now + 10.0;
            }
        }
        else
        {
            candidateTime = now - path.startTime;

            const double lastTime = path.keyframes.back().timeSeconds;
            if (candidateTime <= lastTime + 0.01)
                candidateTime = lastTime + 1.0;

            /* Do not stack a new node on top of an existing one. */
            for (const auto& keyframe : path.keyframes)
            {
                if (std::abs (keyframe.timeSeconds - candidateTime) < 0.25)
                {
                    candidateTime = lastTime + 1.0;
                    break;
                }
            }
        }

        if (candidateTime < 0.0)
            candidateTime = 0.0;

        Keyframe keyframe;
        keyframe.timeSeconds = candidateTime;
        keyframe.x = x;
        keyframe.y = y;
        keyframe.z = z;

        auto insertAt = std::find_if (path.keyframes.begin(), path.keyframes.end(),
                                      [candidateTime] (const Keyframe& node)
                                      {
                                          return node.timeSeconds > candidateTime;
                                      });

        insertedIndex = (int) std::distance (path.keyframes.begin(), insertAt);
        path.keyframes.insert (insertAt, keyframe);

        path.recomputeDefaultTangent (insertedIndex);
        if (insertedIndex > 0)
            path.recomputeDefaultTangent (insertedIndex - 1);

        if (firstNode)
        {
            /* A fresh path spans the known track length, falling back to the
               default 10 s window when the host has not reported one. */
            const double trackEnd = processor.getTrackEndTime();
            if (path.startTime == 0.0 && path.endTime == 10.0 && trackEnd > 10.0)
                path.endTime = trackEnd;
        }

        path.enabled = true;

        if (path.startTime + candidateTime >= path.endTime - 0.05)
            path.endTime = path.startTime + candidateTime + 1.0;
    }

    processor.markPathDirty();
    setSelection (studio::Selection::checkpoint (isReceiver, index, insertedIndex), true);
    notifyPathEdited();
}

void Viewport3D::deleteSelectedCheckpoint()
{
    if (! selection.isCheckpoint())
        return;

    const bool isReceiver = selection.checkpointIsReceiver;
    const int index = juce::jlimit (0, 15, selection.index);
    bool deleted = false;

    {
        const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
        PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                    : processor.getPathBank().getSourcePath (index);

        if (selection.keyframe >= 0
            && selection.keyframe < (int) path.keyframes.size())
        {
            path.keyframes.erase (path.keyframes.begin() + selection.keyframe);
            deleted = true;

            if (path.keyframes.empty())
            {
                path.enabled = false;
                path.loop = false;
            }
        }
    }

    if (! deleted)
        return;

    processor.markPathDirty();
    setSelection (isReceiver ? studio::Selection::receiver (index)
                             : studio::Selection::source (index), true);
    notifyPathEdited();
}

void Viewport3D::notifyPathEdited()
{
    repaint();

    if (onPathEdited)
        onPathEdited();
}

bool Viewport3D::createPrimitive (Primitive primitive)
{
    studio::Selection target = selection;

    if (! target.isActor() && ! target.isCheckpoint())
        return false;

    const bool isReceiver = target.isCheckpoint() ? target.checkpointIsReceiver
                                                  : target.isReceiver();
    const int index = juce::jlimit (0, 15, target.index);

    const float roomX = processor.getParameterFloat ("roomX");
    const float roomY = processor.getParameterFloat ("roomY");
    const float roomZ = processor.getParameterFloat ("roomZ");

    studio::RenderOptions neutralOptions = options;
    neutralOptions.previewActive = false;

    studio::Vec3 reference;
    studio::SceneRenderer::getActorPosition (processor, isReceiver, index,
                                             neutralOptions, reference);

    double duration = processor.isTimeInPpq() ? 16.0 : 10.0;
    const double trackEnd = processor.getTrackEndTime();
    if (trackEnd >= 4.0)
        duration = juce::jlimit (4.0, 120.0, trackEnd);

    const auto clampX = [roomX] (float value)
    {
        return juce::jlimit (0.0f, roomX, value);
    };
    const auto clampY = [roomY] (float value)
    {
        return juce::jlimit (0.0f, roomY, value);
    };
    const auto clampZ = [roomZ] (float value)
    {
        return juce::jlimit (0.0f, roomZ, value);
    };

    std::vector<Keyframe> nodes;

    auto addNode = [&nodes] (double time, float x, float y, float z)
    {
        Keyframe node;
        node.timeSeconds = time;
        node.x = x;
        node.y = y;
        node.z = z;
        nodes.push_back (node);
    };

    const float centreX = roomX * 0.5f;
    const float centreY = roomY * 0.5f;
    const float z = clampZ (reference.z);

    switch (primitive)
    {
        case Primitive::line:
        {
            addNode (0.0, clampX (roomX * 0.12f), clampY (reference.y), z);
            addNode (duration, clampX (roomX * 0.88f), clampY (reference.y), z);
            break;
        }

        case Primitive::circle:
        {
            constexpr int segments = 8;
            const float radius = juce::jmin (roomX, roomY) * 0.28f;

            for (int i = 0; i <= segments; ++i)
            {
                const float angle = juce::MathConstants<float>::twoPi
                                  * (float) i / (float) segments;
                addNode (duration * (double) i / (double) segments,
                         clampX (centreX + radius * std::cos (angle)),
                         clampY (centreY + radius * std::sin (angle)),
                         z);
            }

            break;
        }

        case Primitive::figureEight:
        {
            constexpr int segments = 16;
            const float radiusX = roomX * 0.34f;
            const float radiusY = roomY * 0.34f;

            for (int i = 0; i <= segments; ++i)
            {
                const float angle = juce::MathConstants<float>::twoPi
                                  * (float) i / (float) segments;
                addNode (duration * (double) i / (double) segments,
                         clampX (centreX + radiusX * std::sin (angle)),
                         clampY (centreY + radiusY * std::sin (angle)
                                             * std::cos (angle)),
                         z);
            }

            break;
        }
    }

    if (nodes.size() < 2)
        return false;

    {
        const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
        PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                    : processor.getPathBank().getSourcePath (index);

        path.keyframes = nodes;
        path.startTime = 0.0;
        path.endTime = duration;
        path.enabled = true;
        path.loop = primitive != Primitive::line;
        path.recomputeDefaultTangents();
    }

    processor.markPathDirty();
    setSelection (isReceiver ? studio::Selection::receiver (index)
                             : studio::Selection::source (index), true);
    notifyPathEdited();
    return true;
}

void Viewport3D::clearSelectedPath()
{
    studio::Selection target = selection;

    if (! target.isActor() && ! target.isCheckpoint())
        return;

    const bool isReceiver = target.isCheckpoint() ? target.checkpointIsReceiver
                                                  : target.isReceiver();
    const int index = juce::jlimit (0, 15, target.index);
    bool changed = false;

    {
        const juce::SpinLock::ScopedLockType sl (processor.getPathLock());
        PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                    : processor.getPathBank().getSourcePath (index);

        if (! path.keyframes.empty() || path.enabled || path.loop)
        {
            path.keyframes.clear();
            path.enabled = false;
            path.loop = false;
            changed = true;
        }
    }

    if (! changed)
        return;

    processor.markPathDirty();
    setSelection (isReceiver ? studio::Selection::receiver (index)
                             : studio::Selection::source (index), true);
    notifyPathEdited();
}

void Viewport3D::beginParameterGestures (bool isReceiver, int index)
{
    const char axes[3] = { 'X', 'Y', 'Z' };
    for (char axis : axes)
    {
        if (auto* parameter = processor.parameters.getParameter (
                positionParameterID (isReceiver, index, axis)))
            parameter->beginChangeGesture();
    }
    drag.gesturesStarted = true;
}

void Viewport3D::endParameterGestures()
{
    if (! drag.gesturesStarted || ! drag.selection.isActor())
        return;

    const bool isReceiver = drag.selection.isReceiver();
    const int index = drag.selection.index;
    const char axes[3] = { 'X', 'Y', 'Z' };
    for (char axis : axes)
    {
        if (auto* parameter = processor.parameters.getParameter (
                positionParameterID (isReceiver, index, axis)))
            parameter->endChangeGesture();
    }

    drag.gesturesStarted = false;
}

juce::String Viewport3D::getToolHint() const
{
    switch (tool)
    {
        case Tool::select:
            return "Click node or actor  /  Drag to move  /  Shift drag height  /  Del removes node";
        case Tool::addSource:
            return "Click the floor to place a source  /  Alt-drag orbit";
        case Tool::drawSpline:
            if (! selection.isActor() && ! selection.isCheckpoint())
                return "Click an object first, then click to add checkpoints";
            return "Click to add checkpoints  /  Shift-click for floor height  /  Alt-drag orbit";
        case Tool::primitives:
            return "Choose Line, Circle or Figure eight from the Primitives tool";
        case Tool::orbit:
            return "Drag to orbit  /  Wheel to zoom";
        case Tool::fly:
            return "Drag to look  /  W A S D and Q E to fly  /  Wheel dolly";
        default:
            return "Select an object to edit it";
    }
}

void Viewport3D::drawHint (juce::Graphics& g) const
{
    const auto hint = getToolHint();
    const auto font = studio::uiFont (9.5f);
    const float width = font.getStringWidthFloat (hint) + 20.0f;
    const float leftLimit = 216.0f;
    const float rightLimit = (float) getWidth() - 76.0f;
    const float availableWidth = juce::jmax (120.0f, rightLimit - leftLimit);
    const float chipWidth = juce::jmin (width, availableWidth);

    const float centreX = (float) getWidth() * 0.5f;
    const float maxX = juce::jmax (leftLimit, rightLimit - chipWidth);
    juce::Rectangle<float> chip (juce::jlimit (leftLimit, maxX, centreX - chipWidth * 0.5f),
                                 (float) getHeight() - 36.0f,
                                 chipWidth, 22.0f);

    g.setColour (studio::Colours::bg.withAlpha (0.64f));
    g.fillRoundedRectangle (chip, 4.0f);
    g.setColour (studio::Colours::stroke);
    g.drawRoundedRectangle (chip.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (studio::Colours::textSecondary);
    g.setFont (font);
    g.drawFittedText (hint, chip.toNearestInt(),
                      juce::Justification::centred, 1, 0.85f);
}
