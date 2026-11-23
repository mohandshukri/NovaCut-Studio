#include "TimelineEngine.h"
#include <algorithm>
#include <cmath>

namespace cc {

TimelineEngine::TimelineEngine() {
    resetProject();
}

void TimelineEngine::resetProject(const std::string& name) {
    m_project = Project{};
    m_project.name = name;
    m_project.initDefaultTracks();
    m_selectedClipId = 0;
    m_playhead = 0.0;
    m_undoStack.clear();
    m_redoStack.clear();
}

void TimelineEngine::pushUndoSnapshot() {
    m_undoStack.push_back(m_project);
    if (m_undoStack.size() > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
}

bool TimelineEngine::undo() {
    if (m_undoStack.empty()) return false;
    m_redoStack.push_back(m_project);
    m_project = m_undoStack.back();
    m_undoStack.pop_back();
    if (m_selectedClipId != 0 && !findClip(m_selectedClipId)) {
        m_selectedClipId = 0;
    }
    return true;
}

bool TimelineEngine::redo() {
    if (m_redoStack.empty()) return false;
    m_undoStack.push_back(m_project);
    m_project = m_redoStack.back();
    m_redoStack.pop_back();
    if (m_selectedClipId != 0 && !findClip(m_selectedClipId)) {
        m_selectedClipId = 0;
    }
    return true;
}

uint64_t TimelineEngine::addAsset(const MediaAsset& asset) {
    MediaAsset a = asset;
    a.id = m_project.nextAssetId++;
    if (a.durationSec <= 0.0) a.durationSec = 5.0;
    m_project.assets.push_back(a);
    return a.id;
}

const MediaAsset* TimelineEngine::findAsset(uint64_t assetId) const {
    for (const auto& a : m_project.assets) {
        if (a.id == assetId) return &a;
    }
    return nullptr;
}

bool TimelineEngine::isTrackCompatible(int trackIndex, MediaType mediaType) const {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(m_project.tracks.size())) return false;
    TrackType tt = m_project.tracks[trackIndex].type;
    if (mediaType == MediaType::Text) return tt == TrackType::Text;
    if (mediaType == MediaType::Audio) return tt == TrackType::Audio;
    return tt == TrackType::Video;
}

int TimelineEngine::defaultTrackForMedia(MediaType mediaType) const {
    if (mediaType == MediaType::Text) {
        for (int i = static_cast<int>(m_project.tracks.size()) - 1; i >= 0; --i)
            if (m_project.tracks[i].type == TrackType::Text) return i;
        return 0;
    }
    if (mediaType == MediaType::Audio) {
        for (int i = 0; i < static_cast<int>(m_project.tracks.size()); ++i)
            if (m_project.tracks[i].type == TrackType::Audio) return i;
        return static_cast<int>(m_project.tracks.size()) - 1;
    }
    // Main video track = bottom-most Video track (V1)
    for (int i = static_cast<int>(m_project.tracks.size()) - 1; i >= 0; --i)
        if (m_project.tracks[i].type == TrackType::Video) return i;
    return 0;
}

int TimelineEngine::addTrack(TrackType type) {
    pushUndoSnapshot();
    int countOfType = 0;
    for (const auto& t : m_project.tracks) {
        if (t.type == type) ++countOfType;
    }
    std::string prefix = (type == TrackType::Text) ? "T" : (type == TrackType::Audio ? "A" : "V");
    std::string label = prefix + std::to_string(countOfType + 1);

    int insertPos = static_cast<int>(m_project.tracks.size());
    if (type == TrackType::Text) {
        insertPos = 0; // Add new Text track at very top (T2 above T1)
    } else if (type == TrackType::Video) {
        // Insert right after all Text tracks (V3 above V2)
        insertPos = 0;
        while (insertPos < static_cast<int>(m_project.tracks.size()) &&
               m_project.tracks[insertPos].type == TrackType::Text) {
            ++insertPos;
        }
    } else {
        // Append Audio track at bottom (A2 below A1)
        insertPos = static_cast<int>(m_project.tracks.size());
    }

    Track newTr;
    newTr.index = insertPos;
    newTr.label = label;
    newTr.type = type;
    m_project.tracks.insert(m_project.tracks.begin() + insertPos, newTr);

    // Re-index tracks & shift existing clips at or below insertPos
    for (int i = 0; i < static_cast<int>(m_project.tracks.size()); ++i) {
        m_project.tracks[i].index = i;
    }
    for (auto& c : m_project.clips) {
        if (c.trackIndex >= insertPos) ++c.trackIndex;
    }
    return insertPos;
}

uint64_t TimelineEngine::addClipFromAsset(uint64_t assetId, int trackIndex, double timelineStart) {
    const MediaAsset* asset = findAsset(assetId);
    if (!asset) return 0;
    if (!isTrackCompatible(trackIndex, asset->type)) {
        trackIndex = defaultTrackForMedia(asset->type);
    }
    if (timelineStart < 0.0) timelineStart = 0.0;

    pushUndoSnapshot();
    Clip c;
    c.id = m_project.nextClipId++;
    c.assetId = asset->id;
    c.trackIndex = trackIndex;
    c.type = asset->type;
    c.name = asset->name;
    c.sourcePath = asset->filePath;
    c.timelineStart = timelineStart;
    c.sourceIn = 0.0;
    c.duration = asset->durationSec;
    m_project.clips.push_back(c);
    m_selectedClipId = c.id;
    return c.id;
}

uint64_t TimelineEngine::addTextClip(const std::string& text, double timelineStart, double duration) {
    if (timelineStart < 0.0) timelineStart = 0.0;
    if (duration < 0.2) duration = 0.2;

    pushUndoSnapshot();
    Clip c;
    c.id = m_project.nextClipId++;
    c.assetId = 0;
    c.trackIndex = defaultTrackForMedia(MediaType::Text);
    c.type = MediaType::Text;
    c.name = text.empty() ? "Text" : text.substr(0, 24);
    c.timelineStart = timelineStart;
    c.duration = duration;
    c.text.content = text.empty() ? "CapCutVS Title" : text;
    m_project.clips.push_back(c);
    m_selectedClipId = c.id;
    return c.id;
}

Clip* TimelineEngine::findClip(uint64_t clipId) {
    for (auto& c : m_project.clips) {
        if (c.id == clipId) return &c;
    }
    return nullptr;
}

const Clip* TimelineEngine::findClip(uint64_t clipId) const {
    for (const auto& c : m_project.clips) {
        if (c.id == clipId) return &c;
    }
    return nullptr;
}

bool TimelineEngine::moveClip(uint64_t clipId, int newTrackIndex, double newStart, bool snap) {
    Clip* c = findClip(clipId);
    if (!c) return false;
    if (!isTrackCompatible(newTrackIndex, c->type)) {
        newTrackIndex = c->trackIndex;
    }
    if (newTrackIndex >= 0 && newTrackIndex < static_cast<int>(m_project.tracks.size()) &&
        m_project.tracks[newTrackIndex].locked) {
        return false;
    }
    if (newStart < 0.0) newStart = 0.0;
    if (snap) {
        double snappedStart = snapTime(newStart, clipId);
        double snappedEnd = snapTime(newStart + c->duration, clipId);
        if (std::abs(snappedStart - newStart) <= std::abs(snappedEnd - (newStart + c->duration))) {
            newStart = snappedStart;
        } else {
            newStart = std::max(0.0, snappedEnd - c->duration);
        }
    }
    c->trackIndex = newTrackIndex;
    c->timelineStart = std::max(0.0, newStart);
    return true;
}

bool TimelineEngine::trimClipStart(uint64_t clipId, double newStart) {
    Clip* c = findClip(clipId);
    if (!c) return false;
    constexpr double kMinDur = 0.2;
    double end = c->timelineEnd();
    if (newStart < 0.0) newStart = 0.0;
    if (newStart > end - kMinDur) newStart = end - kMinDur;

    double delta = newStart - c->timelineStart;
    if (c->type == MediaType::Video || c->type == MediaType::Audio) {
        double newSourceIn = c->sourceIn + delta * c->speed;
        if (newSourceIn < 0.0) {
            delta = -c->sourceIn / c->speed;
            newStart = c->timelineStart + delta;
            newSourceIn = 0.0;
        }
        c->sourceIn = newSourceIn;
    }
    c->timelineStart = newStart;
    c->duration = end - newStart;
    return true;
}

bool TimelineEngine::trimClipEnd(uint64_t clipId, double newEnd) {
    Clip* c = findClip(clipId);
    if (!c) return false;
    constexpr double kMinDur = 0.2;
    if (newEnd < c->timelineStart + kMinDur) {
        newEnd = c->timelineStart + kMinDur;
    }
    if (c->type == MediaType::Video || c->type == MediaType::Audio) {
        if (const MediaAsset* a = findAsset(c->assetId)) {
            double maxTimelineDur = (a->durationSec - c->sourceIn) / std::max(0.1, c->speed);
            if (maxTimelineDur > kMinDur && (newEnd - c->timelineStart) > maxTimelineDur) {
                newEnd = c->timelineStart + maxTimelineDur;
            }
        }
    }
    c->duration = newEnd - c->timelineStart;
    return true;
}

uint64_t TimelineEngine::splitClipAt(uint64_t clipId, double splitTime) {
    const Clip* orig = findClip(clipId);
    if (!orig) return 0;
    constexpr double kMinPart = 0.15;
    if (splitTime <= orig->timelineStart + kMinPart || splitTime >= orig->timelineEnd() - kMinPart) {
        return 0;
    }

    pushUndoSnapshot();
    Clip* left = findClip(clipId);
    if (!left) return 0;

    double leftDur = splitTime - left->timelineStart;
    double rightDur = left->duration - leftDur;

    Clip right = *left;
    right.id = m_project.nextClipId++;
    right.timelineStart = splitTime;
    right.duration = rightDur;
    if (right.type == MediaType::Video || right.type == MediaType::Audio) {
        right.sourceIn = left->sourceIn + leftDur * left->speed;
    }

    left->duration = leftDur;
    m_project.clips.push_back(right);
    m_selectedClipId = right.id;
    return right.id;
}

bool TimelineEngine::deleteClip(uint64_t clipId) {
    auto it = std::find_if(m_project.clips.begin(), m_project.clips.end(),
        [clipId](const Clip& c) { return c.id == clipId; });
    if (it == m_project.clips.end()) return false;

    pushUndoSnapshot();
    auto it2 = std::find_if(m_project.clips.begin(), m_project.clips.end(),
        [clipId](const Clip& c) { return c.id == clipId; });
    if (it2 != m_project.clips.end()) {
        m_project.clips.erase(it2);
    }
    if (m_selectedClipId == clipId) m_selectedClipId = 0;
    return true;
}

uint64_t TimelineEngine::duplicateClip(uint64_t clipId) {
    const Clip* orig = findClip(clipId);
    if (!orig) return 0;
    pushUndoSnapshot();
    const Clip* src = findClip(clipId);
    if (!src) return 0;

    Clip copy = *src;
    copy.id = m_project.nextClipId++;
    copy.timelineStart = src->timelineEnd();
    m_project.clips.push_back(copy);
    m_selectedClipId = copy.id;
    return copy.id;
}

void TimelineEngine::setPlayhead(double sec) {
    if (sec < 0.0) sec = 0.0;
    m_playhead = sec;
}

double TimelineEngine::totalDuration() const {
    double maxEnd = 0.0;
    for (const auto& c : m_project.clips) {
        if (c.timelineEnd() > maxEnd) maxEnd = c.timelineEnd();
    }
    return maxEnd;
}

double TimelineEngine::snapTime(double rawSec, uint64_t ignoreClipId, double thresholdSec) const {
    double best = rawSec;
    double bestDist = thresholdSec;

    auto consider = [&](double candidate) {
        double d = std::abs(rawSec - candidate);
        if (d < bestDist) {
            bestDist = d;
            best = candidate;
        }
    };

    consider(0.0);
    consider(m_playhead);
    for (const auto& c : m_project.clips) {
        if (c.id == ignoreClipId) continue;
        consider(c.timelineStart);
        consider(c.timelineEnd());
    }
    return best;
}

std::vector<const Clip*> TimelineEngine::activeVisualClipsAt(double t) const {
    std::vector<const Clip*> result;
    for (const auto& c : m_project.clips) {
        if (c.type == MediaType::Audio) continue;
        if (c.trackIndex < 0 || c.trackIndex >= static_cast<int>(m_project.tracks.size())) continue;
        if (m_project.tracks[c.trackIndex].hidden) continue;
        if (t >= c.timelineStart && t < c.timelineEnd()) {
            result.push_back(&c);
        }
    }
    // Render bottom-to-top: V1 (index 2) -> V2 (index 1) -> T1 (index 0)
    std::sort(result.begin(), result.end(), [](const Clip* a, const Clip* b) {
        if (a->trackIndex != b->trackIndex) return a->trackIndex > b->trackIndex;
        return a->id < b->id;
    });
    return result;
}

std::vector<const Clip*> TimelineEngine::activeAudioClipsAt(double t) const {
    std::vector<const Clip*> result;
    for (const auto& c : m_project.clips) {
        if (c.muted || c.type == MediaType::Text || c.type == MediaType::Image) continue;
        if (c.trackIndex < 0 || c.trackIndex >= static_cast<int>(m_project.tracks.size())) continue;
        if (m_project.tracks[c.trackIndex].muted) continue;
        if (t >= c.timelineStart && t < c.timelineEnd()) {
            result.push_back(&c);
        }
    }
    return result;
}


bool TimelineEngine::addOrUpdateKeyframeAtPlayhead(uint64_t clipId) {
    Clip* c = findClip(clipId);
    if (!c) return false;
    double localT = std::clamp(m_playhead - c->timelineStart, 0.0, c->duration);
    pushUndoSnapshot();
    c = findClip(clipId);
    if (!c) return false;

    for (auto& k : c->keyframes) {
        if (std::abs(k.localTime - localT) < 0.08) {
            k.posX = c->transform.posX;
            k.posY = c->transform.posY;
            k.scale = c->transform.scale;
            k.rotationDeg = c->transform.rotationDeg;
            k.opacity = c->color.opacity;
            return true;
        }
    }
    Keyframe kf;
    kf.localTime = localT;
    kf.posX = c->transform.posX;
    kf.posY = c->transform.posY;
    kf.scale = c->transform.scale;
    kf.rotationDeg = c->transform.rotationDeg;
    kf.opacity = c->color.opacity;
    c->keyframes.push_back(kf);
    std::sort(c->keyframes.begin(), c->keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.localTime < b.localTime; });
    return true;
}

bool TimelineEngine::removeKeyframeNearPlayhead(uint64_t clipId, double toleranceSec) {
    Clip* c = findClip(clipId);
    if (!c || c->keyframes.empty()) return false;
    double localT = m_playhead - c->timelineStart;
    auto it = std::find_if(c->keyframes.begin(), c->keyframes.end(),
        [&](const Keyframe& k) { return std::abs(k.localTime - localT) <= toleranceSec; });
    if (it == c->keyframes.end()) return false;

    pushUndoSnapshot();
    c = findClip(clipId);
    if (!c) return false;
    auto it2 = std::find_if(c->keyframes.begin(), c->keyframes.end(),
        [&](const Keyframe& k) { return std::abs(k.localTime - localT) <= toleranceSec; });
    if (it2 != c->keyframes.end()) c->keyframes.erase(it2);
    return true;
}

bool TimelineEngine::hasKeyframeNearPlayhead(uint64_t clipId, double toleranceSec) const {
    const Clip* c = findClip(clipId);
    if (!c) return false;
    double localT = m_playhead - c->timelineStart;
    for (const auto& k : c->keyframes) {
        if (std::abs(k.localTime - localT) <= toleranceSec) return true;
    }
    return false;
}


uint64_t TimelineEngine::extractAudioFromClip(uint64_t clipId) {
    const Clip* orig = findClip(clipId);
    if (!orig || orig->type != MediaType::Video || orig->sourcePath.empty()) return 0;

    pushUndoSnapshot();
    Clip* src = findClip(clipId);
    if (!src) return 0;

    Clip aud;
    aud.id = m_project.nextClipId++;
    aud.assetId = src->assetId;
    aud.trackIndex = defaultTrackForMedia(MediaType::Audio);
    aud.type = MediaType::Audio;
    aud.name = src->name + " (Audio)";
    aud.sourcePath = src->sourcePath;
    aud.timelineStart = src->timelineStart;
    aud.sourceIn = src->sourceIn;
    aud.duration = src->duration;
    aud.speed = src->speed;
    aud.volume = src->volume;
    aud.audioFadeIn = src->audioFadeIn;
    aud.audioFadeOut = src->audioFadeOut;

    src->muted = true; // Mute original video clip
    m_project.clips.push_back(aud);
    m_selectedClipId = aud.id;
    return aud.id;
}

uint64_t TimelineEngine::freezeFrameAtPlayhead(uint64_t clipId, double freezeDur) {
    const Clip* orig = findClip(clipId);
    if (!orig || orig->type != MediaType::Video) return 0;
    if (m_playhead < orig->timelineStart || m_playhead > orig->timelineEnd()) return 0;

    pushUndoSnapshot();
    Clip* src = findClip(clipId);
    if (!src) return 0;

    double frozenSrcTime = src->sourceIn + (m_playhead - src->timelineStart) * src->speed;
    Clip frozen = *src;
    frozen.id = m_project.nextClipId++;
    frozen.name = src->name + " [Freeze]";
    frozen.timelineStart = m_playhead;
    frozen.sourceIn = frozenSrcTime;
    frozen.duration = std::max(0.5, freezeDur);
    frozen.freezeFrame = true;
    frozen.muted = true;
    frozen.keyframes.clear();
    frozen.trackIndex = (src->trackIndex == 2) ? 1 : src->trackIndex; // Place on V2 overlay if on V1

    m_project.clips.push_back(frozen);
    m_selectedClipId = frozen.id;
    return frozen.id;
}


bool TimelineEngine::trimBeforePlayhead(uint64_t clipId) {
    const Clip* orig = findClip(clipId);
    if (!orig) return false;
    if (m_playhead <= orig->timelineStart + 0.05 || m_playhead >= orig->timelineEnd() - 0.2) return false;
    pushUndoSnapshot();
    return trimClipStart(clipId, m_playhead);
}

bool TimelineEngine::trimAfterPlayhead(uint64_t clipId) {
    const Clip* orig = findClip(clipId);
    if (!orig) return false;
    if (m_playhead <= orig->timelineStart + 0.2 || m_playhead >= orig->timelineEnd() - 0.05) return false;
    pushUndoSnapshot();
    return trimClipEnd(clipId, m_playhead);
}


bool TimelineEngine::rippleDeleteClip(uint64_t clipId) {
    const Clip* orig = findClip(clipId);
    if (!orig) return false;
    int tr = orig->trackIndex;
    double delStart = orig->timelineStart;
    double delDur = orig->duration;

    pushUndoSnapshot();
    auto it = std::find_if(m_project.clips.begin(), m_project.clips.end(),
                           [clipId](const Clip& c) { return c.id == clipId; });
    if (it != m_project.clips.end()) m_project.clips.erase(it);
    for (auto& c : m_project.clips) {
        if (c.trackIndex == tr && c.timelineStart >= delStart + 0.01) {
            c.timelineStart = std::max(0.0, c.timelineStart - delDur);
        }
    }
    if (m_selectedClipId == clipId) m_selectedClipId = 0;
    return true;
}


bool TimelineEngine::mergeWithNextClip(uint64_t clipId) {
    const Clip* cur = findClip(clipId);
    if (!cur) return false;
    int tr = cur->trackIndex;
    double curEnd = cur->timelineEnd();

    // Find the nearest clip after cur on the same track
    uint64_t nextId = 0;
    double bestStart = 1e12;
    for (const auto& c : m_project.clips) {
        if (c.id == clipId || c.trackIndex != tr) continue;
        if (c.timelineStart >= cur->timelineStart + 0.01 && c.timelineStart < bestStart) {
            bestStart = c.timelineStart;
            nextId = c.id;
        }
    }
    if (nextId == 0) return false;

    pushUndoSnapshot();
    Clip* left = findClip(clipId);
    Clip* right = findClip(nextId);
    if (!left || !right) return false;

    // If same source media (e.g. previously split), combine directly into one seamless clip!
    if (left->type == right->type && left->sourcePath == right->sourcePath) {
        left->duration += right->duration;
        auto it = std::find_if(m_project.clips.begin(), m_project.clips.end(),
                               [nextId](const Clip& c) { return c.id == nextId; });
        if (it != m_project.clips.end()) m_project.clips.erase(it);
    } else {
        // Different clips: snap tightly together (zero gap) & apply smooth crossfade!
        right->timelineStart = curEnd;
        if (right->transitionIn == TransitionType::None) {
            right->transitionIn = TransitionType::Fade;
            right->transitionDuration = 0.35;
        }
    }
    return true;
}


bool TimelineEngine::slipClip(uint64_t clipId, double deltaSourceSec) {
    Clip* c = findClip(clipId);
    if (!c || (c->type != MediaType::Video && c->type != MediaType::Audio)) return false;
    pushUndoSnapshot();
    c = findClip(clipId);
    c->sourceIn = std::max(0.0, c->sourceIn + deltaSourceSec);
    return true;
}

bool TimelineEngine::slideClip(uint64_t clipId, double deltaTimelineSec) {
    const Clip* orig = findClip(clipId);
    if (!orig) return false;
    int tr = orig->trackIndex;
    double oldStart = orig->timelineStart;
    double oldEnd   = orig->timelineEnd();
    double newStart = std::max(0.0, oldStart + deltaTimelineSec);
    double actualDelta = newStart - oldStart;

    pushUndoSnapshot();
    for (auto& c : m_project.clips) {
        if (c.trackIndex != tr || c.id == clipId) continue;
        if (std::abs(c.timelineEnd() - oldStart) < 0.15) {
            c.duration = std::max(0.2, c.duration + actualDelta);
        } else if (std::abs(c.timelineStart - oldEnd) < 0.15) {
            c.timelineStart = std::max(0.0, c.timelineStart + actualDelta);
            c.duration = std::max(0.2, c.duration - actualDelta);
        }
    }
    if (Clip* c = findClip(clipId)) c->timelineStart = newStart;
    return true;
}

uint64_t TimelineEngine::applyJCut(uint64_t clipId, double leadSec) {
    uint64_t audId = extractAudioFromClip(clipId);
    if (audId == 0) return 0;
    if (Clip* a = findClip(audId)) {
        double shift = std::min(leadSec, a->timelineStart);
        a->timelineStart -= shift;
        a->duration += shift;
        a->audioFadeIn = 0.25f;
    }
    return audId;
}

uint64_t TimelineEngine::applyLCut(uint64_t clipId, double trailSec) {
    uint64_t audId = extractAudioFromClip(clipId);
    if (audId == 0) return 0;
    if (Clip* a = findClip(audId)) {
        a->duration += trailSec;
        a->audioFadeOut = 0.35f;
    }
    return audId;
}

bool TimelineEngine::createCompoundOnTrack(int trackIndex) {
    std::vector<uint64_t> ids;
    for (const auto& c : m_project.clips) {
        if (c.trackIndex == trackIndex) ids.push_back(c.id);
    }
    if (ids.size() < 2) return false;
    pushUndoSnapshot();
    double minS = 1e12, maxE = 0.0;
    Clip firstCopy;
    bool gotFirst = false;
    for (const auto& c : m_project.clips) {
        if (c.trackIndex == trackIndex) {
            if (!gotFirst || c.timelineStart < minS) { firstCopy = c; gotFirst = true; }
            minS = std::min(minS, c.timelineStart);
            maxE = std::max(maxE, c.timelineEnd());
        }
    }
    m_project.clips.erase(std::remove_if(m_project.clips.begin(), m_project.clips.end(),
        [trackIndex](const Clip& c) { return c.trackIndex == trackIndex; }), m_project.clips.end());
    firstCopy.id = m_project.nextClipId++;
    firstCopy.name = "[Compound] " + firstCopy.name;
    firstCopy.timelineStart = minS;
    firstCopy.duration = std::max(0.5, maxE - minS);
    m_project.clips.push_back(firstCopy);
    m_selectedClipId = firstCopy.id;
    return true;
}

bool TimelineEngine::importNestedSequence(const Project& nestedProj, double atTimelineSec) {
    if (nestedProj.clips.empty()) return false;
    pushUndoSnapshot();
    for (const auto& a : nestedProj.assets) {
        MediaAsset copy = a;
        copy.id = m_project.nextAssetId++;
        m_project.assets.push_back(copy);
    }
    for (const auto& c : nestedProj.clips) {
        Clip copy = c;
        copy.id = m_project.nextClipId++;
        copy.name = "[Nested] " + copy.name;
        copy.timelineStart += std::max(0.0, atTimelineSec);
        copy.trackIndex = defaultTrackForMedia(copy.type);
        m_project.clips.push_back(copy);
    }
    return true;
}

bool TimelineEngine::multiCamSwitchAtPlayhead() {
    // Splits the top overlay video clip (V2) at playhead and toggles its opacity/cut to switch between Cam A (V1) and Cam B (V2)
    for (const auto& c : m_project.clips) {
        if (c.type == MediaType::Video && c.trackIndex == 1 && m_playhead > c.timelineStart + 0.2 && m_playhead < c.timelineEnd() - 0.2) {
            uint64_t rightId = splitClipAt(c.id, m_playhead);
            if (Clip* r = findClip(rightId)) {
                r->color.opacity = (r->color.opacity > 0.5f) ? 0.0f : 1.0f;
                return true;
            }
        }
    }
    return false;
}

} // namespace cc






