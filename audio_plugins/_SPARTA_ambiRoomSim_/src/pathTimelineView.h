#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "pathAutomation.h"
#include "ui/SelectionState.h"

class pathTimelineView  : public Component
{
public:
    pathTimelineView (PluginProcessor& p);
    ~pathTimelineView() override;

    /* Live host position (the red playhead). */
    void setScrubTime(double t);
    /* Preview position (the amber line). When the preview is active the
       room view shows the objects at this position instead of their live
       position. */
    void setPreviewTime(double t, bool active);
    bool isPreviewActive() const { return previewActive; }
    double getPreviewTime() const { return previewTime; }

    /* Called when the user clicks the timeline to preview a position. */
    std::function<void(double)> onPreviewRequested;
    /* Selection follows checkpoint/row clicks, in both directions. */
    void setSelection (const studio::Selection& newSelection);
    const studio::Selection& getSelection() const { return selection; }
    std::function<void (studio::Selection)> onSelectionChanged;
    /* Called after a checkpoint has been retimed by dragging. */
    std::function<void()> onPathEdited;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    PluginProcessor& processor;
    void* hAmbi;
    double scrubTime = 0.0;
    double previewTime = 0.0;
    bool previewActive = false;
    bool isDraggingKeyframe = false;
    int dragObjectIdx = -1;
    bool dragIsReceiver = false;
    int dragKeyframeIdx = -1;
    studio::Selection selection;

    struct RowInfo {
        juce::String label;
        int objIdx;
        bool isReceiver;
    };
    juce::Array<RowInfo> rows;
    void rebuildRows();

    float getRowsMaxY() const;
    double getTimelineMaxTime() const;
    float timeToX(double t, double maxTime) const;
    double xToTime(float x, double maxTime) const;
    void paintWaveformStrip(juce::Graphics& g, double maxTime, float rowsMaxY);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(pathTimelineView)
};
