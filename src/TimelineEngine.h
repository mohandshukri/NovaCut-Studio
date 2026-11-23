#pragma once

#include "Types.h"
#include <vector>
#include <optional>

namespace cc {

class TimelineEngine {
public:
    TimelineEngine();

    Project& project() { return m_project; }
    const Project& project() const { return m_project; }

    void resetProject(const std::string& name = "Untitled Project");

    // Asset management
    uint64_t addAsset(const MediaAsset& asset);
    const MediaAsset* findAsset(uint64_t assetId) const;

    // Clip operations (all record undo state)
    uint64_t addClipFromAsset(uint64_t assetId, int trackIndex, double timelineStart);
    uint64_t addTextClip(const std::string& text, double timelineStart, double duration = 4.0);
    bool moveClip(uint64_t clipId, int newTrackIndex, double newStart, bool snap = true);
    bool trimClipStart(uint64_t clipId, double newStart);
    bool trimClipEnd(uint64_t clipId, double newEnd);
    uint64_t splitClipAt(uint64_t clipId, double splitTime);
    bool deleteClip(uint64_t clipId);
    bool rippleDeleteClip(uint64_t clipId);
    bool mergeWithNextClip(uint64_t clipId);

    // Batch 11: Cinema NLE Pro Tools
    bool slipClip(uint64_t clipId, double deltaSourceSec);
    bool slideClip(uint64_t clipId, double deltaTimelineSec);
    uint64_t applyJCut(uint64_t clipId, double leadSec = 1.0);
    uint64_t applyLCut(uint64_t clipId, double trailSec = 1.0);
    bool createCompoundOnTrack(int trackIndex);
    bool importNestedSequence(const Project& nestedProj, double atTimelineSec);
    bool multiCamSwitchAtPlayhead();
    uint64_t duplicateClip(uint64_t clipId);

    // Part 4: Pro Editing Tools
    uint64_t extractAudioFromClip(uint64_t clipId);
    uint64_t freezeFrameAtPlayhead(uint64_t clipId, double freezeDur = 3.0);
    bool trimBeforePlayhead(uint64_t clipId);
    bool trimAfterPlayhead(uint64_t clipId);

    // Part 2: Keyframe operations
    bool addOrUpdateKeyframeAtPlayhead(uint64_t clipId);
    bool removeKeyframeNearPlayhead(uint64_t clipId, double toleranceSec = 0.12);
    bool hasKeyframeNearPlayhead(uint64_t clipId, double toleranceSec = 0.12) const;

    // Selection & lookup
    Clip* findClip(uint64_t clipId);
    const Clip* findClip(uint64_t clipId) const;
    uint64_t selectedClipId() const { return m_selectedClipId; }
    void selectClip(uint64_t clipId) { m_selectedClipId = clipId; }
    Clip* selectedClip() { return findClip(m_selectedClipId); }

    // Playback & timeline queries
    double playhead() const { return m_playhead; }
    void setPlayhead(double sec);
    double totalDuration() const;
    double snapTime(double rawSec, uint64_t ignoreClipId = 0, double thresholdSec = 0.15) const;

    // Returns clips active at time `t`, ordered bottom-to-top for rendering (V1 -> V2 -> T1)
    std::vector<const Clip*> activeVisualClipsAt(double t) const;
    std::vector<const Clip*> activeAudioClipsAt(double t) const;

    // Track helpers
    bool isTrackCompatible(int trackIndex, MediaType mediaType) const;
    int defaultTrackForMedia(MediaType mediaType) const;
    int addTrack(TrackType type);

    // Undo / Redo
    void pushUndoSnapshot();
    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    bool undo();
    bool redo();

private:
    Project m_project;
    uint64_t m_selectedClipId = 0;
    double m_playhead = 0.0;
    std::vector<Project> m_undoStack;
    std::vector<Project> m_redoStack;
    static constexpr size_t kMaxUndoDepth = 64;
};

} // namespace cc
