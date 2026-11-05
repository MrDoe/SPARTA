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
#include <functional>
#include <vector>

#include "../PluginProcessor.h"
#include "SelectionState.h"

/* Read-only mixer strip: one chip per source and listener with the same
   colour and name used in the 3D canvas. There are deliberately no meters or
   gain controls here; the DSP has no per-source gain stage. */
class SourceBar : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    explicit SourceBar (PluginProcessor& processor);
    ~SourceBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    void setSelection (const studio::Selection& newSelection);
    const studio::Selection& getSelection() const { return selection; }

    /* Re-reads the source/listener counts and the scene cast. Repaints only
       when something actually changed. */
    void refresh();

    std::function<void (studio::Selection)> onSelectionChanged;
    std::function<void()> onSceneChanged;

    static constexpr int preferredHeight = 92;

private:
    struct Chip
    {
        juce::Rectangle<int> bounds;
        studio::Selection selection;
        juce::String tag;
        juce::String name;
        juce::Colour colour;
    };

    struct AddButton
    {
        juce::Rectangle<int> bounds;
        bool visible = false;
        bool isReceiver = false;
    };

    PluginProcessor& processor;
    studio::Selection selection;
    std::vector<Chip> chips;
    AddButton sourceAdd;
    AddButton receiverAdd;
    int hoveredChip = -1;
    juce::String signature;

    void rebuild();
    juce::String buildSignature() const;
    juce::String buildSummary() const;
    void addObject (bool isReceiver);

    int hitTest (juce::Point<int> position) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceBar)
};
