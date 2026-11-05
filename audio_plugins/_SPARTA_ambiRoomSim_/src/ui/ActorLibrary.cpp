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

#include "ActorLibrary.h"

#include <array>

namespace studio
{

/* ---------------------------------------------------------------------------
   Low-poly actor meshes. Everything is authored in metres, z up, facing +y.
   Grounded actors have their feet at z = 0; aerial actors are centred on the
   acoustic point. Colour is baked per face so the meshes can be rendered
   without a material system.
   ------------------------------------------------------------------------- */

namespace
{
    const juce::Colour skinLight  { 0xffc9a07a };
    const juce::Colour skinWarm   { 0xffbd9370 };
    const juce::Colour hairDark   { 0xff2f2620 };
    const juce::Colour shoeDark   { 0xff20282d };
    const juce::Colour metalDark  { 0xff39464e };
    const juce::Colour metalLight { 0xff72838d };
    const juce::Colour glassBlue  { 0xff3c5c6b };

    Mesh buildPerformer()
    {
        Mesh m;
        const juce::Colour trousers { 0xff394854 };
        const juce::Colour shirt    { 0xff61869a };

        /* shoes and legs */
        addBox (m, { -0.10f, 0.055f, 0.045f }, { 0.15f, 0.27f, 0.09f }, shoeDark);
        addBox (m, {  0.10f, 0.055f, 0.045f }, { 0.15f, 0.27f, 0.09f }, shoeDark);
        addBox (m, { -0.10f, 0.0f, 0.44f }, { 0.14f, 0.16f, 0.72f }, trousers);
        addBox (m, {  0.10f, 0.0f, 0.44f }, { 0.14f, 0.16f, 0.72f }, trousers);

        /* hips and torso */
        addBox (m, { 0.0f, 0.0f, 0.85f }, { 0.36f, 0.22f, 0.18f }, trousers.darker (0.08f));
        addBox (m, { 0.0f, 0.0f, 1.12f }, { 0.42f, 0.24f, 0.44f }, shirt);
        addBox (m, { 0.0f, 0.125f, 1.16f }, { 0.16f, 0.015f, 0.28f }, shirt.brighter (0.10f));
        addBox (m, { 0.0f, 0.0f, 1.35f }, { 0.50f, 0.26f, 0.10f }, shirt.darker (0.04f));

        /* arms and hands */
        addBox (m, { -0.27f, 0.0f, 1.13f }, { 0.10f, 0.12f, 0.54f }, shirt.darker (0.08f));
        addBox (m, {  0.27f, 0.0f, 1.13f }, { 0.10f, 0.12f, 0.54f }, shirt.darker (0.08f));
        addSphere (m, { -0.27f, 0.0f, 0.83f }, 0.062f, 8, 5, skinWarm);
        addSphere (m, {  0.27f, 0.0f, 0.83f }, 0.062f, 8, 5, skinWarm);

        /* neck, head, hair, face details */
        addCylinder (m, { 0.0f, 0.0f, 1.46f }, 0.055f, 0.12f, 2, 8, skinWarm);
        addSphere (m, { 0.0f, 0.0f, 1.62f }, { 0.115f, 0.11f, 0.135f }, 10, 6, skinLight);
        addSphere (m, { 0.0f, -0.012f, 1.675f }, { 0.118f, 0.112f, 0.085f }, 10, 4, hairDark);
        addBox (m, { 0.0f, 0.112f, 1.60f }, { 0.034f, 0.05f, 0.05f }, skinLight.brighter (0.06f));
        addBox (m, { -0.045f, 0.105f, 1.647f }, { 0.022f, 0.012f, 0.014f }, shoeDark);
        addBox (m, {  0.045f, 0.105f, 1.647f }, { 0.022f, 0.012f, 0.014f }, shoeDark);

        return m;
    }

    Mesh buildListener()
    {
        Mesh m;
        const juce::Colour clothes { 0xff6f6079 };
        const juce::Colour trousers { 0xff3a3d4b };

        addBox (m, { -0.10f, 0.055f, 0.045f }, { 0.15f, 0.27f, 0.09f }, shoeDark);
        addBox (m, {  0.10f, 0.055f, 0.045f }, { 0.15f, 0.27f, 0.09f }, shoeDark);
        addBox (m, { -0.10f, 0.0f, 0.44f }, { 0.14f, 0.16f, 0.72f }, trousers);
        addBox (m, {  0.10f, 0.0f, 0.44f }, { 0.14f, 0.16f, 0.72f }, trousers);
        addBox (m, { 0.0f, 0.0f, 0.85f }, { 0.36f, 0.22f, 0.18f }, trousers.darker (0.08f));
        addBox (m, { 0.0f, 0.0f, 1.12f }, { 0.42f, 0.24f, 0.44f }, clothes);
        addBox (m, { 0.0f, 0.0f, 1.35f }, { 0.50f, 0.26f, 0.10f }, clothes.darker (0.04f));
        addBox (m, { -0.27f, 0.0f, 1.13f }, { 0.10f, 0.12f, 0.54f }, clothes.darker (0.08f));
        addBox (m, {  0.27f, 0.0f, 1.13f }, { 0.10f, 0.12f, 0.54f }, clothes.darker (0.08f));
        addSphere (m, { -0.27f, 0.0f, 0.83f }, 0.062f, 8, 5, skinWarm);
        addSphere (m, {  0.27f, 0.0f, 0.83f }, 0.062f, 8, 5, skinWarm);

        addCylinder (m, { 0.0f, 0.0f, 1.46f }, 0.055f, 0.12f, 2, 8, skinWarm);
        addSphere (m, { 0.0f, 0.0f, 1.62f }, { 0.115f, 0.11f, 0.135f }, 10, 6, skinWarm);
        addSphere (m, { 0.0f, -0.012f, 1.675f }, { 0.118f, 0.112f, 0.085f }, 10, 4, hairDark);

        /* headphones: band over the head and two cups */
        addBox (m, { 0.0f, -0.005f, 1.755f }, { 0.28f, 0.055f, 0.03f }, metalDark);
        addCylinder (m, { -0.135f, -0.005f, 1.62f }, 0.058f, 0.045f, 0, 10, metalLight);
        addCylinder (m, {  0.135f, -0.005f, 1.62f }, 0.058f, 0.045f, 0, 10, metalLight);

        return m;
    }

    Mesh buildSpeaker()
    {
        Mesh m;
        const juce::Colour cabinet { 0xff2b333a };

        addBox (m, { 0.0f, 0.0f, 0.025f }, { 0.38f, 0.38f, 0.05f }, metalDark);
        addCylinder (m, { 0.0f, 0.0f, 0.40f }, 0.022f, 0.72f, 2, 8, metalLight);
        addBox (m, { 0.0f, 0.0f, 1.08f }, { 0.34f, 0.28f, 0.60f }, cabinet);
        addBox (m, { 0.0f, 0.145f, 1.08f }, { 0.30f, 0.02f, 0.56f }, cabinet.brighter (0.12f));

        /* woofer and tweeter on the +y baffle */
        addCylinder (m, { 0.0f, 0.165f, 0.93f }, 0.105f, 0.035f, 1, 12, juce::Colour (0xff171d21));
        addCylinder (m, { 0.0f, 0.185f, 0.93f }, 0.045f, 0.015f, 1, 10, metalDark);
        addCylinder (m, { 0.0f, 0.165f, 1.23f }, 0.048f, 0.03f, 1, 10, juce::Colour (0xff171d21));

        /* power LED */
        addBox (m, { 0.11f, 0.158f, 0.80f }, { 0.018f, 0.008f, 0.018f }, juce::Colour (0xff00d8df));

        return m;
    }

    Mesh buildCar()
    {
        Mesh m;
        const juce::Colour body   { 0xff7d4742 };
        const juce::Colour bodyLo { 0xff58322f };
        const juce::Colour tyre   { 0xff15191c };

        addBox (m, { 0.0f, 0.0f, 0.55f }, { 1.70f, 3.40f, 0.55f }, body);
        addBox (m, { 0.0f, 0.0f, 0.33f }, { 1.62f, 3.30f, 0.22f }, bodyLo);
        addBox (m, { 0.0f, -0.10f, 1.00f }, { 1.45f, 1.72f, 0.52f }, body.darker (0.05f));

        /* glazing */
        addBox (m, { 0.0f, 0.77f, 1.02f }, { 1.30f, 0.02f, 0.36f }, glassBlue);
        addBox (m, { 0.0f, -0.97f, 1.02f }, { 1.30f, 0.02f, 0.34f }, glassBlue);
        addBox (m, { -0.727f, -0.10f, 1.03f }, { 0.02f, 1.16f, 0.30f }, glassBlue);
        addBox (m, {  0.727f, -0.10f, 1.03f }, { 0.02f, 1.16f, 0.30f }, glassBlue);

        /* wheels */
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
            {
                addCylinder (m, { 0.86f * sx, 1.10f * sy, 0.34f }, 0.34f, 0.22f, 0, 14, tyre);
                addCylinder (m, { 0.88f * sx, 1.10f * sy, 0.34f }, 0.13f, 0.24f, 0, 10, metalLight);
            }

        /* lights */
        addBox (m, { -0.55f, 1.705f, 0.62f }, { 0.24f, 0.03f, 0.10f }, juce::Colour (0xfff2d6a0));
        addBox (m, {  0.55f, 1.705f, 0.62f }, { 0.24f, 0.03f, 0.10f }, juce::Colour (0xfff2d6a0));
        addBox (m, { -0.55f, -1.705f, 0.62f }, { 0.22f, 0.03f, 0.09f }, juce::Colour (0xffc94c46));
        addBox (m, {  0.55f, -1.705f, 0.62f }, { 0.22f, 0.03f, 0.09f }, juce::Colour (0xffc94c46));

        return m;
    }

    Mesh buildBird()
    {
        Mesh m;
        const juce::Colour feather { 0xff8fa6b4 };
        const juce::Colour wing    { 0xff738b9a };

        addSphere (m, { 0.0f, 0.0f, 0.0f }, { 0.13f, 0.26f, 0.12f }, 10, 6, feather);
        addSphere (m, { 0.0f, 0.22f, 0.10f }, 0.085f, 10, 6, feather.brighter (0.06f));
        addBox (m, { 0.0f, 0.315f, 0.09f }, { 0.038f, 0.08f, 0.03f }, juce::Colour (0xffd09a4e));

        /* flat wings, one triangle fan per side */
        const int lr = m.addVertex ({ -0.07f, 0.05f, 0.02f });
        const int lf = m.addVertex ({ -0.18f, 0.15f, -0.01f });
        const int lb = m.addVertex ({ -0.18f, -0.15f, -0.01f });
        const int lt = m.addVertex ({ -0.56f, -0.06f, 0.12f });
        m.addTriangle (lr, lf, lt, wing);
        m.addTriangle (lr, lt, lb, wing.darker (0.08f));

        const int rr = m.addVertex ({ 0.07f, 0.05f, 0.02f });
        const int rf = m.addVertex ({ 0.18f, 0.15f, -0.01f });
        const int rb = m.addVertex ({ 0.18f, -0.15f, -0.01f });
        const int rt = m.addVertex ({ 0.56f, -0.06f, 0.12f });
        m.addTriangle (rr, rt, rf, wing);
        m.addTriangle (rr, rb, rt, wing.darker (0.08f));

        /* tail */
        const int tr = m.addVertex ({ 0.0f, -0.20f, 0.02f });
        const int tl = m.addVertex ({ -0.10f, -0.40f, 0.07f });
        const int tb = m.addVertex ({ 0.10f, -0.40f, 0.07f });
        m.addTriangle (tr, tl, tb, wing.darker (0.15f));

        return m;
    }

    Mesh buildDrone()
    {
        Mesh m;
        const juce::Colour shell { 0xff343e45 };
        const juce::Colour trim  { 0xff00b6c2 };

        addBox (m, { 0.0f, 0.0f, 0.0f }, { 0.26f, 0.20f, 0.10f }, shell);
        addBox (m, { 0.0f, 0.0f, 0.065f }, { 0.20f, 0.15f, 0.04f }, shell.brighter (0.12f));
        addSphere (m, { 0.0f, 0.06f, -0.08f }, 0.052f, 8, 5, metalLight);

        Mesh arm;
        addBox (arm, { 0.0f, 0.0f, 0.0f }, { 0.055f, 0.42f, 0.035f }, shell);
        for (int i = 0; i < 4; ++i)
            appendMesh (m, arm, { 0.0f, 0.0f, 0.0f },
                        juce::MathConstants<float>::halfPi * 0.5f + i * juce::MathConstants<float>::halfPi);

        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
            {
                addCylinder (m, { 0.21f * sx, 0.21f * sy, 0.055f }, 0.032f, 0.06f, 2, 8, metalDark);
                addCylinder (m, { 0.21f * sx, 0.21f * sy, 0.095f }, 0.145f, 0.008f, 2, 12, metalDark.brighter (0.18f));
                addBox (m, { 0.21f * sx, 0.21f * sy, 0.045f }, { 0.025f, 0.025f, 0.012f }, trim);
            }

        /* landing skids */
        addBox (m, { -0.09f, 0.0f, -0.115f }, { 0.018f, 0.24f, 0.02f }, metalDark);
        addBox (m, {  0.09f, 0.0f, -0.115f }, { 0.018f, 0.24f, 0.02f }, metalDark);

        return m;
    }

    Mesh buildMarker()
    {
        Mesh m;
        const juce::Colour pin { 0xffd8a34a };

        addSphere (m, { 0.0f, 0.0f, 0.0f }, 0.095f, 12, 7, pin);
        addCylinder (m, { 0.0f, 0.0f, 0.012f }, 0.135f, 0.014f, 2, 14, pin.darker (0.25f));
        /* cone whose tip is exactly on the acoustic anchor */
        addCone (m, { 0.0f, 0.0f, -0.22f }, 0.085f, 0.22f, 10, pin.darker (0.35f));

        return m;
    }
}

const ActorDefinition& getActorDefinition (ActorKind kind)
{
    static const std::array<ActorDefinition, (int) ActorKind::numKinds> definitions = []
    {
        std::array<ActorDefinition, (int) ActorKind::numKinds> d;

        d[(int) ActorKind::performer].kind = ActorKind::performer;
        d[(int) ActorKind::performer].name = "Performer";
        d[(int) ActorKind::performer].aerial = false;
        d[(int) ActorKind::performer].footprint = 0.46f;
        d[(int) ActorKind::performer].labelHeight = 1.95f;
        d[(int) ActorKind::performer].mesh = buildPerformer();

        d[(int) ActorKind::listener].kind = ActorKind::listener;
        d[(int) ActorKind::listener].name = "Listener";
        d[(int) ActorKind::listener].aerial = false;
        d[(int) ActorKind::listener].footprint = 0.44f;
        d[(int) ActorKind::listener].labelHeight = 1.98f;
        d[(int) ActorKind::listener].mesh = buildListener();

        d[(int) ActorKind::speaker].kind = ActorKind::speaker;
        d[(int) ActorKind::speaker].name = "Speaker";
        d[(int) ActorKind::speaker].aerial = false;
        d[(int) ActorKind::speaker].footprint = 0.40f;
        d[(int) ActorKind::speaker].labelHeight = 1.58f;
        d[(int) ActorKind::speaker].mesh = buildSpeaker();

        d[(int) ActorKind::car].kind = ActorKind::car;
        d[(int) ActorKind::car].name = "Car";
        d[(int) ActorKind::car].aerial = false;
        d[(int) ActorKind::car].footprint = 1.25f;
        d[(int) ActorKind::car].labelHeight = 1.55f;
        d[(int) ActorKind::car].mesh = buildCar();

        d[(int) ActorKind::bird].kind = ActorKind::bird;
        d[(int) ActorKind::bird].name = "Bird";
        d[(int) ActorKind::bird].aerial = true;
        d[(int) ActorKind::bird].footprint = 0.34f;
        d[(int) ActorKind::bird].labelHeight = 0.46f;
        d[(int) ActorKind::bird].mesh = buildBird();

        d[(int) ActorKind::drone].kind = ActorKind::drone;
        d[(int) ActorKind::drone].name = "Drone";
        d[(int) ActorKind::drone].aerial = true;
        d[(int) ActorKind::drone].footprint = 0.40f;
        d[(int) ActorKind::drone].labelHeight = 0.40f;
        d[(int) ActorKind::drone].mesh = buildDrone();

        d[(int) ActorKind::marker].kind = ActorKind::marker;
        d[(int) ActorKind::marker].name = "Marker";
        d[(int) ActorKind::marker].aerial = true;
        d[(int) ActorKind::marker].footprint = 0.20f;
        d[(int) ActorKind::marker].labelHeight = 0.32f;
        d[(int) ActorKind::marker].mesh = buildMarker();

        return d;
    }();

    const int index = juce::jlimit (0, (int) ActorKind::numKinds - 1, (int) kind);
    return definitions[(size_t) index];
}

juce::Array<ActorKind> allActorKinds()
{
    return { ActorKind::performer, ActorKind::listener, ActorKind::speaker,
             ActorKind::car, ActorKind::bird, ActorKind::drone,
             ActorKind::marker };
}

juce::String toString (ActorKind kind)
{
    return getActorDefinition (kind).name;
}

ActorKind actorKindFromString (const juce::String& text, ActorKind fallback)
{
    for (auto kind : allActorKinds())
        if (text.equalsIgnoreCase (toString (kind)))
            return kind;
    return fallback;
}

} // namespace studio
