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

#include "SceneModel.h"

namespace studio
{

SceneModel::SceneModel()
{
    reset();
}

void SceneModel::reset()
{
    for (auto& a : sources)
        a = ActorInfo();

    for (auto& a : receivers)
        a = ActorInfo();

    /* A small default cast so the first launch is not an empty stage. The
       extra source entries only become visible as the source count grows. */
    sources[0].kind = ActorKind::performer;
    sources[1].kind = ActorKind::drone;
    sources[2].kind = ActorKind::car;
    sources[3].kind = ActorKind::speaker;
    sources[4].kind = ActorKind::bird;
    for (size_t i = 5; i < sources.size(); ++i)
        sources[i].kind = ActorKind::marker;

    for (auto& a : receivers)
    {
        a.kind = ActorKind::listener;
        a.yawDegrees = 180.0f; /* face the front of the room */
    }

    markers.clear();
    ui = UiPreferences();
}

ActorInfo& SceneModel::at (bool isReceiver, int index)
{
    const int clamped = juce::jlimit (0, maxSceneObjects - 1, index);
    return isReceiver ? receivers[(size_t) clamped] : sources[(size_t) clamped];
}

const ActorInfo& SceneModel::at (bool isReceiver, int index) const
{
    const int clamped = juce::jlimit (0, maxSceneObjects - 1, index);
    return isReceiver ? receivers[(size_t) clamped] : sources[(size_t) clamped];
}

ActorInfo& SceneModel::getActor (bool isReceiver, int index)
{
    return at (isReceiver, index);
}

const ActorInfo& SceneModel::getActor (bool isReceiver, int index) const
{
    return at (isReceiver, index);
}

juce::ValueTree SceneModel::toValueTree (int version) const
{
    juce::ValueTree state ("SCENE");
    state.setProperty ("version", version, nullptr);

    for (int i = 0; i < maxSceneObjects; ++i)
    {
        for (int side = 0; side < 2; ++side)
        {
            const bool isReceiver = side == 1;
            const auto& actor = at (isReceiver, i);
            juce::ValueTree node ("ACTOR");
            node.setProperty ("type", isReceiver ? "receiver" : "source", nullptr);
            node.setProperty ("index", i, nullptr);
            node.setProperty ("kind", toString (actor.kind), nullptr);
            node.setProperty ("name", actor.name, nullptr);
            node.setProperty ("yaw", actor.yawDegrees, nullptr);
            node.setProperty ("scale", actor.scale, nullptr);
            state.addChild (node, -1, nullptr);
        }
    }

    for (const auto& marker : markers)
    {
        juce::ValueTree node ("MARKER");
        node.setProperty ("time", marker.time, nullptr);
        node.setProperty ("label", marker.label, nullptr);
        state.addChild (node, -1, nullptr);
    }

    juce::ValueTree prefs ("UI");
    prefs.setProperty ("showTimes", ui.showTimes, nullptr);
    prefs.setProperty ("showGrid", ui.showGrid, nullptr);
    prefs.setProperty ("showLabels", ui.showLabels, nullptr);
    prefs.setProperty ("snap", ui.snap, nullptr);
    prefs.setProperty ("fontScale", ui.fontScale, nullptr);
    state.addChild (prefs, -1, nullptr);

    return state;
}

void SceneModel::fromValueTree (const juce::ValueTree& state)
{
    if (! state.hasType ("SCENE"))
        return;

    /* The node always carries the complete marker list, so replace the
       current one instead of appending a second copy. */
    markers.clear();

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto child = state.getChild (i);

        if (child.hasType ("ACTOR"))
        {
            const bool isReceiver = child.getProperty ("type").toString() == "receiver";
            const int index = (int) child.getProperty ("index", 0);
            if (index < 0 || index >= maxSceneObjects)
                continue;

            auto& actor = at (isReceiver, index);
            actor.kind = actorKindFromString (child.getProperty ("kind").toString(), actor.kind);
            actor.name = child.getProperty ("name").toString();
            actor.yawDegrees = (float) (double) child.getProperty ("yaw", 0.0);
            actor.scale = juce::jlimit (0.25f, 3.0f,
                                        (float) (double) child.getProperty ("scale", 1.0));
        }
        else if (child.hasType ("MARKER"))
        {
            MarkerInfo marker;
            marker.time = (double) child.getProperty ("time", 0.0);
            marker.label = child.getProperty ("label").toString();
            markers.push_back (marker);
        }
        else if (child.hasType ("UI"))
        {
            ui.showTimes = (bool) child.getProperty ("showTimes", true);
            ui.showGrid = (bool) child.getProperty ("showGrid", true);
            ui.showLabels = (bool) child.getProperty ("showLabels", true);
            ui.snap = (bool) child.getProperty ("snap", false);
            ui.fontScale = juce::jlimit (0.8f, 1.35f,
                                         (float) (double) child.getProperty ("fontScale", 1.0));
        }
    }
}

} // namespace studio
