/**
 * @file track_bench.cpp
 * @brief Runs recorded 40x30 gray clips through MotionDetector + Tracker: regression checks and a host benchmark.
 * @details Usage: track_bench [--repeat N] <clip.gray>... For each clip, reads <clip>.expect (if present) and
 * checks it (a malformed .expect line fails the clip); exits 1 if any expectation fails or a clip is malformed. Built
 * with plain g++ (see ci.yml).
 */

#include "motion_detector.h"
#include "tracker.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace tracking;

namespace
{
struct ClipResult {
    int frames = 0;
    int first_lock = -1;
    int ids = 0;
    int locked = 0;
    int coasting = 0;
    int scene_changes = 0;
    int last_active = -1; ///< last frame index where the tracker was not Idle (-1 = never)
    double ns_per_frame = 0.0;
};

std::string trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

/** Parses <clip>.expect into key=integer pairs. Returns false (with a message in @p error) on a malformed line. */
bool read_expect(const std::string& clip_path, std::map<std::string, long>& out, bool& found, std::string& error)
{
    std::string path = clip_path;
    const auto dot = clip_path.rfind('.');
    const auto slash = clip_path.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
    {
        path = clip_path.substr(0, dot);
    }
    path += ".expect";
    std::ifstream in(path);
    found = static_cast<bool>(in);
    std::string raw;
    int line_no = 0;
    while (std::getline(in, raw))
    {
        ++line_no;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            error = path + ":" + std::to_string(line_no) + ": expected key=value, got '" + line + "'";
            return false;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        char* end = nullptr;
        const long number = std::strtol(value.c_str(), &end, 10);
        if (key.empty() || value.empty() || *end != '\0')
        {
            error = path + ":" + std::to_string(line_no) + ": '" + value + "' is not an integer for key '" + key + "'";
            return false;
        }
        out[key] = number;
    }
    return true;
}

ClipResult run_clip(const std::vector<GrayFrame>& frames, int repeat)
{
    ClipResult r;
    const auto start = std::chrono::steady_clock::now();
    for (int rep = 0; rep < repeat; ++rep)
    {
        MotionDetector detector;
        Tracker tracker;
        std::set<uint32_t> ids;
        ClipResult pass;
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const DetectionResult& det = detector.process(frames[i]);
            const TrackState& t = tracker.update(det);
            pass.scene_changes += det.scene_change ? 1 : 0;
            if (t.state == TrackPhase::Locked)
            {
                ++pass.locked;
                if (pass.first_lock < 0)
                {
                    pass.first_lock = static_cast<int>(i);
                }
            }
            if (t.state != TrackPhase::Idle || t.id != 0)
            {
                pass.last_active = static_cast<int>(i);
            }
            pass.coasting += t.state == TrackPhase::Coasting ? 1 : 0;
            if (t.id)
            {
                ids.insert(t.id);
            }
        }
        pass.ids = static_cast<int>(ids.size());
        r = pass; // every repetition is identical; keep the last
    }
    const auto ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    r.frames = static_cast<int>(frames.size());
    r.ns_per_frame = frames.empty() ? 0.0 : ns / (static_cast<double>(frames.size()) * repeat);
    return r;
}

bool check(const char* key, bool ok, long expected, long actual)
{
    if (!ok)
    {
        std::printf("  FAIL %s: expected %ld, got %ld\n", key, expected, actual);
    }
    return ok;
}
} // namespace

int main(int argc, char** argv)
{
    int repeat = 1;
    std::vector<std::string> clips;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--repeat")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "track_bench: --repeat needs a value\n");
                return 2;
            }
            repeat = std::max(1, std::atoi(argv[++i]));
        } else
        {
            clips.push_back(arg);
        }
    }
    if (clips.empty())
    {
        std::fprintf(stderr, "usage: track_bench [--repeat N] <clip.gray>...\n");
        return 2;
    }

    bool all_ok = true;
    for (const std::string& path : clips)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            std::printf("clip=%s ERROR unreadable\n", path.c_str());
            all_ok = false;
            continue;
        }
        const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (bytes.empty() || bytes.size() % kCells != 0)
        {
            std::printf("clip=%s ERROR size %zu is not a multiple of %d\n", path.c_str(), bytes.size(), kCells);
            all_ok = false;
            continue;
        }
        std::vector<GrayFrame> frames(bytes.size() / kCells);
        for (size_t f = 0; f < frames.size(); ++f)
        {
            for (int i = 0; i < kCells; ++i)
            {
                frames[f][i] = static_cast<uint8_t>(bytes[f * kCells + i]);
            }
        }

        const ClipResult r = run_clip(frames, repeat);
        std::printf("clip=%s frames=%d first_lock=%d last_active=%d ids=%d locked=%d coasting=%d scene_changes=%d "
                    "ns_per_frame=%.0f\n",
                    path.c_str(), r.frames, r.first_lock, r.last_active, r.ids, r.locked, r.coasting, r.scene_changes,
                    r.ns_per_frame);

        bool ok = true;
        std::map<std::string, long> expectations;
        bool has_expect = false;
        std::string error;
        if (!read_expect(path, expectations, has_expect, error))
        {
            std::printf("  FAIL malformed expectation file: %s\n", error.c_str());
            std::printf("  FAIL\n");
            all_ok = false;
            continue;
        }
        if (!has_expect)
        {
            std::printf("  WARN no .expect file\n");
        }
        for (const auto& [key, value] : expectations)
        {
            if (key == "lock_by_frame")
            {
                ok &= check("lock_by_frame", r.first_lock >= 0 && r.first_lock <= value, value, r.first_lock);
            } else if (key == "max_ids")
            {
                ok &= check("max_ids", r.ids <= value, value, r.ids);
            } else if (key == "min_locked_frames")
            {
                ok &= check("min_locked_frames", r.locked >= value, value, r.locked);
            } else if (key == "min_scene_changes")
            {
                ok &= check("min_scene_changes", r.scene_changes >= value, value, r.scene_changes);
            } else if (key == "no_track_after_frame")
            {
                // Idle on every frame from `value` to the end: the last non-Idle frame must be before it.
                ok &= check("no_track_after_frame", r.last_active < value, value, r.last_active);
            } else if (key == "max_scene_changes")
            {
                ok &= check("max_scene_changes", r.scene_changes <= value, value, r.scene_changes);
            } else
            {
                std::printf("  FAIL unknown expectation key '%s'\n", key.c_str());
                ok = false;
            }
        }
        std::printf("  %s\n", ok ? "PASS" : "FAIL");
        all_ok &= ok;
    }
    return all_ok ? 0 : 1;
}
