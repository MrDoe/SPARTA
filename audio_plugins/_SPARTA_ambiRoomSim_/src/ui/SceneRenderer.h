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
#include <vector>

#include "../PluginProcessor.h"
#include "ActorLibrary.h"
#include "Mesh3D.h"
#include "SelectionState.h"

namespace studio
{

/* Orbiting perspective/orthographic camera for the spatial canvas.

   World coordinates are the room coordinates used by the DSP: x = width,
   y = depth, z = height, in metres, with z up. */
struct Camera
{
    Vec3 target { 4.0f, 3.0f, 1.6f };
    float yaw = -2.35f;        /* azimuth of the eye around the target, radians */
    float pitch = 0.42f;       /* elevation of the eye, radians */
    float distance = 16.0f;    /* perspective orbit radius */
    float fovDegrees = 45.0f;
    bool orthographic = false;
    float orthoHeight = 10.0f; /* visible world height in ortho views */

    /* Derived per paint. */
    Vec3 eye, right, up, forward;
    juce::Rectangle<float> viewArea;
    float viewW = 1.0f, viewH = 1.0f;
    float cx = 0.0f, cy = 0.0f;
    float tanHalf = 0.4142f;
    float orthoScale = 1.0f;

    void setup (juce::Rectangle<float> area);
    Vec3 toView (const Vec3& world) const;
    juce::Point<float> viewToScreen (const Vec3& view, float& depth) const;
    bool project (const Vec3& world, juce::Point<float>& screen, float& depth) const;
    bool projectSegment (const Vec3& a, const Vec3& b,
                         juce::Point<float>& pa, juce::Point<float>& pb) const;

    /* Intersects the screen ray with a horizontal plane at the given height. */
    bool rayToPlane (juce::Point<float> screen, float planeZ, Vec3& world) const;
    /* World units covered by one screen pixel at the given view depth. */
    float worldPerPixel (float depth) const;

    void orbit (float dxPixels, float dyPixels);
    void pan (float dxPixels, float dyPixels);
    void zoom (float factor);
};

struct RenderOptions
{
    bool showGrid = true;
    bool showTimes = true;
    bool showLabels = true;
    bool showPaths = true;
    bool snap = false;
    bool previewActive = false;
    double previewTime = 0.0;
};

/* Painter's-algorithm vector renderer for the room, actors and paths.

   The class is stateless: every call reads the current parameters, the path
   bank and the scene cast, so the viewport only has to repaint when
   something changed. */
class SceneRenderer
{
public:
    static void paint (juce::Graphics& g,
                       juce::Rectangle<int> bounds,
                       PluginProcessor& processor,
                       const Camera& camera,
                       const RenderOptions& options,
                       const Selection& selection);

    /* Returns the actor nearest to the mouse, or a none-selection. */
    static Selection pickActor (juce::Rectangle<int> bounds,
                                PluginProcessor& processor,
                                const Camera& camera,
                                const RenderOptions& options,
                                juce::Point<float> mouse);

    /* Returns the path checkpoint nearest to the mouse. Checkpoints on the
       preferred actor's path win when both are inside the hit radius, so a
       selected path stays easy to edit. */
    static Selection pickCheckpoint (juce::Rectangle<int> bounds,
                                     PluginProcessor& processor,
                                     const Camera& camera,
                                     juce::Point<float> mouse,
                                     const Selection& preferred);

    static void frameRoom (Camera& camera, PluginProcessor& processor,
                           juce::Rectangle<int> bounds);
    static void frameSelection (Camera& camera, PluginProcessor& processor,
                                const Selection& selection,
                                juce::Rectangle<int> bounds);

    /* 0 = perspective, 1 = top, 2 = front, 3 = side. */
    static void setViewPreset (Camera& camera, int preset,
                               PluginProcessor& processor,
                               juce::Rectangle<int> bounds);

    /* Position of an object at the audition time (or its live position). */
    static bool getActorPosition (PluginProcessor& processor,
                                  bool isReceiver, int index,
                                  const RenderOptions& options,
                                  Vec3& position);
};

} // namespace studio
