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
#include "SelectionState.h"

/* Context-sensitive right-hand panel: room geometry, surface absorption,
   the selected actor and its path window, and the output/engine section. */
class InspectorPanel : public juce::Component
{
public:
    enum class Tab
    {
        inspector,
        room,
        motion
    };

    explicit InspectorPanel (PluginProcessor& processor);
    ~InspectorPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSelection (const studio::Selection& selection);
    const studio::Selection& getSelection() const { return selection; }

    /* Updates the read-only values (path range, checkpoint time, ...). */
    void refresh();

    /* Forces a rebuild, e.g. after a host state restore. */
    void rebuild();

    std::function<void()> onEdited;
    std::function<void()> onFrameRequested;
    std::function<void()> onStructureChanged;
    std::function<void (studio::Selection)> onSelectionRequested;

private:
    class Content;

    PluginProcessor& processor;
    studio::Selection selection;
    Tab activeTab = Tab::inspector;
    juce::TextButton inspectorTabButton { "Inspector" };
    juce::TextButton roomTabButton { "Room" };
    juce::TextButton motionTabButton { "Motion" };
    juce::Viewport viewport;
    std::unique_ptr<Content> content;

    juce::String getSelectionTitle() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
};
