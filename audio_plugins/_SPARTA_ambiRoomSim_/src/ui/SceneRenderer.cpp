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

#include "SceneRenderer.h"
#include "StudioTheme.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace studio
{

namespace
{
    constexpr float nearPlane = 0.05f;
    const Vec3 lightDirection = normalized (Vec3 (-0.35f, -0.55f, 0.75f));

    struct ActorVisual
    {
        Selection selection;
        Vec3 position;          /* acoustic point */
        Vec3 base;              /* mesh origin */
        const ActorDefinition* definition = nullptr;
        float yawRadians = 0.0f;
        float scale = 1.0f;
        juce::String label;
        juce::Colour colour;
        bool selected = false;
    };

    struct PolyDraw
    {
        juce::Point<float> points[4];
        int numPoints = 0;
        juce::Colour colour;
        float depth = 0.0f;
    };

    struct KeyMarker
    {
        juce::Point<float> screen;
        float depth = 0.0f;
        juce::Colour colour;
        juce::String time;
        double timeSeconds = 0.0;
        bool selected = false;
        /* Lower values are placed first, so the selected checkpoint (and the
           active path) never loses its label to a crowded unselected path. */
        int priority = 2;
    };

    struct ChipRect
    {
        juce::Rectangle<float> rect;
        bool used = false;
    };

    struct RoomDimensions
    {
        float width = 8.0f, depth = 6.0f, height = 4.0f;
    };

    RoomDimensions getRoomDimensions (PluginProcessor& processor)
    {
        RoomDimensions room;
        room.width  = juce::jmax (0.5f, processor.getParameterFloat ("roomX"));
        room.depth  = juce::jmax (0.5f, processor.getParameterFloat ("roomY"));
        room.height = juce::jmax (0.5f, processor.getParameterFloat ("roomZ"));
        return room;
    }

    int getSourceCount (PluginProcessor& processor)
    {
        return juce::jlimit (1, 16, processor.getParameterInt ("numSources"));
    }

    int getReceiverCount (PluginProcessor& processor)
    {
        return juce::jlimit (1, 16, processor.getParameterInt ("numReceivers"));
    }

    bool clipPolygonNear (std::vector<Vec3>& polygon)
    {
        if (polygon.empty())
            return false;

        std::vector<Vec3> out;
        out.reserve (polygon.size() + 1);

        for (size_t i = 0; i < polygon.size(); ++i)
        {
            const Vec3 current = polygon[i];
            const Vec3 next = polygon[(i + 1) % polygon.size()];
            const bool currentInside = current.z >= nearPlane;
            const bool nextInside = next.z >= nearPlane;

            if (currentInside)
                out.push_back (current);

            if (currentInside != nextInside)
            {
                const float denominator = next.z - current.z;
                if (std::abs (denominator) > 1.0e-6f)
                {
                    const float t = (nearPlane - current.z) / denominator;
                    out.push_back (current + (next - current) * t);
                }
            }
        }

        polygon.swap (out);
        return polygon.size() >= 3;
    }

    bool projectWorldPolygon (const Camera& camera,
                              const std::vector<Vec3>& worldPoints,
                              juce::Path& path,
                              float& averageDepth)
    {
        if (worldPoints.size() < 3)
            return false;

        std::vector<Vec3> viewPoints;
        viewPoints.reserve (worldPoints.size());
        for (const auto& world : worldPoints)
            viewPoints.push_back (camera.toView (world));

        if (! camera.orthographic && ! clipPolygonNear (viewPoints))
            return false;

        if (viewPoints.size() < 3)
            return false;

        averageDepth = 0.0f;
        path.clear();
        for (size_t i = 0; i < viewPoints.size(); ++i)
        {
            float depth = 0.0f;
            const auto p = camera.viewToScreen (viewPoints[i], depth);
            averageDepth += depth;
            if (i == 0)
                path.startNewSubPath (p.x, p.y);
            else
                path.lineTo (p.x, p.y);
        }
        path.closeSubPath();
        averageDepth /= (float) viewPoints.size();
        return true;
    }

    /* Sutherland-Hodgman for a single triangle against the near plane, then
       projected to screen. A clipped triangle has at most four vertices. */
    bool projectTriangle (const Camera& camera,
                          const Vec3& a, const Vec3& b, const Vec3& c,
                          PolyDraw& out)
    {
        Vec3 view[3] = { camera.toView (a), camera.toView (b), camera.toView (c) };

        if (! camera.orthographic)
        {
            Vec3 clipped[4];
            int count = 0;
            for (int i = 0; i < 3; ++i)
            {
                const Vec3& current = view[i];
                const Vec3& next = view[(i + 1) % 3];
                const bool currentInside = current.z >= nearPlane;
                const bool nextInside = next.z >= nearPlane;

                if (currentInside)
                    clipped[count++] = current;

                if (currentInside != nextInside)
                {
                    const float denominator = next.z - current.z;
                    if (std::abs (denominator) > 1.0e-6f)
                    {
                        const float t = (nearPlane - current.z) / denominator;
                        clipped[count++] = current + (next - current) * t;
                    }
                }
            }

            if (count < 3)
                return false;

            out.numPoints = juce::jmin (count, 4);
            out.depth = 0.0f;
            for (int i = 0; i < out.numPoints; ++i)
            {
                float depth = 0.0f;
                out.points[i] = camera.viewToScreen (clipped[i], depth);
                out.depth += depth;
            }
            out.depth /= (float) out.numPoints;
            return true;
        }

        out.numPoints = 3;
        out.depth = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            float depth = 0.0f;
            out.points[i] = camera.viewToScreen (view[i], depth);
            out.depth += depth;
        }
        out.depth /= 3.0f;
        return true;
    }

    void drawFloorDisc (juce::Graphics& g, const Camera& camera,
                        const Vec3& centre, float radius, juce::Colour colour,
                        float thickness = 0.0f)
    {
        std::vector<Vec3> points;
        points.reserve (28);
        for (int i = 0; i < 28; ++i)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) i / 28.0f;
            points.push_back ({ centre.x + std::cos (angle) * radius,
                                centre.y + std::sin (angle) * radius,
                                centre.z });
        }

        juce::Path path;
        float depth = 0.0f;
        if (! projectWorldPolygon (camera, points, path, depth))
            return;

        if (thickness <= 0.0f)
        {
            g.setColour (colour);
            g.fillPath (path);
        }
        else
        {
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (thickness));
        }
    }

    void drawDashedLine (juce::Graphics& g, juce::Point<float> a,
                         juce::Point<float> b, juce::Colour colour)
    {
        const auto delta = b - a;
        const float length = delta.getDistanceFromOrigin();
        if (length < 1.0f)
            return;

        const auto direction = delta / length;
        const float dash = 5.0f, gap = 5.0f;
        g.setColour (colour);
        for (float t = 0.0f; t < length; t += dash + gap)
        {
            const auto start = a + direction * t;
            const auto end = a + direction * juce::jmin (length, t + dash);
            g.drawLine ({ start, end }, 1.0f);
        }
    }

    void drawWall (juce::Graphics& g, const Camera& camera,
                   const Vec3 corners[4], juce::Colour fill, juce::Colour edge)
    {
        std::vector<Vec3> points (corners, corners + 4);
        juce::Path path;
        float depth = 0.0f;
        if (! projectWorldPolygon (camera, points, path, depth))
            return;

        g.setColour (fill);
        g.fillPath (path);
        g.setColour (edge);
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

    void drawRoom (juce::Graphics& g, const Camera& camera,
                   PluginProcessor& processor, const RenderOptions& options)
    {
        const auto room = getRoomDimensions (processor);
        const float W = room.width, D = room.depth, H = room.height;

        /* Draw only the walls that face away from the camera, so the room is
           always readable as a cutaway box instead of a closed cube. */
        struct Wall
        {
            Vec3 corners[4];
            Vec3 centre;
            Vec3 outward;
            juce::Colour fill;
            juce::Colour edge;
        };

        const juce::Colour xFill { 0xff0b181e };
        const juce::Colour yFill { 0xff0d1c23 };
        const juce::Colour zFill { 0xff0a171c };
        const juce::Colour edge  { 0x2effffff };

        const Wall walls[6] = {
            { { { 0.0f, 0.0f, 0.0f }, { 0.0f, D, 0.0f }, { 0.0f, D, H }, { 0.0f, 0.0f, H } },
              { 0.0f, D * 0.5f, H * 0.5f }, { -1.0f, 0.0f, 0.0f }, xFill, edge },
            { { { W, 0.0f, 0.0f }, { W, D, 0.0f }, { W, D, H }, { W, 0.0f, H } },
              { W, D * 0.5f, H * 0.5f }, { 1.0f, 0.0f, 0.0f }, xFill, edge },
            { { { 0.0f, 0.0f, 0.0f }, { W, 0.0f, 0.0f }, { W, 0.0f, H }, { 0.0f, 0.0f, H } },
              { W * 0.5f, 0.0f, H * 0.5f }, { 0.0f, -1.0f, 0.0f }, yFill, edge },
            { { { 0.0f, D, 0.0f }, { W, D, 0.0f }, { W, D, H }, { 0.0f, D, H } },
              { W * 0.5f, D, H * 0.5f }, { 0.0f, 1.0f, 0.0f }, yFill, edge },
            { { { 0.0f, 0.0f, 0.0f }, { W, 0.0f, 0.0f }, { W, D, 0.0f }, { 0.0f, D, 0.0f } },
              { W * 0.5f, D * 0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, zFill, edge },
            { { { 0.0f, 0.0f, H }, { W, 0.0f, H }, { W, D, H }, { 0.0f, D, H } },
              { W * 0.5f, D * 0.5f, H }, { 0.0f, 0.0f, 1.0f }, zFill, edge }
        };

        const bool lookingDown = camera.pitch > 1.30f;
        for (const auto& wall : walls)
        {
            const bool floor = wall.outward.z < -0.5f;
            if (! floor)
            {
                if (lookingDown)
                    continue;
                if (dot (wall.outward, camera.eye - wall.centre) >= 0.0f)
                    continue;
            }
            drawWall (g, camera, wall.corners, wall.fill, wall.edge);
        }

        /* Floor */
        {
            std::vector<Vec3> floorPoints = {
                { 0.0f, 0.0f, 0.0f }, { W, 0.0f, 0.0f },
                { W, D, 0.0f }, { 0.0f, D, 0.0f }
            };
            juce::Path floorPath;
            float depth = 0.0f;
            if (projectWorldPolygon (camera, floorPoints, floorPath, depth))
            {
                g.setColour (Colours::floorFill);
                g.fillPath (floorPath);
                g.setColour (Colours::wallEdge);
                g.strokePath (floorPath, juce::PathStrokeType (1.0f));
            }
        }

        /* Floor grid */
        if (options.showGrid)
        {
            auto drawGridLine = [&] (const Vec3& a, const Vec3& b, juce::Colour colour)
            {
                juce::Point<float> pa, pb;
                if (camera.projectSegment (a, b, pa, pb))
                {
                    g.setColour (colour);
                    g.drawLine ({ pa, pb }, 1.0f);
                }
            };

            const int nx = (int) std::floor (W + 0.5f);
            const int ny = (int) std::floor (D + 0.5f);

            for (int i = 1; i < nx; ++i)
                drawGridLine ({ (float) i, 0.0f, 0.0f }, { (float) i, D, 0.0f },
                              i % 5 == 0 ? Colours::gridMajor : Colours::gridMinor);
            for (int i = 1; i < ny; ++i)
                drawGridLine ({ 0.0f, (float) i, 0.0f }, { W, (float) i, 0.0f },
                              i % 5 == 0 ? Colours::gridMajor : Colours::gridMinor);

            /* Half-metre ticks on the two front edges, useful while dragging. */
            for (float x = 0.5f; x < W; x += 1.0f)
                drawGridLine ({ x, 0.0f, 0.0f }, { x, 0.16f, 0.0f }, Colours::gridMajor);
            for (float y = 0.5f; y < D; y += 1.0f)
                drawGridLine ({ 0.0f, y, 0.0f }, { 0.16f, y, 0.0f }, Colours::gridMajor);
        }
    }

    std::vector<ActorVisual> collectActors (PluginProcessor& processor,
                                            const RenderOptions& options,
                                            const Selection& selection)
    {
        std::vector<ActorVisual> actors;
        const int nSrc = getSourceCount (processor);
        const int nRec = getReceiverCount (processor);
        actors.reserve ((size_t) (nSrc + nRec));

        auto& scene = processor.getSceneModel();

        for (int side = 0; side < 2; ++side)
        {
            const bool isReceiver = side == 1;
            const int count = isReceiver ? nRec : nSrc;
            for (int i = 0; i < count; ++i)
            {
                Vec3 position;
                if (! SceneRenderer::getActorPosition (processor, isReceiver, i, options, position))
                    continue;

                ActorInfo info;
                {
                    const juce::ScopedLock sl (processor.getSceneLock());
                    info = scene.getActor (isReceiver, i);
                }
                const auto& definition = getActorDefinition (info.kind);

                ActorVisual actor;
                actor.selection = isReceiver ? Selection::receiver (i) : Selection::source (i);
                actor.position = position;
                actor.base = definition.aerial ? position : Vec3 (position.x, position.y, 0.0f);
                actor.definition = &definition;
                actor.yawRadians = juce::degreesToRadians (info.yawDegrees);
                actor.scale = juce::jlimit (0.25f, 3.0f, info.scale);
                actor.colour = getPathColour (isReceiver, i);
                actor.selected = actor.selection == selection;

                actor.label = juce::String (isReceiver ? "R" : "S")
                            + juce::String (i + 1) + "  ";
                if (info.name.isNotEmpty())
                    actor.label += info.name;
                else
                    actor.label += definition.name;

                actors.push_back (actor);
            }
        }

        return actors;
    }

    void drawShadowsAndDiscs (juce::Graphics& g, const Camera& camera,
                              const std::vector<ActorVisual>& actors)
    {
        for (const auto& actor : actors)
        {
            if (actor.definition == nullptr)
                continue;

            const float radius = actor.definition->footprint * actor.scale * 1.15f;

            if (actor.selected)
            {
                drawFloorDisc (g, camera, { actor.position.x, actor.position.y, 0.004f },
                               radius * 1.55f, Colours::accent.withAlpha (0.055f));
                drawFloorDisc (g, camera, { actor.position.x, actor.position.y, 0.006f },
                               radius * 1.55f, Colours::accent.withAlpha (0.85f), 1.5f);
            }

            const float heightFactor = 1.0f / (1.0f + juce::jmax (0.0f, actor.position.z - 0.2f) * 0.35f);
            drawFloorDisc (g, camera, { actor.position.x, actor.position.y, 0.003f },
                           radius, juce::Colour (0xff000000).withAlpha (0.40f * heightFactor));
            drawFloorDisc (g, camera, { actor.position.x, actor.position.y, 0.004f },
                           radius * 0.62f, juce::Colour (0xff000000).withAlpha (0.22f * heightFactor));

            /* A dashed drop line makes the acoustic height legible. */
            juce::Point<float> pa, pb;
            if (camera.projectSegment (actor.position,
                                       { actor.position.x, actor.position.y, 0.0f }, pa, pb))
                drawDashedLine (g, pa, pb, actor.colour.withAlpha (0.30f));
        }
    }

    void buildActorTriangles (const Camera& camera,
                              const std::vector<ActorVisual>& actors,
                              std::vector<PolyDraw>& polygons)
    {
        for (const auto& actor : actors)
        {
            if (actor.definition == nullptr)
                continue;

            const Mesh& mesh = actor.definition->mesh;
            if (mesh.vertices.empty())
                continue;

            const float c = std::cos (actor.yawRadians);
            const float s = std::sin (actor.yawRadians);
            const float scale = actor.scale;

            std::vector<Vec3> transformed;
            transformed.reserve (mesh.vertices.size());
            for (const auto& vertex : mesh.vertices)
            {
                transformed.push_back ({
                    actor.base.x + (vertex.x * c - vertex.y * s) * scale,
                    actor.base.y + (vertex.x * s + vertex.y * c) * scale,
                    actor.base.z + vertex.z * scale
                });
            }

            for (const auto& face : mesh.faces)
            {
                const Vec3& a = transformed[(size_t) face.a];
                const Vec3& b = transformed[(size_t) face.b];
                const Vec3& d = transformed[(size_t) face.c];

                const Vec3 normal = normalized (cross (b - a, d - a));
                const float light = std::abs (dot (normal, lightDirection));
                const float ambient = 0.58f + 0.10f * juce::jlimit (0.0f, 1.0f, (a.z + b.z + d.z) / (3.0f * 2.2f));
                const float brightness = juce::jlimit (0.35f, 1.25f, ambient + 0.34f * light);

                PolyDraw poly;
                if (! projectTriangle (camera, a, b, d, poly))
                    continue;

                poly.colour = face.colour.withMultipliedBrightness (brightness);
                polygons.push_back (poly);
            }
        }

        std::sort (polygons.begin(), polygons.end(),
                   [] (const PolyDraw& lhs, const PolyDraw& rhs)
                   {
                       return lhs.depth > rhs.depth;
                   });
    }

    void drawPolygons (juce::Graphics& g, const std::vector<PolyDraw>& polygons)
    {
        for (const auto& poly : polygons)
        {
            if (poly.numPoints < 3)
                continue;

            juce::Path path;
            path.startNewSubPath (poly.points[0].x, poly.points[0].y);
            for (int i = 1; i < poly.numPoints; ++i)
                path.lineTo (poly.points[i].x, poly.points[i].y);
            path.closeSubPath();

            g.setColour (poly.colour);
            g.fillPath (path);
            /* Seals hairline seams between adjacent triangles. */
            g.strokePath (path, juce::PathStrokeType (0.55f));
        }
    }

    void drawPaths (juce::Graphics& g,
                    const Camera& camera,
                    PluginProcessor& processor,
                    const RenderOptions& options,
                    const Selection& selection,
                    std::vector<KeyMarker>& keyMarkers)
    {
        if (! options.showPaths)
            return;

        const bool isPpq = processor.isTimeInPpq();

        for (int side = 0; side < 2; ++side)
        {
            const bool isReceiver = side == 1;
            const int count = isReceiver ? getReceiverCount (processor) : getSourceCount (processor);

            for (int i = 0; i < count; ++i)
            {
                const PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (i)
                                                  : processor.getPathBank().getSourcePath (i);
                if (path.keyframes.empty())
                    continue;

                /* A selected checkpoint also belongs to this path, so the
                   path stays at full strength while one of its nodes is
                   being edited. */
                const bool pathSelected =
                    (selection.isActor()
                     && selection.index == i
                     && selection.isReceiver() == isReceiver)
                    || (selection.isCheckpoint()
                        && selection.index == i
                        && selection.checkpointIsReceiver == isReceiver);
                float alpha = pathSelected ? 1.0f : 0.58f;
                if (! path.enabled)
                    alpha *= 0.45f;

                const auto baseColour = getPathColour (isReceiver, i);
                const juce::Colour colour = baseColour.withAlpha (alpha);

                const double start = path.startTime;
                const double end = path.endTime > start + 0.001 ? path.endTime : start + 1.0;
                const int steps = juce::jlimit (32, 128, 24 + (int) path.keyframes.size() * 10);

                juce::Path screenPath;
                bool started = false;

                for (int step = 0; step <= steps; ++step)
                {
                    const double t = start + (end - start) * (double) step / (double) steps;
                    float x = 0.0f, y = 0.0f, z = 0.0f;
                    path.evaluate (t, x, y, z);

                    juce::Point<float> point;
                    float depth = 0.0f;
                    if (! camera.project ({ x, y, z }, point, depth))
                    {
                        started = false;
                        continue;
                    }

                    if (! started)
                    {
                        screenPath.startNewSubPath (point.x, point.y);
                        started = true;
                    }
                    else
                    {
                        screenPath.lineTo (point.x, point.y);
                    }
                }

                g.setColour (colour.withMultipliedAlpha (0.35f));
                g.strokePath (screenPath, juce::PathStrokeType (4.0f,
                                                                juce::PathStrokeType::curved,
                                                                juce::PathStrokeType::rounded));
                g.setColour (colour);
                g.strokePath (screenPath, juce::PathStrokeType (1.6f,
                                                                juce::PathStrokeType::curved,
                                                                juce::PathStrokeType::rounded));

                /* Direction arrows along the spline. */
                for (float fraction : { 0.25f, 0.5f, 0.75f })
                {
                    const double t = start + (end - start) * (double) fraction;
                    const double dt = (end - start) * 0.01;
                    float x0 = 0.0f, y0 = 0.0f, z0 = 0.0f, x1 = 0.0f, y1 = 0.0f, z1 = 0.0f;
                    path.evaluate (t - dt, x0, y0, z0);
                    path.evaluate (t + dt, x1, y1, z1);

                    juce::Point<float> p0, p1;
                    float d0 = 0.0f, d1 = 0.0f;
                    if (! camera.project ({ x0, y0, z0 }, p0, d0)
                        || ! camera.project ({ x1, y1, z1 }, p1, d1))
                        continue;

                    const auto delta = p1 - p0;
                    const float deltaLength = delta.getDistanceFromOrigin();
                    if (deltaLength < 0.5f)
                        continue;
                    const auto direction = delta / deltaLength;

                    const auto tip = (p0 + p1) * 0.5f + direction * 5.0f;
                    const auto normal = juce::Point<float> (-direction.y, direction.x) * 3.0f;
                    juce::Path arrow;
                    arrow.addTriangle (tip.x, tip.y,
                                       tip.x - direction.x * 7.0f + normal.x,
                                       tip.y - direction.y * 7.0f + normal.y,
                                       tip.x - direction.x * 7.0f - normal.x,
                                       tip.y - direction.y * 7.0f - normal.y);
                    g.setColour (colour.withMultipliedAlpha (0.9f));
                    g.fillPath (arrow);
                }

                /* Keyframe diamonds and their times are collected and drawn
                   after the actors so they are never buried in a mesh. */
                for (size_t k = 0; k < path.keyframes.size(); ++k)
                {
                    const auto& keyframe = path.keyframes[k];

                    juce::Point<float> point;
                    float depth = 0.0f;
                    if (! camera.project ({ keyframe.x, keyframe.y, keyframe.z }, point, depth))
                        continue;

                    const bool keyframeSelected =
                        pathSelected
                        && selection.isCheckpoint()
                        && selection.keyframe == (int) k;

                    KeyMarker marker;
                    marker.screen = point;
                    marker.depth = depth;
                    marker.colour = baseColour;
                    marker.timeSeconds = path.startTime + keyframe.timeSeconds;
                    marker.time = formatTimeShort (marker.timeSeconds, isPpq);
                    marker.selected = keyframeSelected;
                    marker.priority = keyframeSelected ? 0 : (pathSelected ? 1 : 2);
                    keyMarkers.push_back (marker);
                }
            }
        }
    }

    void drawKeyMarkers (juce::Graphics& g,
                         const std::vector<KeyMarker>& keyMarkers,
                         const RenderOptions& options,
                         juce::Rectangle<float> bounds,
                         std::vector<ChipRect>& chips)
    {
        if (keyMarkers.empty())
            return;

        for (const auto& marker : keyMarkers)
        {
            if (marker.selected)
            {
                g.setColour (Colours::accent.withAlpha (0.20f));
                g.fillEllipse (marker.screen.x - 8.0f, marker.screen.y - 8.0f,
                               16.0f, 16.0f);
            }

            juce::Path diamond;
            diamond.startNewSubPath (marker.screen.x, marker.screen.y - 4.5f);
            diamond.lineTo (marker.screen.x + 4.5f, marker.screen.y);
            diamond.lineTo (marker.screen.x, marker.screen.y + 4.5f);
            diamond.lineTo (marker.screen.x - 4.5f, marker.screen.y);
            diamond.closeSubPath();

            g.setColour (marker.colour);
            g.fillPath (diamond);
            g.setColour (Colours::bg.withAlpha (0.85f));
            g.strokePath (diamond, juce::PathStrokeType (1.0f));

            if (marker.selected)
            {
                g.setColour (Colours::accent);
                g.drawEllipse (marker.screen.x - 7.0f, marker.screen.y - 7.0f,
                               14.0f, 14.0f, 1.2f);
            }
        }

        if (! options.showTimes)
            return;

        const auto font = uiFont (9.5f);
        g.setFont (font);

        /* Place the selected checkpoint first, then the active path, then the
           remaining paths. */
        std::vector<const KeyMarker*> ordered;
        ordered.reserve (keyMarkers.size());
        for (const auto& marker : keyMarkers)
            ordered.push_back (&marker);

        std::stable_sort (ordered.begin(), ordered.end(),
                          [] (const KeyMarker* a, const KeyMarker* b)
                          {
                              return a->priority < b->priority;
                          });

        int markerCounter = 0;

        for (const auto* markerPtr : ordered)
        {
            const auto& marker = *markerPtr;
            const float width = juce::jmax (34.0f, font.getStringWidthFloat (marker.time) + 10.0f);
            constexpr float height = 15.0f;
            constexpr float gap = 10.0f;
            const float verticalStep = height + 3.0f;

            if (width > bounds.getWidth() - 8.0f)
                continue;

            /* Fan the chips out around their keyframe instead of stacking
               them all in one vertical column: neighbouring checkpoints start
               in different directions, so dense paths stay readable. */
            const int startSlot = markerCounter++ % 4;

            auto candidate = [&] (int slot, int ring) -> juce::Rectangle<float>
            {
                const float step = (float) ring;
                switch (slot)
                {
                    case 0: /* above */
                        return { marker.screen.x - width * 0.5f,
                                 marker.screen.y - gap - height
                                     - step * verticalStep,
                                 width, height };

                    case 1: /* right */
                        return { marker.screen.x + gap
                                     + step * (width * 0.5f + 8.0f),
                                 marker.screen.y - height * 0.5f,
                                 width, height };

                    case 2: /* left */
                        return { marker.screen.x - gap - width
                                     - step * (width * 0.5f + 8.0f),
                                 marker.screen.y - height * 0.5f,
                                 width, height };

                    default: /* below */
                        return { marker.screen.x - width * 0.5f,
                                 marker.screen.y + gap + step * verticalStep,
                                 width, height };
                }
            };

            const float minX = bounds.getX() + 4.0f;
            const float maxX = juce::jmax (minX, bounds.getRight() - width - 4.0f);
            const float minY = bounds.getY() + 4.0f;
            const float maxY = juce::jmax (minY, bounds.getBottom() - height - 4.0f);

            bool placed = false;
            juce::Rectangle<float> finalRect;

            for (int attempt = 0; attempt < 32 && ! placed; ++attempt)
            {
                auto rect = candidate ((startSlot + attempt) % 4, attempt / 4);
                rect.setX (juce::jlimit (minX, maxX, rect.getX()));
                rect.setY (juce::jlimit (minY, maxY, rect.getY()));

                if (rect.expanded (1.0f).contains (marker.screen))
                    continue;

                bool free = true;
                for (const auto& chip : chips)
                {
                    if (chip.used && chip.rect.intersects (rect.expanded (1.0f)))
                    {
                        free = false;
                        break;
                    }
                }

                if (! free)
                    continue;

                finalRect = rect;
                placed = true;
            }

            if (! placed)
                continue;

            ChipRect record;
            record.rect = finalRect;
            record.used = true;
            chips.push_back (record);

            const juce::Point<float> attach (
                juce::jlimit (finalRect.getX(), finalRect.getRight(), marker.screen.x),
                juce::jlimit (finalRect.getY(), finalRect.getBottom(), marker.screen.y));
            const auto delta = attach - marker.screen;
            const float length = delta.getDistanceFromOrigin();
            const auto start = length > 6.0f
                                   ? marker.screen + delta * (6.0f / length)
                                   : marker.screen;

            g.setColour (Colours::time.withAlpha (0.30f));
            g.drawLine (start.x, start.y, attach.x, attach.y, 1.0f);

            g.setColour (marker.selected ? Colours::accentDim
                                         : Colours::bg.withAlpha (0.92f));
            g.fillRoundedRectangle (finalRect, 3.0f);
            g.setColour (marker.selected ? Colours::accent
                                         : Colours::time.withAlpha (0.55f));
            g.drawRoundedRectangle (finalRect.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (marker.selected ? Colours::text : Colours::time);
            g.drawText (marker.time, finalRect, juce::Justification::centred, false);
        }
    }

    void drawAcousticPoints (juce::Graphics& g,
                             const Camera& camera,
                             const std::vector<ActorVisual>& actors)
    {
        for (const auto& actor : actors)
        {
            juce::Point<float> point;
            float depth = 0.0f;
            if (! camera.project (actor.position, point, depth))
                continue;

            const float baseRadius = 5.0f;

            g.setColour (actor.colour.withAlpha (0.16f));
            g.fillEllipse (point.x - baseRadius * 2.2f, point.y - baseRadius * 2.2f,
                           baseRadius * 4.4f, baseRadius * 4.4f);
            g.setColour (actor.colour.withAlpha (0.85f));
            g.drawEllipse (point.x - baseRadius, point.y - baseRadius,
                           baseRadius * 2.0f, baseRadius * 2.0f, 1.2f);
            g.setColour (Colours::text);
            g.fillEllipse (point.x - 1.8f, point.y - 1.8f, 3.6f, 3.6f);

            if (actor.selected)
            {
                g.setColour (Colours::accent);
                g.drawEllipse (point.x - 9.0f, point.y - 9.0f, 18.0f, 18.0f, 1.5f);
            }
        }
    }

    juce::Rectangle<float> placeLabelChip (juce::Graphics& g,
                                           juce::Point<float> anchor,
                                           juce::String text,
                                           juce::Colour accent,
                                           bool selected,
                                           juce::Rectangle<float> bounds,
                                           std::vector<ChipRect>& chips,
                                           const juce::Font& font)
    {
        const float textWidth = font.getStringWidthFloat (text);
        const float width = textWidth + 24.0f;
        constexpr float height = 18.0f;

        juce::Rectangle<float> desired (anchor.x - width * 0.5f,
                                        anchor.y - height - 11.0f,
                                        width, height);

        juce::Rectangle<float> finalRect = desired;
        bool placed = false;
        for (int attempt = 0; attempt < 10 && ! placed; ++attempt)
        {
            finalRect = desired.translated (0.0f, -attempt * (height + 4.0f));
            finalRect.setX (juce::jlimit (bounds.getX() + 6.0f,
                                          bounds.getRight() - width - 6.0f,
                                          finalRect.getX()));
            if (finalRect.getY() < bounds.getY() + 6.0f)
                break;

            placed = true;
            for (const auto& chip : chips)
            {
                if (chip.used && chip.rect.intersects (finalRect.expanded (2.0f)))
                {
                    placed = false;
                    break;
                }
            }
        }

        if (! placed)
            finalRect = desired.withX (juce::jlimit (bounds.getX() + 6.0f,
                                                     bounds.getRight() - width - 6.0f,
                                                     desired.getX()))
                                .withY (juce::jlimit (bounds.getY() + 6.0f,
                                                      bounds.getBottom() - height - 6.0f,
                                                      desired.getY()));

        ChipRect record;
        record.rect = finalRect;
        record.used = true;
        chips.push_back (record);

        g.setColour (Colours::bg.withAlpha (0.93f));
        g.fillRoundedRectangle (finalRect, 3.0f);
        g.setColour (selected ? Colours::accent : accent.withAlpha (0.55f));
        g.drawRoundedRectangle (finalRect.reduced (0.5f), 3.0f,
                                selected ? 1.5f : 1.0f);

        g.setColour (accent);
        g.fillEllipse (finalRect.getX() + 7.0f, finalRect.getCentreY() - 2.5f, 5.0f, 5.0f);

        g.setColour (Colours::text);
        g.setFont (font);
        g.drawText (text, finalRect.withTrimmedLeft (16.0f).withTrimmedRight (5.0f),
                    juce::Justification::centredLeft, true);

        g.setColour (accent.withAlpha (0.35f));
        g.drawLine (anchor.x, anchor.y - 2.0f,
                    finalRect.getCentreX(), finalRect.getBottom(), 1.0f);

        return finalRect;
    }

    void drawActorLabels (juce::Graphics& g,
                          const Camera& camera,
                          const std::vector<ActorVisual>& actors,
                          juce::Rectangle<float> bounds,
                          std::vector<ChipRect>& chips)
    {
        const auto font = uiFont (10.5f);
        g.setFont (font);

        for (const auto& actor : actors)
        {
            if (actor.definition == nullptr)
                continue;

            const float labelZ = actor.base.z + actor.definition->labelHeight * actor.scale;
            juce::Point<float> anchor;
            float depth = 0.0f;
            if (! camera.project ({ actor.position.x, actor.position.y, labelZ }, anchor, depth))
                continue;

            placeLabelChip (g, anchor, actor.label, actor.colour, actor.selected,
                            bounds, chips, font);
        }
    }

    void drawCompass (juce::Graphics& g, const Camera& camera,
                      juce::Rectangle<float> bounds)
    {
        const float radius = 28.0f;
        const juce::Point<float> centre (bounds.getRight() - radius - 16.0f,
                                         bounds.getY() + radius + 16.0f);

        g.setColour (Colours::bg.withAlpha (0.72f));
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (Colours::strokeStrong);
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        juce::Point<float> origin;
        float depth = 0.0f;
        if (! camera.project (camera.target, origin, depth))
            origin = centre;

        auto drawAxis = [&] (const Vec3& direction, juce::Colour colour, const juce::String& label)
        {
            juce::Point<float> end;
            float d = 0.0f;
            if (! camera.project (camera.target + direction, end, d))
                return;

            auto vector = end - origin;
            const float vectorLength = vector.getDistanceFromOrigin();
            if (vectorLength < 0.5f)
                return;
            vector = vector / vectorLength;

            const auto tip = centre + vector * 18.0f;
            g.setColour (colour);
            g.drawLine ({ centre, tip }, 1.6f);
            g.fillEllipse (tip.x - 2.0f, tip.y - 2.0f, 4.0f, 4.0f);

            g.setFont (uiFont (8.5f, true));
            g.drawText (label,
                        juce::Rectangle<float> (tip.x + vector.x * 6.0f - 4.0f,
                                                tip.y + vector.y * 6.0f - 5.0f,
                                                10.0f, 10.0f),
                        juce::Justification::centred, false);
        };

        drawAxis ({ 0.0f, 1.0f, 0.0f }, juce::Colour (0xff6fbf83), "Y");
        drawAxis ({ 1.0f, 0.0f, 0.0f }, juce::Colour (0xffd97a6a), "X");
        drawAxis ({ 0.0f, 0.0f, 1.0f }, juce::Colour (0xff6a9fd8), "Z");

        g.setColour (Colours::textSecondary);
        g.fillEllipse (centre.x - 1.5f, centre.y - 1.5f, 3.0f, 3.0f);
    }

    void drawRoomInfo (juce::Graphics& g, PluginProcessor& processor,
                       juce::Rectangle<float> bounds)
    {
        const auto room = getRoomDimensions (processor);
        const int nSrc = getSourceCount (processor);
        const int nRec = getReceiverCount (processor);

        const juce::String line1 = "ROOM  "
            + juce::String (room.width, 1) + " x "
            + juce::String (room.depth, 1) + " x "
            + juce::String (room.height, 1) + " m";
        const juce::String line2 = juce::String (nSrc) + (nSrc == 1 ? " source" : " sources")
            + "  /  " + juce::String (nRec) + (nRec == 1 ? " listener" : " listeners");

        juce::Rectangle<float> panel (bounds.getX() + 14.0f, bounds.getBottom() - 55.0f,
                                      190.0f, 41.0f);
        g.setColour (Colours::bg.withAlpha (0.66f));
        g.fillRoundedRectangle (panel, 4.0f);
        g.setColour (Colours::stroke);
        g.drawRoundedRectangle (panel.reduced (0.5f), 4.0f, 1.0f);

        g.setColour (Colours::textSecondary);
        g.setFont (uiFont (9.5f, true));
        g.drawText (line1, panel.reduced (10.0f, 0.0f).withHeight (20.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (Colours::textMuted);
        g.setFont (uiFont (9.5f));
        g.drawText (line2, panel.reduced (10.0f, 0.0f).withTrimmedTop (20.0f).withHeight (16.0f),
                    juce::Justification::centredLeft, false);
    }

    void drawPreviewBadge (juce::Graphics& g, const RenderOptions& options,
                           bool isPpq, juce::Rectangle<float> bounds)
    {
        if (! options.previewActive)
            return;

        const juce::String text = "PREVIEW  " + formatTime (options.previewTime, isPpq);
        const auto font = uiFont (10.0f, true);
        const float width = font.getStringWidthFloat (text) + 20.0f;
        juce::Rectangle<float> chip (bounds.getCentreX() - width * 0.5f,
                                     bounds.getY() + 14.0f, width, 22.0f);

        g.setColour (Colours::bg.withAlpha (0.90f));
        g.fillRoundedRectangle (chip, 4.0f);
        g.setColour (Colours::time.withAlpha (0.75f));
        g.drawRoundedRectangle (chip.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Colours::time);
        g.setFont (font);
        g.drawText (text, chip, juce::Justification::centred, false);
    }
}

/* ------------------------------------------------------------------------- */

void Camera::setup (juce::Rectangle<float> area)
{
    viewArea = area;
    viewW = juce::jmax (1.0f, area.getWidth());
    viewH = juce::jmax (1.0f, area.getHeight());
    cx = area.getCentreX();
    cy = area.getCentreY();
    tanHalf = std::tan (juce::degreesToRadians (juce::jlimit (20.0f, 80.0f, fovDegrees)) * 0.5f);

    pitch = juce::jlimit (-1.45f, 1.45f, pitch);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    eye = target + Vec3 (std::cos (yaw) * cp, std::sin (yaw) * cp, sp)
                       * juce::jmax (0.5f, distance);

    forward = normalized (target - eye);
    right = normalized (cross (forward, { 0.0f, 0.0f, 1.0f }));
    if (length (right) < 0.5f)
        right = { 1.0f, 0.0f, 0.0f };
    up = cross (right, forward);

    orthoScale = viewH / juce::jmax (0.2f, orthoHeight);
}

Vec3 Camera::toView (const Vec3& world) const
{
    const Vec3 d = world - eye;
    return { dot (d, right), dot (d, up), dot (d, forward) };
}

juce::Point<float> Camera::viewToScreen (const Vec3& view, float& depth) const
{
    depth = view.z;

    if (orthographic)
        return { cx + view.x * orthoScale, cy - view.y * orthoScale };

    const float denominator = juce::jmax (nearPlane, view.z);
    const float scale = (viewH * 0.5f) / tanHalf;
    return { cx + view.x * scale / denominator,
             cy - view.y * scale / denominator };
}

bool Camera::project (const Vec3& world, juce::Point<float>& screen, float& depth) const
{
    const Vec3 view = toView (world);
    if (! orthographic && view.z < nearPlane)
        return false;

    screen = viewToScreen (view, depth);
    return true;
}

bool Camera::projectSegment (const Vec3& a, const Vec3& b,
                             juce::Point<float>& pa, juce::Point<float>& pb) const
{
    Vec3 va = toView (a);
    Vec3 vb = toView (b);

    if (! orthographic)
    {
        if (va.z < nearPlane && vb.z < nearPlane)
            return false;

        if (va.z < nearPlane)
        {
            const float t = (nearPlane - va.z) / (vb.z - va.z);
            va = va + (vb - va) * t;
        }
        else if (vb.z < nearPlane)
        {
            const float t = (nearPlane - vb.z) / (va.z - vb.z);
            vb = vb + (va - vb) * t;
        }
    }

    float d0 = 0.0f, d1 = 0.0f;
    pa = viewToScreen (va, d0);
    pb = viewToScreen (vb, d1);
    return true;
}

bool Camera::rayToPlane (juce::Point<float> screen, float planeZ, Vec3& world) const
{
    const float screenX = screen.x - cx;
    const float screenY = -(screen.y - cy);

    if (orthographic)
    {
        const Vec3 origin = eye + right * (screenX / orthoScale)
                                + up * (screenY / orthoScale);
        if (std::abs (forward.z) < 1.0e-6f)
            return false;
        const float t = (planeZ - origin.z) / forward.z;
        if (t < 0.0f)
            return false;
        world = origin + forward * t;
        return true;
    }

    const float scale = (viewH * 0.5f) / tanHalf;
    const Vec3 direction = normalized (forward + right * (screenX / scale)
                                               + up * (screenY / scale));
    if (std::abs (direction.z) < 1.0e-6f)
        return false;
    const float t = (planeZ - eye.z) / direction.z;
    if (t <= 0.0f)
        return false;

    world = eye + direction * t;
    return true;
}

float Camera::worldPerPixel (float depth) const
{
    if (orthographic)
        return 1.0f / juce::jmax (0.01f, orthoScale);

    return juce::jmax (0.01f, depth) * tanHalf * 2.0f / viewH;
}

void Camera::orbit (float dxPixels, float dyPixels)
{
    yaw -= dxPixels * 0.009f;
    pitch = juce::jlimit (-1.45f, 1.45f, pitch + dyPixels * 0.009f);
}

void Camera::pan (float dxPixels, float dyPixels)
{
    const float perPixel = worldPerPixel (distance);
    target -= right * (dxPixels * perPixel);
    target += up * (dyPixels * perPixel);
}

void Camera::zoom (float factor)
{
    if (orthographic)
        orthoHeight = juce::jlimit (1.0f, 80.0f, orthoHeight * factor);
    else
        distance = juce::jlimit (1.2f, 90.0f, distance * factor);
}

/* ------------------------------------------------------------------------- */

bool SceneRenderer::getActorPosition (PluginProcessor& processor,
                                      bool isReceiver, int index,
                                      const RenderOptions& options,
                                      Vec3& position)
{
    index = juce::jlimit (0, 15, index);

    const juce::String id = (isReceiver ? "receiver" : "source") + juce::String (index);
    position.x = processor.getParameterFloat (id + "X");
    position.y = processor.getParameterFloat (id + "Y");
    position.z = processor.getParameterFloat (id + "Z");

    if (options.previewActive)
    {
        const PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                          : processor.getPathBank().getSourcePath (index);
        if (path.enabled && ! path.keyframes.empty())
            path.evaluate (options.previewTime, position.x, position.y, position.z);
    }

    return true;
}

void SceneRenderer::paint (juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           PluginProcessor& processor,
                           const Camera& cameraToUse,
                           const RenderOptions& options,
                           const Selection& selection)
{
    const auto area = bounds.toFloat();
    if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
        return;

    /* Background */
    g.setGradientFill (juce::ColourGradient (Colours::canvasTop, area.getX(), area.getY(),
                                             Colours::canvasBottom, area.getX(), area.getBottom(),
                                             false));
    g.fillRect (area);

    Camera camera = cameraToUse;
    camera.setup (area);

    /* Slightly stronger vignette at the bottom keeps the overlays legible. */
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, area.getX(), area.getCentreY(),
                                             juce::Colour (0x55000000), area.getX(), area.getBottom(),
                                             false));
    g.fillRect (area);

    drawRoom (g, camera, processor, options);

    const auto actors = collectActors (processor, options, selection);

    std::vector<KeyMarker> keyMarkers;
    drawPaths (g, camera, processor, options, selection, keyMarkers);

    drawShadowsAndDiscs (g, camera, actors);

    std::vector<PolyDraw> polygons;
    polygons.reserve (actors.size() * 220);
    buildActorTriangles (camera, actors, polygons);
    drawPolygons (g, polygons);

    drawAcousticPoints (g, camera, actors);

    std::vector<ChipRect> chips;
    chips.reserve (64);
    drawActorLabels (g, camera, actors, area, chips);
    drawKeyMarkers (g, keyMarkers, options, area, chips);

    drawCompass (g, camera, area);
    drawRoomInfo (g, processor, area);
    drawPreviewBadge (g, options, processor.isTimeInPpq(), area);
}

Selection SceneRenderer::pickActor (juce::Rectangle<int> bounds,
                                    PluginProcessor& processor,
                                    const Camera& cameraToUse,
                                    const RenderOptions& options,
                                    juce::Point<float> mouse)
{
    Camera camera = cameraToUse;
    camera.setup (bounds.toFloat());

    const auto actors = collectActors (processor, options, Selection::none());

    Selection best;
    float bestDistance = 1.0e9f;

    for (const auto& actor : actors)
    {
        if (actor.definition == nullptr)
            continue;

        juce::Point<float> anchor;
        float depth = 0.0f;
        if (! camera.project (actor.position, anchor, depth))
            continue;

        const float bodyZ = actor.definition->aerial
                              ? actor.base.z
                              : juce::jmin (actor.definition->labelHeight * 0.55f * actor.scale,
                                            actor.position.z);
        juce::Point<float> body;
        float bodyDepth = 0.0f;
        if (! camera.project ({ actor.position.x, actor.position.y, bodyZ }, body, bodyDepth))
            body = anchor;

        const float radius = juce::jmax (18.0f, actor.definition->footprint * 28.0f * actor.scale);
        const float distance = juce::jmin (mouse.getDistanceFrom (anchor),
                                           mouse.getDistanceFrom (body));

        if (distance < radius && distance < bestDistance)
        {
            bestDistance = distance;
            best = actor.selection;
        }
    }

    return best;
}

Selection SceneRenderer::pickCheckpoint (juce::Rectangle<int> bounds,
                                        PluginProcessor& processor,
                                        const Camera& cameraToUse,
                                        juce::Point<float> mouse,
                                        const Selection& preferred)
{
    Camera camera = cameraToUse;
    camera.setup (bounds.toFloat());

    const juce::SpinLock::ScopedLockType sl (processor.getPathLock());

    const bool hasPreferred = preferred.isActor() || preferred.isCheckpoint();
    const bool preferredIsReceiver = preferred.isCheckpoint()
                                        ? preferred.checkpointIsReceiver
                                        : preferred.isReceiver();
    const int preferredIndex = preferred.index;

    Selection best;
    bool bestIsPreferred = false;
    float bestDistance = 10.0f;

    for (int side = 0; side < 2; ++side)
    {
        const bool isReceiver = side == 1;
        const int count = isReceiver ? getReceiverCount (processor)
                                     : getSourceCount (processor);

        for (int i = 0; i < count; ++i)
        {
            const PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (i)
                                              : processor.getPathBank().getSourcePath (i);
            const bool pathIsPreferred = hasPreferred
                                      && preferredIndex == i
                                      && preferredIsReceiver == isReceiver;

            for (int k = 0; k < (int) path.keyframes.size(); ++k)
            {
                const auto& keyframe = path.keyframes[(size_t) k];

                juce::Point<float> point;
                float depth = 0.0f;
                if (! camera.project ({ keyframe.x, keyframe.y, keyframe.z },
                                      point, depth))
                    continue;

                const float distance = mouse.getDistanceFrom (point);

                if (distance > 10.0f)
                    continue;

                if (best.isCheckpoint())
                {
                    if (pathIsPreferred && ! bestIsPreferred)
                    {
                        /* A checkpoint on the selected path wins. */
                    }
                    else if (pathIsPreferred != bestIsPreferred
                             || distance >= bestDistance)
                    {
                        continue;
                    }
                }

                best = Selection::checkpoint (isReceiver, i, k);
                bestIsPreferred = pathIsPreferred;
                bestDistance = distance;
            }
        }
    }

    return best;
}

void SceneRenderer::frameRoom (Camera& camera, PluginProcessor& processor,
                               juce::Rectangle<int> bounds)
{
    const auto room = getRoomDimensions (processor);
    const Vec3 centre (room.width * 0.5f, room.depth * 0.5f, room.height * 0.45f);
    const float radius = 0.5f * std::sqrt (room.width * room.width
                                           + room.depth * room.depth
                                           + room.height * room.height);

    const float aspect = juce::jmax (0.2f, (float) bounds.getWidth() / (float) juce::jmax (1, bounds.getHeight()));
    camera.target = centre;

    if (camera.orthographic)
    {
        camera.orthoHeight = juce::jmax (room.height,
                                         juce::jmax (room.depth, room.width / aspect)) * 1.30f;
    }
    else
    {
        camera.distance = juce::jlimit (4.0f, 90.0f,
                                        1.18f * radius / std::sin (juce::degreesToRadians (camera.fovDegrees) * 0.5f));
    }
}

void SceneRenderer::frameSelection (Camera& camera, PluginProcessor& processor,
                                    const Selection& selection,
                                    juce::Rectangle<int> bounds)
{
    if (selection.isNone())
    {
        frameRoom (camera, processor, bounds);
        return;
    }

    RenderOptions options;
    Vec3 position;
    const bool isReceiver = selection.isReceiver()
                         || (selection.isCheckpoint() && selection.checkpointIsReceiver);
    const int index = juce::jlimit (0, 15, selection.index);

    if (selection.isCheckpoint())
    {
        const PathData& path = isReceiver ? processor.getPathBank().getReceiverPath (index)
                                          : processor.getPathBank().getSourcePath (index);
        if (selection.keyframe >= 0 && selection.keyframe < (int) path.keyframes.size())
        {
            const auto& keyframe = path.keyframes[(size_t) selection.keyframe];
            position = { keyframe.x, keyframe.y, keyframe.z };
        }
        else
        {
            getActorPosition (processor, isReceiver, index, options, position);
        }
    }
    else
    {
        getActorPosition (processor, isReceiver, index, options, position);
    }

    ActorInfo info;
    {
        const juce::ScopedLock sl (processor.getSceneLock());
        info = processor.getSceneModel().getActor (isReceiver, index);
    }
    const auto& definition = getActorDefinition (info.kind);
    const float scale = juce::jlimit (0.25f, 3.0f, info.scale);

    camera.target = { position.x, position.y,
                      definition.aerial ? position.z : juce::jmin (position.z, 1.1f) };
    if (camera.orthographic)
        camera.orthoHeight = juce::jlimit (1.5f, 30.0f, 4.2f * scale);
    else
        camera.distance = juce::jlimit (2.0f, 40.0f, 4.6f * juce::jmax (1.0f, scale));
}

void SceneRenderer::setViewPreset (Camera& camera, int preset,
                                   PluginProcessor& processor,
                                   juce::Rectangle<int> bounds)
{
    const auto room = getRoomDimensions (processor);
    const float aspect = juce::jmax (0.2f, (float) bounds.getWidth() / (float) juce::jmax (1, bounds.getHeight()));

    switch (preset)
    {
        case 1: /* top */
            camera.orthographic = true;
            camera.yaw = -juce::MathConstants<float>::halfPi;
            camera.pitch = 1.5f;
            camera.target = { room.width * 0.5f, room.depth * 0.5f, room.height * 0.5f };
            camera.distance = juce::jmax (8.0f, room.width + room.depth);
            camera.orthoHeight = juce::jmax (room.depth, room.width / aspect) * 1.16f;
            break;

        case 2: /* front */
            camera.orthographic = true;
            camera.yaw = -juce::MathConstants<float>::halfPi;
            camera.pitch = 0.0f;
            camera.target = { room.width * 0.5f, room.depth * 0.5f, room.height * 0.5f };
            camera.distance = juce::jmax (8.0f, room.width + room.depth);
            camera.orthoHeight = juce::jmax (room.height, room.width / aspect) * 1.16f;
            break;

        case 3: /* side */
            camera.orthographic = true;
            camera.yaw = 0.0f;
            camera.pitch = 0.0f;
            camera.target = { room.width * 0.5f, room.depth * 0.5f, room.height * 0.5f };
            camera.distance = juce::jmax (8.0f, room.width + room.depth);
            camera.orthoHeight = juce::jmax (room.height, room.depth / aspect) * 1.16f;
            break;

        case 0:
        default:
            camera.orthographic = false;
            camera.yaw = -2.35f;
            camera.pitch = 0.42f;
            frameRoom (camera, processor, bounds);
            break;
    }
}

} // namespace studio
