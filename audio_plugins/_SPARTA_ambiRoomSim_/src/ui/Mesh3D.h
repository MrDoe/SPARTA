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
#include <vector>
#include <cmath>

/* A deliberately small mesh toolkit for the built-in studio actors.

   Everything is triangles, generated procedurally at startup: no asset files,
   no importers, no new dependencies. The renderer only needs positions,
   colours and a depth-sorted triangle list, so this is enough for low-poly
   mannequins, vehicles and studio props. */
namespace studio
{

struct Vec3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3 (float X, float Y, float Z) : x (X), y (Y), z (Z) {}

    Vec3 operator+ (const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const       { return { x * s, y * s, z * s }; }
    Vec3& operator+= (const Vec3& o)     { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-= (const Vec3& o)     { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline float dot (const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross (const Vec3& a, const Vec3& b)
{
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

inline float length (const Vec3& v)
{
    return std::sqrt (dot (v, v));
}

inline Vec3 normalized (const Vec3& v)
{
    const float l = length (v);
    return l > 1.0e-6f ? v * (1.0f / l) : Vec3 (0.0f, 0.0f, 1.0f);
}

struct Face
{
    int a = 0, b = 0, c = 0;
    juce::Colour colour { 0xff9aa8b2 };
};

struct Mesh
{
    std::vector<Vec3> vertices;
    std::vector<Face> faces;

    int addVertex (const Vec3& v)
    {
        vertices.push_back (v);
        return (int) vertices.size() - 1;
    }

    void addTriangle (int a, int b, int c, juce::Colour colour)
    {
        faces.push_back ({ a, b, c, colour });
    }

    void addQuad (int a, int b, int c, int d, juce::Colour colour)
    {
        addTriangle (a, b, c, colour);
        addTriangle (a, c, d, colour);
    }
};

/* Axis-aligned box. `centre` is the geometric centre. */
inline void addBox (Mesh& m, const Vec3& centre, const Vec3& size, juce::Colour colour)
{
    const float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
    const int v0 = m.addVertex ({ centre.x - hx, centre.y - hy, centre.z - hz });
    const int v1 = m.addVertex ({ centre.x + hx, centre.y - hy, centre.z - hz });
    const int v2 = m.addVertex ({ centre.x + hx, centre.y + hy, centre.z - hz });
    const int v3 = m.addVertex ({ centre.x - hx, centre.y + hy, centre.z - hz });
    const int v4 = m.addVertex ({ centre.x - hx, centre.y - hy, centre.z + hz });
    const int v5 = m.addVertex ({ centre.x + hx, centre.y - hy, centre.z + hz });
    const int v6 = m.addVertex ({ centre.x + hx, centre.y + hy, centre.z + hz });
    const int v7 = m.addVertex ({ centre.x - hx, centre.y + hy, centre.z + hz });

    m.addQuad (v0, v1, v2, v3, colour.darker (0.28f)); /* bottom */
    m.addQuad (v4, v7, v6, v5, colour.brighter (0.08f)); /* top   */
    m.addQuad (v0, v4, v5, v1, colour);                  /* front */
    m.addQuad (v1, v5, v6, v2, colour.darker (0.10f));   /* right */
    m.addQuad (v2, v6, v7, v3, colour.darker (0.16f));   /* back  */
    m.addQuad (v3, v7, v4, v0, colour.darker (0.06f));   /* left  */
}

/* Cylinder with a configurable axis (0=x, 1=y, 2=z). */
inline void addCylinder (Mesh& m, const Vec3& centre, float radius, float length,
                         int axis, int segments, juce::Colour colour)
{
    segments = juce::jmax (4, segments);
    const float half = length * 0.5f;

    Vec3 u (1.0f, 0.0f, 0.0f), v (0.0f, 1.0f, 0.0f), w (0.0f, 0.0f, 1.0f);
    if (axis == 0)      { u = { 0.0f, 1.0f, 0.0f }; v = { 0.0f, 0.0f, 1.0f }; w = { 1.0f, 0.0f, 0.0f }; }
    else if (axis == 1) { u = { 1.0f, 0.0f, 0.0f }; v = { 0.0f, 0.0f, 1.0f }; w = { 0.0f, 1.0f, 0.0f }; }

    std::vector<int> lo ((size_t) segments), hi ((size_t) segments);
    for (int i = 0; i < segments; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) segments;
        const Vec3 offset = u * (std::cos (a) * radius) + v * (std::sin (a) * radius);
        lo[(size_t) i] = m.addVertex (centre + offset - w * half);
        hi[(size_t) i] = m.addVertex (centre + offset + w * half);
    }

    for (int i = 0; i < segments; ++i)
    {
        const int n = (i + 1) % segments;
        m.addQuad (lo[(size_t) i], lo[(size_t) n], hi[(size_t) n], hi[(size_t) i], colour);
    }

    const int loCentre = m.addVertex (centre - w * half);
    const int hiCentre = m.addVertex (centre + w * half);
    for (int i = 0; i < segments; ++i)
    {
        const int n = (i + 1) % segments;
        m.addTriangle (loCentre, lo[(size_t) n], lo[(size_t) i], colour.darker (0.3f));
        m.addTriangle (hiCentre, hi[(size_t) i], hi[(size_t) n], colour.brighter (0.1f));
    }
}

/* UV sphere centred at `centre`; can be non-uniformly scaled for ellipsoids. */
inline void addSphere (Mesh& m, const Vec3& centre, const Vec3& radii,
                       int segments, int rings, juce::Colour colour)
{
    segments = juce::jmax (4, segments);
    rings = juce::jmax (2, rings);

    std::vector<std::vector<int>> grid ((size_t) (rings + 1));
    for (int r = 0; r <= rings; ++r)
    {
        const float phi = juce::MathConstants<float>::pi * ((float) r / (float) rings) - juce::MathConstants<float>::halfPi;
        const float cp = std::cos (phi), sp = std::sin (phi);
        grid[(size_t) r].resize ((size_t) segments);
        for (int s = 0; s < segments; ++s)
        {
            const float theta = juce::MathConstants<float>::twoPi * (float) s / (float) segments;
            const Vec3 p = centre + Vec3 (radii.x * cp * std::cos (theta),
                                          radii.y * cp * std::sin (theta),
                                          radii.z * sp);
            grid[(size_t) r][(size_t) s] = m.addVertex (p);
        }
    }

    for (int r = 0; r < rings; ++r)
    {
        for (int s = 0; s < segments; ++s)
        {
            const int n = (s + 1) % segments;
            const juce::Colour c = colour.withMultipliedBrightness (0.92f + 0.12f * (float) r / (float) rings);
            m.addTriangle (grid[(size_t) r][(size_t) s],
                           grid[(size_t) r][(size_t) n],
                           grid[(size_t) (r + 1)][(size_t) n], c);
            m.addTriangle (grid[(size_t) r][(size_t) s],
                           grid[(size_t) (r + 1)][(size_t) n],
                           grid[(size_t) (r + 1)][(size_t) s], c.darker (0.06f));
        }
    }
}

inline void addSphere (Mesh& m, const Vec3& centre, float radius,
                       int segments, int rings, juce::Colour colour)
{
    addSphere (m, centre, Vec3 (radius, radius, radius), segments, rings, colour);
}

/* Cone with its base at `base`, pointing along +z (or another axis). */
inline void addCone (Mesh& m, const Vec3& base, float radius, float height,
                     int segments, juce::Colour colour)
{
    segments = juce::jmax (4, segments);
    const int tip = m.addVertex ({ base.x, base.y, base.z + height });
    const int centre = m.addVertex (base);

    std::vector<int> ring ((size_t) segments);
    for (int i = 0; i < segments; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) segments;
        ring[(size_t) i] = m.addVertex ({ base.x + std::cos (a) * radius,
                                          base.y + std::sin (a) * radius,
                                          base.z });
    }

    for (int i = 0; i < segments; ++i)
    {
        const int n = (i + 1) % segments;
        m.addTriangle (ring[(size_t) i], ring[(size_t) n], tip, colour);
        m.addTriangle (centre, ring[(size_t) n], ring[(size_t) i], colour.darker (0.3f));
    }
}

/* Appends a mesh with a yaw rotation around z, a uniform scale and an offset.
   Used to assemble limbs, wheels and rotors from the primitives above. */
inline void appendMesh (Mesh& destination, const Mesh& source,
                        const Vec3& offset, float yawRadians = 0.0f,
                        const Vec3& scale = Vec3 (1.0f, 1.0f, 1.0f))
{
    const int base = (int) destination.vertices.size();
    const float c = std::cos (yawRadians), s = std::sin (yawRadians);

    for (const auto& v : source.vertices)
    {
        const Vec3 scaled (v.x * scale.x, v.y * scale.y, v.z * scale.z);
        destination.vertices.push_back ({ offset.x + scaled.x * c - scaled.y * s,
                                          offset.y + scaled.x * s + scaled.y * c,
                                          offset.z + scaled.z });
    }

    for (const auto& f : source.faces)
        destination.faces.push_back ({ base + f.a, base + f.b, base + f.c, f.colour });
}

} // namespace studio
