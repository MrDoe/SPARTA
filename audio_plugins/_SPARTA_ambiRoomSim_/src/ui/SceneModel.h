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
#include <array>
#include <vector>
#include "ActorLibrary.h"

namespace studio
{

/* Visual metadata for one source/receiver: which built-in actor represents
   it, an optional display name, and a visual facing/scale. Facing is a
   standard azimuth in degrees: 0 faces the front wall (-y), +90 faces +x
   (left when looking at the front wall), +/-180 faces the back wall (+y),
   and -90 faces -x (right). The receiver facing is not purely cosmetic:
   the processor rotates that receiver's Ambisonic output so the drawn
   facing arrow always matches the decoded front (see
   PluginProcessor::updateReceiverRotationMatrices()). Everything else is
   stored in the plug-in state under a SCENE node so the casting survives
   save/reload. */
struct ActorInfo
{
    ActorKind kind = ActorKind::performer;
    juce::String name;
    float facingDegrees = 0.0f; /* azimuth: 0 = front wall, +90 = left */
    float scale = 1.0f;
};

struct MarkerInfo
{
    double time = 0.0;
    juce::String label;
};

struct UiPreferences
{
    bool showTimes = true;
    bool showGrid = true;
    bool showLabels = true;
    bool snap = false;
    /* Global text scale for the studio UI. All studio text is drawn through
       studio::uiFont(), so this one value scales the whole interface. */
    float fontScale = 1.0f;
};

/* Mirrors ROOM_SIM_MAX_NUM_SOURCES and ROOM_SIM_MAX_NUM_RECEIVERS in the DSP
   (kept local so the UI headers do not depend on the SAF header). */
static constexpr int maxSceneObjects = 16;

/* Scene metadata for all 16 sources and receivers plus timeline markers and
   view preferences. The DSP never reads it. */
class SceneModel
{
public:
    SceneModel();

    void reset();

    ActorInfo& getActor (bool isReceiver, int index);
    const ActorInfo& getActor (bool isReceiver, int index) const;

    UiPreferences& getUiPreferences() { return ui; }
    const UiPreferences& getUiPreferences() const { return ui; }

    std::vector<MarkerInfo>& getMarkers() { return markers; }
    const std::vector<MarkerInfo>& getMarkers() const { return markers; }

    juce::ValueTree toValueTree (int version) const;
    void fromValueTree (const juce::ValueTree& state);

private:
    std::array<ActorInfo, maxSceneObjects> sources;
    std::array<ActorInfo, maxSceneObjects> receivers;
    std::vector<MarkerInfo> markers;
    UiPreferences ui;

    ActorInfo& at (bool isReceiver, int index);
    const ActorInfo& at (bool isReceiver, int index) const;
};

} // namespace studio
