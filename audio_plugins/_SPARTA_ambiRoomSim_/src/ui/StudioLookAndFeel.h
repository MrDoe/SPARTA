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

#include "StudioTheme.h"

/* Look-and-feel for the studio: flat surfaces, cyan focus, compact sliders
   with a read-only value box, and switch-style toggles. */
class StudioLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel()
    {
        using namespace studio::Colours;

        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::PopupMenu::backgroundColourId, panelRaised);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, textSecondary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, surfaceHover);
        setColour (juce::PopupMenu::highlightedTextColourId, text);

        setColour (juce::TextEditor::backgroundColourId, surface);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, strokeStrong);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, accentDim);
        setColour (juce::TextEditor::highlightedTextColourId, text);
        setColour (juce::CaretComponent::caretColourId, accent);

        setColour (juce::ComboBox::backgroundColourId, surface);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, strokeStrong);
        setColour (juce::ComboBox::arrowColourId, textSecondary);
        setColour (juce::ComboBox::focusedOutlineColourId, accent);

        setColour (juce::Label::textColourId, text);
        setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);

        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, surface);
        setColour (juce::Slider::textBoxOutlineColourId, strokeStrong);
        setColour (juce::Slider::textBoxHighlightColourId, accentDim);
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Slider::trackColourId, accent.withAlpha (0.70f));
        setColour (juce::Slider::backgroundColourId, strokeStrong);

        setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, strokeStrong);

        setColour (juce::TooltipWindow::backgroundColourId, panelRaised);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, strokeStrong);

        setColour (juce::ToggleButton::textColourId, textSecondary);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, textDisabled);

        setColour (juce::TextButton::buttonColourId, panelRaised);
        setColour (juce::TextButton::buttonOnColourId, accentDim);
        setColour (juce::TextButton::textColourOffId, textSecondary);
        setColour (juce::TextButton::textColourOnId, text);

        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, strokeStrong);

        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::ListBox::textColourId, text);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return studio::uiFont (juce::jlimit (10.0f, 12.5f, (float) buttonHeight * 0.42f));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return studio::uiFont (juce::jlimit (10.0f, 12.5f, (float) box.getHeight() * 0.42f));
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return studio::uiFont (10.5f);
    }

    juce::Font getPopupMenuFont() override
    {
        return studio::uiFont (11.5f);
    }

    juce::Font getAlertWindowMessageFont() override
    {
        return studio::uiFont (12.5f);
    }

    int getSliderThumbRadius (juce::Slider&) override
    {
        return 5;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool isHighlighted,
                               bool isDown) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = button.getToggleState();

        juce::Colour fill = on ? studio::Colours::accentDim
                               : (isDown ? studio::Colours::surfaceHover
                                         : (isHighlighted ? studio::Colours::surface
                                                          : studio::Colours::panelRaised));

        g.setColour (fill);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (button.hasKeyboardFocus (false) ? studio::Colours::accent
                                                     : studio::Colours::strokeStrong);
        g.drawRoundedRectangle (r, 4.0f,
                                button.hasKeyboardFocus (false) ? 1.5f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        g.setColour (button.getToggleState() ? studio::Colours::text
                                             : studio::Colours::textSecondary);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (8, 0),
                          juce::Justification::centred, 1, 0.9f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal
            && style != juce::Slider::LinearBar
            && style != juce::Slider::LinearBarVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                    sliderPos, minSliderPos,
                                                    maxSliderPos, style, slider);
            return;
        }

        if (style == juce::Slider::LinearBarVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                    sliderPos, minSliderPos,
                                                    maxSliderPos, style, slider);
            return;
        }

        const float trackY = (float) y + (float) height * 0.5f;
        const juce::Rectangle<float> track ((float) x, trackY - 1.5f,
                                            (float) width, 3.0f);

        g.setColour (studio::Colours::strokeStrong);
        g.fillRoundedRectangle (track, 1.5f);

        const float filledWidth = juce::jlimit (0.0f, (float) width, sliderPos - (float) x);
        if (filledWidth > 0.5f)
        {
            g.setColour (studio::Colours::accent.withAlpha (0.72f));
            g.fillRoundedRectangle (track.withWidth (filledWidth), 1.5f);
        }

        const float thumbRadius = 5.0f;
        g.setColour (studio::Colours::bg);
        g.fillEllipse (sliderPos - thumbRadius, trackY - thumbRadius,
                       thumbRadius * 2.0f, thumbRadius * 2.0f);
        g.setColour (studio::Colours::accent);
        g.drawEllipse (sliderPos - thumbRadius, trackY - thumbRadius,
                       thumbRadius * 2.0f, thumbRadius * 2.0f, 1.5f);

        if (slider.hasKeyboardFocus (false))
        {
            g.setColour (studio::Colours::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (juce::Rectangle<float> ((float) x - 3.0f,
                                                            trackY - 8.0f,
                                                            (float) width + 6.0f,
                                                            16.0f),
                                    4.0f, 1.0f);
        }
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (studio::Colours::surface);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (false) ? studio::Colours::accent
                                                  : studio::Colours::strokeStrong);
        g.drawRoundedRectangle (r, 4.0f, box.hasKeyboardFocus (false) ? 1.5f : 1.0f);

        juce::Path arrow;
        const float centreX = (float) buttonX + (float) buttonW * 0.5f;
        const float centreY = (float) buttonY + (float) buttonH * 0.5f;
        arrow.startNewSubPath (centreX - 3.5f, centreY - 1.5f);
        arrow.lineTo (centreX, centreY + 2.0f);
        arrow.lineTo (centreX + 3.5f, centreY - 1.5f);

        g.setColour (studio::Colours::textSecondary);
        g.strokePath (arrow, juce::PathStrokeType (1.4f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool isHighlighted, bool) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = button.getToggleState();

        auto switchRect = juce::Rectangle<float> (r.getX() + 1.0f,
                                                  r.getCentreY() - 8.0f,
                                                  30.0f, 16.0f);
        g.setColour (on ? studio::Colours::accentDim
                        : (isHighlighted ? studio::Colours::surfaceHover
                                         : studio::Colours::surface));
        g.fillRoundedRectangle (switchRect, 8.0f);
        g.setColour (on ? studio::Colours::accent.withAlpha (0.9f)
                        : studio::Colours::strokeStrong);
        g.drawRoundedRectangle (switchRect.reduced (0.5f), 8.0f, 1.0f);

        const float knobX = on ? switchRect.getRight() - 13.0f
                               : switchRect.getX() + 3.0f;
        g.setColour (on ? studio::Colours::accent : studio::Colours::textMuted);
        g.fillEllipse (knobX, switchRect.getCentreY() - 5.0f, 10.0f, 10.0f);

        g.setColour (on ? studio::Colours::text : studio::Colours::textSecondary);
        g.setFont (studio::uiFont (11.0f, on));
        g.drawText (button.getButtonText(),
                    r.withTrimmedLeft (38.0f),
                    juce::Justification::centredLeft, true);

        if (button.hasKeyboardFocus (false))
        {
            g.setColour (studio::Colours::accent);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.5f);
        }
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y,
                        int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override
    {
        const auto track = juce::Rectangle<int> (x, y, width, height);
        g.setColour (studio::Colours::bg.withAlpha (0.4f));
        g.fillRoundedRectangle (track.toFloat().reduced (2.0f), 2.0f);

        if (thumbSize <= 0)
            return;

        const auto thumb = isScrollbarVertical
                             ? juce::Rectangle<int> (x + 2, thumbStartPosition,
                                                     juce::jmax (2, width - 4), thumbSize)
                             : juce::Rectangle<int> (thumbStartPosition, y + 2,
                                                     thumbSize, juce::jmax (2, height - 4));

        g.setColour (isMouseDown ? studio::Colours::accent.withAlpha (0.55f)
                                 : (isMouseOver ? studio::Colours::strokeStrong
                                                : studio::Colours::strokeStrong.withAlpha (0.6f)));
        g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
    }
};
