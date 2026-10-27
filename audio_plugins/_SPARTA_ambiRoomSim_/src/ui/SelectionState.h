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

namespace studio
{

/* One shared selection across the canvas, inspector, source bar and timeline.
   Checkpoint selection is reserved for the 3D path editor; keyframe is the
   index into that object's PathData. */
struct Selection
{
    enum class Type { none, source, receiver, checkpoint };

    Type type = Type::none;
    int index = 0;
    int keyframe = -1;

    static Selection none()
    {
        return {};
    }

    static Selection source (int sourceIndex)
    {
        Selection s;
        s.type = Type::source;
        s.index = sourceIndex;
        return s;
    }

    static Selection receiver (int receiverIndex)
    {
        Selection s;
        s.type = Type::receiver;
        s.index = receiverIndex;
        return s;
    }

    static Selection checkpoint (bool isReceiver, int objectIndex, int keyframeIndex)
    {
        Selection s;
        s.type = Type::checkpoint;
        s.index = objectIndex;
        s.keyframe = keyframeIndex;
        s.checkpointIsReceiver = isReceiver;
        return s;
    }

    bool isActor() const { return type == Type::source || type == Type::receiver; }
    bool isSource() const { return type == Type::source; }
    bool isReceiver() const { return type == Type::receiver; }
    bool isCheckpoint() const { return type == Type::checkpoint; }
    bool isNone() const { return type == Type::none; }

    bool checkpointIsReceiver = false;

    bool operator== (const Selection& other) const
    {
        return type == other.type
            && index == other.index
            && keyframe == other.keyframe
            && (type != Type::checkpoint
                || checkpointIsReceiver == other.checkpointIsReceiver);
    }

    bool operator!= (const Selection& other) const { return ! (*this == other); }
};

} // namespace studio
