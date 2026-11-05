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

/* Shared design tokens for the AmbiRoomSim studio UI.

   The visual direction is a professional dark creative tool (Adobe Spectrum
   style) rather than a neon HUD: dense but legible controls, one accent for
   selection, one accent for time, and visible focus rings everywhere. All
   colours below were checked against the panel background for at least 4.5:1
   contrast where they carry text. */
namespace studio
{

namespace Colours
{
    const juce::Colour bg           { 0xff071114 };
    const juce::Colour bgRaised     { 0xff0a161b };
    const juce::Colour panel        { 0xff0d1a20 };
    const juce::Colour panelRaised  { 0xff122730 };
    const juce::Colour surface      { 0xff173039 };
    const juce::Colour surfaceHover { 0xff1c3a44 };
    const juce::Colour stroke       { 0x14ffffff }; /* 8% white  */
    const juce::Colour strokeStrong { 0x29ffffff }; /* 16% white */
    const juce::Colour text         { 0xffe9f1f4 };
    const juce::Colour textSecondary{ 0xff9fb3bc };
    const juce::Colour textMuted    { 0xff7c919a };
    const juce::Colour textDisabled { 0xff5a6d75 };

    const juce::Colour accent       { 0xff00d8df }; /* selection / focus */
    const juce::Colour accentDim    { 0x3300d8df };
    const juce::Colour time         { 0xffe08a00 }; /* playhead / time    */
    const juce::Colour timeDim      { 0x33e08a00 };
    const juce::Colour receiver     { 0xffe060a8 };
    const juce::Colour source       { 0xff43d17a };
    const juce::Colour warning      { 0xfff5a524 };
    const juce::Colour danger       { 0xfff05252 };
    const juce::Colour success      { 0xff43d17a };

    const juce::Colour canvasTop    { 0xff0b1a21 };
    const juce::Colour canvasBottom { 0xff071216 };
    const juce::Colour gridMinor    { 0x16ffffff };
    const juce::Colour gridMajor    { 0x2affffff };
    const juce::Colour wallFill     { 0x12ffffff };
    const juce::Colour wallEdge     { 0x3dffffff };
    const juce::Colour floorFill    { 0xff0a171c };
}

/* Global text scale for the studio UI. Every studio font goes through
   uiFont(), so changing this one value scales all labels, values and hints
   without touching the individual controls. The value is persisted in
   UiPreferences::fontScale. */
inline float& uiFontScale()
{
    static float scale = 1.0f;
    return scale;
}

/* Inter is bundled with the SPARTA resources; the plugin already links the
   GUI_fonts target, so no extra asset is needed. */
inline juce::Font uiFont (float size, bool bold = false)
{
    static auto regular = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    static auto boldFace = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);

    const float scaled = juce::jmax (1.0f, size * uiFontScale());

    return juce::Font (juce::FontOptions (bold ? boldFace : regular)
                           .withHeight (scaled)
                           .withKerningFactor (0.01f));
}

/* Time display. Seconds use mm:ss.mmm; PPQ uses bar.beat.ticks (4/4). */
inline juce::String formatTime (double t, bool isPpq)
{
    if (t < 0.0)
        t = 0.0;

    if (isPpq)
    {
        const int bar  = (int) std::floor (t / 4.0) + 1;
        const int beat = (int) std::floor (std::fmod (t, 4.0)) + 1;
        const int tick = (int) std::floor (std::fmod (t, 1.0) * 100.0);
        return juce::String (bar) + "." + juce::String (beat) + "."
             + juce::String (juce::jlimit (0, 99, tick)).paddedLeft ('0', 2);
    }

    const int totalMs = (int) std::floor (t * 1000.0 + 0.5);
    const int minutes = totalMs / 60000;
    const int seconds = (totalMs / 1000) % 60;
    const int millis  = totalMs % 1000;
    return juce::String (minutes).paddedLeft ('0', 2) + ":"
         + juce::String (seconds).paddedLeft ('0', 2) + "."
         + juce::String (millis).paddedLeft ('0', 3);
}

/* Compact form used by keyframe chips in the 3D canvas. */
inline juce::String formatTimeShort (double t, bool isPpq)
{
    if (isPpq)
        return formatTime (t, true);

    const int totalMs = (int) std::floor (t * 1000.0 + 0.5);
    const int minutes = totalMs / 60000;
    const int seconds = (totalMs / 1000) % 60;
    const int millis  = totalMs % 1000;
    return juce::String (minutes) + ":"
         + juce::String (seconds).paddedLeft ('0', 2) + "."
         + juce::String (millis).paddedLeft ('0', 3);
}

inline void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r,
                              const juce::String& title)
{
    g.setColour (Colours::textSecondary);
    g.setFont (uiFont (10.5f, true));
    g.drawText (title.toUpperCase(), r, juce::Justification::centredLeft, false);
}

inline void drawChip (juce::Graphics& g, juce::Rectangle<float> r,
                      const juce::String& text, juce::Colour fill,
                      juce::Colour outline, juce::Colour textColour,
                      float fontHeight = 10.0f)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    g.setColour (textColour);
    g.setFont (uiFont (fontHeight));
    g.drawText (text, r.reduced (5.0f, 0.0f), juce::Justification::centred, false);
}

/* ---------------------------------------------------------------------------
   Vector icons. Drawn on a 24x24 canvas and scaled by the button, so the set
   stays crisp at any DPI and needs no image assets.
   --------------------------------------------------------------------------- */
enum class Icon
{
    cursor, plus, bezier, shapes, orbit, fly, settings, help,
    fullscreen, focus, chevronDown, chevronRight, play, pause, stop, loop,
    magnet, eye, lock, listener, source, trash, close
};

inline juce::Path makeIcon (Icon icon, float size)
{
    juce::Path p;
    const float s = size / 24.0f;

    switch (icon)
    {
        case Icon::cursor:
            p.startNewSubPath (5.0f, 3.0f);
            p.lineTo (19.0f, 12.0f);
            p.lineTo (12.4f, 13.3f);
            p.lineTo (9.2f, 20.0f);
            p.closeSubPath();
            break;

        case Icon::plus:
            p.startNewSubPath (12.0f, 4.0f);  p.lineTo (12.0f, 20.0f);
            p.startNewSubPath (4.0f, 12.0f);  p.lineTo (20.0f, 12.0f);
            break;

        case Icon::bezier:
            p.startNewSubPath (4.0f, 18.0f);
            p.cubicTo (8.0f, 4.0f, 16.0f, 20.0f, 20.0f, 6.0f);
            p.addEllipse (2.5f, 16.5f, 3.0f, 3.0f);
            p.addEllipse (18.5f, 4.5f, 3.0f, 3.0f);
            break;

        case Icon::shapes:
            p.addEllipse (3.0f, 3.0f, 9.0f, 9.0f);
            p.addRectangle (12.0f, 12.0f, 9.0f, 9.0f);
            break;

        case Icon::orbit:
            p.addEllipse (3.0f, 7.0f, 18.0f, 10.0f);
            p.addEllipse (10.0f, 10.0f, 4.0f, 4.0f);
            p.startNewSubPath (18.0f, 4.0f);
            p.lineTo (21.0f, 8.0f);
            p.lineTo (16.5f, 8.5f);
            break;

        case Icon::fly:
            p.startNewSubPath (12.0f, 2.5f);
            p.lineTo (14.2f, 10.0f);
            p.lineTo (22.0f, 13.0f);
            p.lineTo (14.2f, 14.5f);
            p.lineTo (12.0f, 21.5f);
            p.lineTo (9.8f, 14.5f);
            p.lineTo (2.0f, 13.0f);
            p.lineTo (9.8f, 10.0f);
            p.closeSubPath();
            break;

        case Icon::settings:
            p.addEllipse (9.0f, 9.0f, 6.0f, 6.0f);
            p.startNewSubPath (3.0f, 6.0f);  p.lineTo (21.0f, 6.0f);
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (6.4f, 12.0f);
            p.startNewSubPath (3.0f, 18.0f); p.lineTo (21.0f, 18.0f);
            p.addEllipse (6.0f, 4.4f, 3.2f, 3.2f);
            p.addEllipse (14.8f, 16.4f, 3.2f, 3.2f);
            break;

        case Icon::help:
            p.addEllipse (4.0f, 4.0f, 16.0f, 16.0f);
            break;

        case Icon::fullscreen:
            p.startNewSubPath (4.0f, 9.0f);  p.lineTo (4.0f, 4.0f);  p.lineTo (9.0f, 4.0f);
            p.startNewSubPath (15.0f, 4.0f); p.lineTo (20.0f, 4.0f); p.lineTo (20.0f, 9.0f);
            p.startNewSubPath (20.0f, 15.0f);p.lineTo (20.0f, 20.0f);p.lineTo (15.0f, 20.0f);
            p.startNewSubPath (9.0f, 20.0f); p.lineTo (4.0f, 20.0f); p.lineTo (4.0f, 15.0f);
            break;

        case Icon::focus:
            p.addRectangle (4.0f, 4.0f, 16.0f, 16.0f);
            p.addRectangle (8.5f, 8.5f, 7.0f, 7.0f);
            break;

        case Icon::chevronDown:
            p.startNewSubPath (6.0f, 9.0f);
            p.lineTo (12.0f, 15.0f);
            p.lineTo (18.0f, 9.0f);
            break;

        case Icon::chevronRight:
            p.startNewSubPath (9.0f, 6.0f);
            p.lineTo (15.0f, 12.0f);
            p.lineTo (9.0f, 18.0f);
            break;

        case Icon::play:
            p.startNewSubPath (8.0f, 5.0f);
            p.lineTo (19.0f, 12.0f);
            p.lineTo (8.0f, 19.0f);
            p.closeSubPath();
            break;

        case Icon::pause:
            p.addRectangle (7.0f, 5.0f, 3.5f, 14.0f);
            p.addRectangle (13.5f, 5.0f, 3.5f, 14.0f);
            break;

        case Icon::stop:
            p.addRectangle (6.0f, 6.0f, 12.0f, 12.0f);
            break;

        case Icon::loop:
            p.startNewSubPath (5.0f, 9.0f);
            p.cubicTo (5.0f, 5.0f, 19.0f, 5.0f, 19.0f, 9.0f);
            p.lineTo (19.0f, 15.0f);
            p.cubicTo (19.0f, 19.0f, 5.0f, 19.0f, 5.0f, 15.0f);
            p.startNewSubPath (8.0f, 12.0f); p.lineTo (5.0f, 9.0f); p.lineTo (2.0f, 12.0f);
            break;

        case Icon::magnet:
            p.addArc (5.0f, 4.0f, 14.0f, 14.0f, 0.0f, juce::MathConstants<float>::pi, true);
            p.startNewSubPath (5.0f, 11.0f);  p.lineTo (5.0f, 20.0f);
            p.startNewSubPath (19.0f, 11.0f); p.lineTo (19.0f, 20.0f);
            break;

        case Icon::eye:
            p.startNewSubPath (2.5f, 12.0f);
            p.cubicTo (7.0f, 4.5f, 17.0f, 4.5f, 21.5f, 12.0f);
            p.cubicTo (17.0f, 19.5f, 7.0f, 19.5f, 2.5f, 12.0f);
            p.addEllipse (9.5f, 9.5f, 5.0f, 5.0f);
            break;

        case Icon::lock:
            p.addRoundedRectangle (5.0f, 10.0f, 14.0f, 10.0f, 2.0f, 2.0f);
            p.startNewSubPath (8.0f, 10.0f);
            p.cubicTo (8.0f, 3.5f, 16.0f, 3.5f, 16.0f, 10.0f);
            break;

        case Icon::listener:
            p.addEllipse (8.0f, 3.0f, 8.0f, 8.0f);
            p.startNewSubPath (5.0f, 21.0f);
            p.cubicTo (5.0f, 13.0f, 19.0f, 13.0f, 19.0f, 21.0f);
            break;

        case Icon::source:
            p.addEllipse (4.0f, 4.0f, 16.0f, 16.0f);
            p.addEllipse (10.0f, 10.0f, 4.0f, 4.0f);
            break;

        case Icon::trash:
            p.addRectangle (6.0f, 7.0f, 12.0f, 14.0f);
            p.startNewSubPath (4.0f, 7.0f); p.lineTo (20.0f, 7.0f);
            p.startNewSubPath (9.0f, 4.0f); p.lineTo (15.0f, 4.0f);
            break;

        case Icon::close:
            p.startNewSubPath (6.0f, 6.0f);   p.lineTo (18.0f, 18.0f);
            p.startNewSubPath (18.0f, 6.0f);  p.lineTo (6.0f, 18.0f);
            break;
    }

    p.applyTransform (juce::AffineTransform::scale (s));
    return p;
}

/* Small square button used by the top bar and the tool rail. */
class IconButton : public juce::Button
{
public:
    IconButton (Icon iconToUse, const juce::String& tooltip, bool withText = false)
        : juce::Button (tooltip), icon (iconToUse), showText (withText)
    {
        setTooltip (tooltip);
        setWantsKeyboardFocus (true);
    }

    void setIcon (Icon newIcon) { icon = newIcon; repaint(); }
    void setText (const juce::String& t) { text = t; repaint(); }
    void setSelected (bool shouldBeSelected) { selected = shouldBeSelected; repaint(); }
    bool isSelected() const { return selected; }

    void paintButton (juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        auto fill = selected ? Colours::accentDim
                             : (isPressed ? Colours::surfaceHover
                                          : (isHovered ? Colours::surface
                                                       : juce::Colours::transparentBlack));
        if (! fill.isTransparent())
        {
            g.setColour (fill);
            g.fillRoundedRectangle (r, 4.0f);
        }

        if (selected)
        {
            g.setColour (Colours::accent);
            g.fillRoundedRectangle (r.withWidth (2.0f), 1.0f);
        }

        const bool hasText = showText && text.isNotEmpty();
        auto iconArea = hasText ? r.removeFromLeft (22.0f)
                                : r.withSizeKeepingCentre (16.0f, 16.0f);

        g.setColour (isEnabled() ? (selected ? Colours::accent : Colours::textSecondary)
                                 : Colours::textDisabled);
        g.strokePath (makeIcon (icon, 16.0f),
                      juce::PathStrokeType (1.5f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (
                          iconArea.getCentreX() - 8.0f,
                          iconArea.getCentreY() - 8.0f));

        if (hasText)
        {
            g.setColour (selected ? Colours::text : Colours::textSecondary);
            g.setFont (uiFont (11.0f, selected));
            g.drawText (text, r.withTrimmedLeft (4.0f),
                        juce::Justification::centredLeft, true);
        }

        if (hasKeyboardFocus (false))
        {
            g.setColour (Colours::accent);
            g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.5f);
        }
    }

private:
    Icon icon;
    juce::String text;
    bool showText = false;
    bool selected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
};

} // namespace studio
