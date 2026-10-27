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

#include "PluginEditor.h"

namespace
{
    constexpr int minimumWidth = 1180;
    constexpr int minimumHeight = 720;
    constexpr int maximumWidth = 3840;
    constexpr int maximumHeight = 2160;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
#ifndef PLUGIN_EDITOR_DISABLE_OPENGL
    openGLContext.setMultisamplingEnabled (true);
    openGLContext.attachTo (*this);
#endif

    shell = std::make_unique<StudioShell> (processor);
    addAndMakeVisible (shell.get());

    setResizable (true, true);
    setResizeLimits (minimumWidth, minimumHeight, maximumWidth, maximumHeight);

    /* Prefer the studio layout, but never open larger than the display's
       usable area on smaller screens. */
    int width = StudioShell::defaultWidth;
    int height = StudioShell::defaultHeight;

    if (auto* display = juce::Desktop::getInstance().getDisplays()
                            .getPrimaryDisplay())
    {
        width = juce::jmin (width,
                            juce::jmax (minimumWidth,
                                        display->userArea.getWidth() - 40));
        height = juce::jmin (height,
                             juce::jmax (minimumHeight,
                                         display->userArea.getHeight() - 40));
    }

    setSize (width, height);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (studio::Colours::bg);
}

void PluginEditor::resized()
{
    if (shell != nullptr)
        shell->setBounds (getLocalBounds());
}
