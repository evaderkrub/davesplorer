#include "ui/Thumbnails.h"
#include "ui/Textures.h"

#include "app/Image.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ui::thumbnails
{

namespace
{

// 300 entries at up to 256x256x4 is 75 MB in the worst case; a typical
// photo folder is far below that.
constexpr size_t kMaxEntries = 300;
constexpr size_t kEvictTo = 250;
constexpr size_t kMaxQueued = 200;   // beyond this the oldest requests are dropped; they re-queue if still shown

struct Entry
{
    Thumb thumb;
    bool  queued = false;
    int   lastUsed = 0;   // ImGui frame count
};

struct Job
{
    std::string key;
    std::string path;
};

struct Result
{
    std::string      key;
    app::DecodedImage image;
    bool             ok = false;
};

// UI thread only.
std::unordered_map<std::string, Entry> g_entries;
bool        g_started = false;
std::thread g_worker;

// Shared with the worker, under g_mutex.
std::mutex              g_mutex;
std::condition_variable g_wake;
std::deque<Job>         g_queue;
std::vector<Result>     g_done;
bool                    g_stop = false;

std::string Key(const std::string& path, int64_t modified)
{
    return path + '|' + std::to_string(modified);
}

void WorkerMain()
{
    for (;;)
        {
        Job job;
        {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_wake.wait(lock, [] { return g_stop || !g_queue.empty(); });
        if (g_stop) return;
        // Newest first: the cells scrolled into view most recently are the
        // ones the user is looking at.
        job = std::move(g_queue.back());
        g_queue.pop_back();
        }
        Result result;
        result.key = job.key;
        std::string error;
        result.ok = app::DecodeImageFile(job.path, kDim, result.image, error);
        std::lock_guard<std::mutex> lock(g_mutex);
        g_done.push_back(std::move(result));
        }
}

void EnsureStarted()
{
    if (g_started) return;
    g_started = true;
    g_stop = false;
    g_worker = std::thread(WorkerMain);
}

void Enqueue(const std::string& key, const std::string& path)
{
    std::string dropped;
    {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_queue.push_back(Job{ key, path });
    if (g_queue.size() > kMaxQueued)
        {
        dropped = g_queue.front().key;
        g_queue.pop_front();
        }
    }
    g_wake.notify_one();
    // A dropped request is no longer queued; if its cell is still on
    // screen the next Get() asks again.
    if (!dropped.empty())
        {
        auto it = g_entries.find(dropped);
        if (it != g_entries.end()) it->second.queued = false;
        }
}

} // namespace

const Thumb* Get(const std::string& path, int64_t modified)
{
    EnsureStarted();
    const std::string key = Key(path, modified);
    const int frame = ImGui::GetFrameCount();
    auto it = g_entries.find(key);
    if (it == g_entries.end())
        {
        Entry e;
        e.queued = true;
        e.lastUsed = frame;
        g_entries.emplace(key, e);
        Enqueue(key, path);
        return nullptr;
        }
    Entry& e = it->second;
    e.lastUsed = frame;
    if (e.thumb.ready()) return &e.thumb;
    if (!e.queued)
        {
        e.queued = true;
        Enqueue(key, path);
        }
    return nullptr;
}

void Pump()
{
    std::vector<Result> done;
    {
    std::lock_guard<std::mutex> lock(g_mutex);
    done.swap(g_done);
    }
    for (Result& r : done)
        {
        auto it = g_entries.find(r.key);
        if (it == g_entries.end()) continue;   // evicted while decoding
        Entry& e = it->second;
        e.queued = false;
        if (r.ok && r.image.width > 0 && r.image.height > 0)
            {
            e.thumb.texture = textures::Create(r.image.width, r.image.height, r.image.rgba.data());
            e.thumb.width = r.image.width;
            e.thumb.height = r.image.height;
            e.thumb.sourceWidth = r.image.sourceWidth;
            e.thumb.sourceHeight = r.image.sourceHeight;
            }
        else
            {
            e.thumb.failed = true;
            }
        }

    if (g_entries.size() <= kMaxEntries) return;
    // Over budget: drop the entries not shown this frame, oldest first.
    // In-flight decodes are kept so their result is not wasted.
    const int frame = ImGui::GetFrameCount();
    std::vector<std::pair<int, std::string>> candidates;
    for (const auto& [key, e] : g_entries)
        if (e.lastUsed != frame && !e.queued) candidates.emplace_back(e.lastUsed, key);
    std::sort(candidates.begin(), candidates.end());
    for (const auto& [used, key] : candidates)
        {
        if (g_entries.size() <= kEvictTo) break;
        auto it = g_entries.find(key);
        textures::Release(it->second.thumb.texture);
        g_entries.erase(it);
        }
}

void Shutdown()
{
    if (g_started)
        {
        {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_stop = true;
        }
        g_wake.notify_all();
        g_worker.join();
        g_started = false;
        }
    for (auto& [key, e] : g_entries) textures::Release(e.thumb.texture);
    g_entries.clear();
    g_queue.clear();
    g_done.clear();
}

} // namespace ui::thumbnails
