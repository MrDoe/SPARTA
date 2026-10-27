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

#include "SourceBar.h"
#include "StudioTheme.h"

namespace
{
    constexpr int padding = 12;
    constexpr int titleHeight = 22;
    constexpr int rowHeight = 26;
    constexpr int rowGap = 4;
    constexpr int chipGap = 5;
    constexpr int maxObjects = 16;

    juce::String orderName (PluginProcessor& processor)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (
                processor.parameters.getParameter ("outputOrder")))
            return parameter->getCurrentChoiceName();

        return "3rd order";
    }
}

SourceBar::SourceBar (PluginProcessor& p)
    : processor (p)
{
    setOpaque (true);
    setTooltip ("Sources and listeners in the scene. Click to select; "
                "the + chip adds another one.");
    refresh();
}

SourceBar::~SourceBar() = default;

void SourceBar::paint (juce::Graphics& g)
{
    g.fillAll (studio::Colours::panel);

    g.setColour (studio::Colours::stroke);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    g.setColour (studio::Colours::textSecondary);
    g.setFont (studio::uiFont (9.5f, true));
    g.drawText ("MIXER & ROUTING",
                juce::Rectangle<int> (padding, 3, 220, 16),
                juce::Justification::centredLeft, false);

    g.setColour (studio::Colours::textMuted);
    g.setFont (studio::uiFont (9.5f));
    g.drawText (buildSummary(),
                juce::Rectangle<int> (padding + 110, 3,
                                      juce::jmax (0, getWidth() - padding * 2 - 110),
                                      16),
                juce::Justification::centredRight, true);

    for (size_t i = 0; i < chips.size(); ++i)
    {
        const auto& chip = chips[i];
        const bool selected = (chip.selection == selection);
        const bool hovered = ((int) i == hoveredChip);

        auto r = chip.bounds.toFloat().reduced (0.5f);

        g.setColour (selected ? studio::Colours::accentDim
                              : (hovered ? studio::Colours::surface
                                         : studio::Colours::panelRaised));
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (selected ? studio::Colours::accent
                              : studio::Colours::strokeStrong);
        g.drawRoundedRectangle (r, 4.0f, selected ? 1.5f : 1.0f);

        g.setColour (chip.colour);
        g.fillEllipse ((float) chip.bounds.getX() + 8.0f,
                       (float) chip.bounds.getCentreY() - 2.5f, 5.0f, 5.0f);

        g.setColour (selected ? studio::Colours::text
                              : studio::Colours::textSecondary);
        g.setFont (studio::uiFont (10.0f, true));
        g.drawText (chip.tag,
                    chip.bounds.withTrimmedLeft (18)
                               .withWidth (juce::jmin (30, chip.bounds.getWidth() - 18)),
                    juce::Justification::centredLeft, false);

        if (chip.bounds.getWidth() >= 82 && chip.name.isNotEmpty())
        {
            g.setColour (selected ? studio::Colours::text
                                  : studio::Colours::textMuted);
            g.setFont (studio::uiFont (9.5f));
            g.drawText (chip.name,
                        chip.bounds.withTrimmedLeft (48)
                                   .withTrimmedRight (7),
                        juce::Justification::centredLeft, true);
        }
    }

    auto drawAdd = [&] (const AddButton& add)
    {
        if (! add.visible)
            return;

        auto r = add.bounds.toFloat().reduced (0.5f);
        g.setColour (studio::Colours::surface.withAlpha (0.45f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (studio::Colours::strokeStrong);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        const auto icon = studio::makeIcon (studio::Icon::plus, 10.0f);
        const float iconX = (float) add.bounds.getX() + 10.0f;
        const float iconY = (float) add.bounds.getCentreY() - 5.0f;
        g.setColour (studio::Colours::textMuted);
        g.strokePath (icon, juce::PathStrokeType (1.3f),
                      juce::AffineTransform::translation (iconX, iconY));

        if (add.bounds.getWidth() >= 52)
        {
            g.setColour (studio::Colours::textMuted);
            g.setFont (studio::uiFont (9.5f));
            g.drawText ("Add",
                        add.bounds.withTrimmedLeft (24),
                        juce::Justification::centredLeft, false);
        }
    };

    drawAdd (sourceAdd);
    drawAdd (receiverAdd);
}

void SourceBar::resized()
{
    rebuild();
}

void SourceBar::mouseDown (const juce::MouseEvent& e)
{
    for (const auto& add : { sourceAdd, receiverAdd })
    {
        if (add.visible && add.bounds.contains (e.getPosition()))
        {
            addObject (add.isReceiver);
            return;
        }
    }

    for (const auto& chip : chips)
    {
        if (chip.bounds.contains (e.getPosition()))
        {
            selection = chip.selection;
            repaint();
            if (onSelectionChanged)
                onSelectionChanged (selection);
            return;
        }
    }
}

void SourceBar::mouseMove (const juce::MouseEvent& e)
{
    const int hit = hitTest (e.getPosition());

    if (hit != hoveredChip)
    {
        hoveredChip = hit;
        repaint();
    }

    setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::NormalCursor);
}

void SourceBar::mouseExit (const juce::MouseEvent&)
{
    if (hoveredChip >= 0)
    {
        hoveredChip = -1;
        repaint();
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SourceBar::setSelection (const studio::Selection& newSelection)
{
    if (selection == newSelection)
        return;

    selection = newSelection;
    repaint();
}

void SourceBar::refresh()
{
    const auto currentSignature = buildSignature();

    if (currentSignature == signature)
        return;

    signature = currentSignature;
    rebuild();
    repaint();
}

juce::String SourceBar::buildSignature() const
{
    juce::String signatureText;
    const int numSources = juce::jlimit (1, maxObjects,
                                         processor.getParameterInt ("numSources"));
    const int numReceivers = juce::jlimit (1, maxObjects,
                                           processor.getParameterInt ("numReceivers"));

    signatureText << numSources << "/" << numReceivers << "/"
                  << processor.getParameterChoice ("outputOrder") << "/"
                  << processor.getParameterInt ("maxReflectionOrder") << "/"
                  << (processor.getParameterBool ("enableReflections") ? 1 : 0);

    {
        const juce::ScopedLock sl (processor.getSceneLock());
        const auto& scene = processor.getSceneModel();

        for (int side = 0; side < 2; ++side)
        {
            const bool isReceiver = side == 1;
            const int count = isReceiver ? numReceivers : numSources;

            for (int i = 0; i < count; ++i)
            {
                const auto& actor = scene.getActor (isReceiver, i);
                signatureText << "/" << (int) actor.kind << ":"
                              << actor.name;
            }
        }
    }

    return signatureText;
}

juce::String SourceBar::buildSummary() const
{
    const int numSources = juce::jlimit (1, maxObjects,
                                         processor.getParameterInt ("numSources"));
    const int numReceivers = juce::jlimit (1, maxObjects,
                                           processor.getParameterInt ("numReceivers"));

    juce::String summary;
    summary << numSources << (numSources == 1 ? " source" : " sources")
            << "  /  "
            << numReceivers << (numReceivers == 1 ? " listener" : " listeners")
            << "  ->  " << orderName (processor)
            << " HOA";

    if (! processor.getParameterBool ("enableReflections"))
        summary << "  /  reflections off";

    return summary;
}

void SourceBar::rebuild()
{
    chips.clear();

    const int numSources = juce::jlimit (1, maxObjects,
                                         processor.getParameterInt ("numSources"));
    const int numReceivers = juce::jlimit (1, maxObjects,
                                           processor.getParameterInt ("numReceivers"));

    const int available = juce::jmax (120, getWidth() - padding * 2);

    auto layoutRow = [&] (bool isReceiver, int count, int y, AddButton& add)
    {
        add = AddButton();
        add.isReceiver = isReceiver;

        const int slots = count + (count < maxObjects ? 1 : 0);
        const int width = juce::jlimit (40, 118,
                                        (available - chipGap * (slots - 1))
                                            / juce::jmax (1, slots));

        int x = padding;

        for (int i = 0; i < count; ++i)
        {
            studio::ActorInfo actor;
            {
                const juce::ScopedLock sl (processor.getSceneLock());
                actor = processor.getSceneModel().getActor (isReceiver, i);
            }

            Chip chip;
            chip.bounds = { x, y, width, rowHeight };
            chip.selection = isReceiver ? studio::Selection::receiver (i)
                                        : studio::Selection::source (i);
            chip.tag = juce::String (isReceiver ? "R" : "S") + juce::String (i + 1);
            chip.name = actor.name.isNotEmpty() ? actor.name
                                                : studio::toString (actor.kind);
            chip.colour = getPathColour (isReceiver, i);
            chips.push_back (chip);

            x += width + chipGap;
        }

        if (count < maxObjects)
        {
            add.visible = true;
            add.bounds = { x, y, juce::jmax (26, width), rowHeight };
        }
    };

    layoutRow (false, numSources, titleHeight + 2, sourceAdd);
    layoutRow (true, numReceivers,
               titleHeight + 2 + rowHeight + rowGap, receiverAdd);

    signature = buildSignature();
}

void SourceBar::addObject (bool isReceiver)
{
    const int count = processor.getParameterInt (isReceiver ? "numReceivers"
                                                            : "numSources");

    if (count >= maxObjects)
        return;

    const int newIndex = count;
    processor.setParameterValue (isReceiver ? "numReceivers" : "numSources",
                                 count + 1);

    /* Keep the new object inside the current room, regardless of the fixed
       default position of that slot. */
    const juce::String prefix = isReceiver ? "receiver" : "source";
    const float roomX = processor.getParameterFloat ("roomX");
    const float roomY = processor.getParameterFloat ("roomY");
    const float roomZ = processor.getParameterFloat ("roomZ");

    auto clamp = [&] (char axis, float maximum)
    {
        const auto id = prefix
                      + juce::String::charToString ((juce_wchar) axis)
                      + juce::String (newIndex);
        processor.setParameterValue (id, juce::jlimit (0.0f, maximum,
                                                       processor.getParameterFloat (id)));
    };

    clamp ('X', roomX);
    clamp ('Y', roomY);
    clamp ('Z', roomZ);

    selection = isReceiver ? studio::Selection::receiver (newIndex)
                           : studio::Selection::source (newIndex);

    if (onSelectionChanged)
        onSelectionChanged (selection);

    if (onSceneChanged)
        onSceneChanged();

    rebuild();
    repaint();
}

int SourceBar::hitTest (juce::Point<int> position) const
{
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i].bounds.contains (position))
            return (int) i;

    return -1;
}
