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
#include "Mesh3D.h"

namespace studio
{

/* The built-in actor meshes. They are generated procedurally at load time,
   so the plug-in ships no binary assets and needs no importer. */
enum class ActorKind
{
    performer = 0,
    listener,
    speaker,
    car,
    bird,
    drone,
    marker,
    numKinds
};

struct ActorDefinition
{
    ActorKind kind = ActorKind::performer;
    juce::String name;
    /* True for actors that are positioned at their acoustic anchor in mid-air
       (bird, drone, pin). Grounded actors stand on the room floor, with the
       acoustic point shown separately. */
    bool aerial = false;
    /* Approximate world radius, used for selection rings and floor shadows. */
    float footprint = 0.4f;
    /* Height of the visual above the anchor / floor (used for label placement). */
    float labelHeight = 1.9f;
    Mesh mesh;
};

juce::Array<ActorKind> allActorKinds();
const ActorDefinition& getActorDefinition (ActorKind kind);

juce::String toString (ActorKind kind);
ActorKind actorKindFromString (const juce::String& text,
                               ActorKind fallback = ActorKind::performer);

} // namespace studio
