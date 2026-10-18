// NVN command buffers (docs/NVN.md, N3): recording. Each nvnCommandBuffer* call appends a Cmd to the buffer's list;
// nvnCommandBufferEndRecording turns the list into an NVNcommandHandle, which the game submits (possibly many times)
// or calls from other command buffers. Replaying into Vulkan is nvn_vk.cpp's job.
//
// Signatures from the game's call sites (nvn_trace.txt): SetRenderTargets(cmd, n, const NVNtexture* const* colors,
// const NVNtextureView* const* views, const NVNtexture* depth, const NVNtextureView* depthView);
// ClearColor(cmd, int index, const float* rgba, int channelMask); ClearDepthStencil(cmd, float depth [s0],
// NVNboolean depthMask, int stencil, int stencilMask); SetViewport / SetScissor(cmd, int x, int y, int w, int h);
// CallCommands(cmd, int count, const NVNcommandHandle*).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <map>

#include "nvn.h"
#include "nvn_shaders.h"

namespace nx::nvn {

std::shared_ptr<CmdList> peek_list(uint64_t handle);

namespace {

struct CommandBuffer : Object {
    std::mutex lock;
    std::shared_ptr<CmdList> recording = std::make_shared<CmdList>();
    // The memory the game hands over (AddCommandMemory / AddControlMemory) and what recording "uses" of it. Engines
    // look at the used amounts right after EndRecording (a list that used nothing is dropped, never submitted) and
    // at the free ones (to add memory in time), so usage grows with the commands recorded, survives EndRecording
    // and BeginRecording like the real memory does. El callback repone cada bloque antes de agotarlo;
    // los índices marcan los comandos que consumen el bloque actual.
    uint64_t memory_callback = 0, memory_callback_data = 0;
    uint64_t copy_row_stride = 0, copy_image_stride = 0;
    size_t command_mark = 0, control_mark = 0;
    uint64_t command_size = 0, control_size = 0;
    uint64_t command_base = 0, control_base = 0;  // usage before this recording
    uint64_t command_memory = 0;                  // the memory added last: its GPU address (see AddCommandMemory)
    uint64_t command_pool = 0;                    // its NVNmemoryPool*
};

constexpr uint64_t kCommandBytes = 16, kControlBytes = 4;  // per recorded command

uint64_t used(uint64_t size, uint64_t base, size_t cmds, uint64_t per) {
    return std::min(base + cmds * per, size);
}

void note_command_range(CommandBuffer& cb, bool final) {
    const size_t count = cb.recording->cmds.size() - cb.command_mark + unsigned(final);
    if (!count || !cb.command_memory || !cb.command_size) return;
    const uint64_t start = cb.command_memory + cb.command_base;
    const uint64_t end = cb.command_memory + used(cb.command_size, cb.command_base, count, kCommandBytes);
    if (end > start) cb.recording->command_ranges.push_back({start, end, cb.command_pool});
}

// A handle lives as long as its command memory: on the GPU a list is valid until the game records over the memory it
// used, so a newer list recorded over the same bytes (our usage estimate, command_base) drops the older ones. Engines
// record some lists once and call them for a long time: Wonder's file select called handles 0x77a/0x77b recorded ~100k
// lists earlier, and every character's draw lists are recorded when a course loads and first called only after the
// intro (a count limit dropped them before that and the characters were never drawn, docs/nvn/DEBUG.md #26).
// kKeepHandles is only a safety net: past it the oldest lists that were used but not since they were queued go.
struct HandleEntry {
    std::shared_ptr<CmdList> list;
    uint64_t queued = 0, used = 0;  // g_handle_clock values
    uint64_t start = 0, end = 0;    // command memory bytes: memory address + usage estimate
    uint64_t pool = 0;              // the NVNmemoryPool* of that memory
    uint64_t cb = 0;                // the NVNcommandBuffer* it was recorded in
    double t = 0;                   // seconds since the first list, when it was recorded
    uint64_t calls = 0;             // lookups (replays and submits)
};
uint64_t g_cross_pool_drops = 0;  // lists dropped by a list recorded in another pool (only with NX_CMD_REGION_OLD)

// Where command memory is, for the overlap rule above: the pool's GPU address plus the offset. It was the
// NVNmemoryPool* (the guest struct's address) plus the offset: pool structs sit a few hundred bytes apart while the
// offsets reach megabytes, so a list recorded in one pool "overlapped" lists of another and dropped them. Wonder records
// its draw lists when a course loads; a dropped one left its draws on the vertex buffers bound before them (exploded
// geometry at course 1-1's start for the session, DEBUG-2 #90). NX_CMD_REGION_OLD=1: the old key.
uint64_t command_memory_address(uint64_t pool, uint64_t offset) {
    static const bool old = std::getenv("NX_CMD_REGION_OLD") != nullptr;
    if (!old)
        if (auto* p = get<MemoryPool>(pool)) return p->gpu + offset;
    return pool + offset;
}
std::map<uint64_t, std::pair<uint64_t, uint64_t>> g_regions;  // start -> (end, handle)
std::mutex g_handles_lock;
std::map<uint64_t, HandleEntry> g_handles;
std::deque<std::pair<uint64_t, uint64_t>> g_handle_order;  // (handle, queued at), oldest first
constexpr size_t kKeepHandles = 1 << 18;
uint64_t g_handle_clock = 0;
std::atomic<uint64_t> g_next_handle{1};
void erase_handle_regions(uint64_t handle) {
    auto it = g_handles.find(handle);
    if (it == g_handles.end()) return;
    for (const auto& range : it->second.list->command_ranges) {
        auto r = g_regions.find(range.start);
        if (r != g_regions.end() && r->second.second == handle) g_regions.erase(r);
    }
}

// The safety net counted g_handle_order, which keeps an entry for every list recorded, also those already dropped by
// the overlap rule: past 2^18 lists ever recorded (2-3 minutes of play) it ran on every EndRecording and dropped any
// live list not called in the last 2^18 recordings. The world map's lists, recorded at its first visit and not called
// while Mario was in a course for 3 minutes, were gone on the way back: the map lavender with black wedges for good
// (DEBUG-2 #77, #91). Now it counts live lists (g_handles) and stale queue entries are compacted away.
// NX_HANDLES_OLD=1: the old count. NX_KEEP_HANDLES=<n>: the limit (to reproduce it quickly).
bool handles_old() {
    static const bool on = std::getenv("NX_HANDLES_OLD") != nullptr;
    return on;
}
size_t keep_handles() {
    static const size_t n = [] {
        const char* e = std::getenv("NX_KEEP_HANDLES");
        return e ? size_t(std::strtoull(e, nullptr, 0)) : kKeepHandles;
    }();
    return n;
}
double seconds_now() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// NX_CMD_DROPS=1: what dropped each list (a ring by handle), told when a dropped handle is called (explain_handle).
struct Dropped {
    uint64_t handle = 0, by = 0;  // by: the list recorded over it; 0: the safety net
    uint64_t start = 0, end = 0, pool = 0, cb = 0, calls = 0;
    uint64_t by_start = 0, by_end = 0, by_pool = 0, by_cb = 0;
    float t = 0, t_drop = 0;
};
constexpr size_t kDropRing = 1 << 19;
std::vector<Dropped> g_dropped;
std::map<uint64_t, Dropped> g_aged_lists;  // the safety net's drops, by handle
uint64_t g_aged = 0, g_aged_called = 0, g_overlap_drops = 0, g_pool_drops = 0;
bool drops_on() {
    static const bool on = std::getenv("NX_CMD_DROPS") != nullptr;
    return on;
}
void note_drop(uint64_t h, const HandleEntry& e, uint64_t by, uint64_t by_start, uint64_t by_end, uint64_t by_pool,
               uint64_t by_cb) {
    if (by == ~0ull) g_pool_drops++;
    else if (by) g_overlap_drops++;
    else g_aged++, g_aged_called += e.calls != 0;
    Dropped d{h,     by,       e.start, e.end,   e.pool, e.cb, e.calls, by_start, by_end, by_pool,
              by_cb, float(e.t), float(seconds_now())};
    if (!by) {  // few: kept by handle (the ring below is overwritten by later handles' overlap drops)
        if (g_aged_lists.size() > 100000) g_aged_lists.clear();
        g_aged_lists[h] = d;
    }
    if (!drops_on()) return;
    if (g_dropped.empty()) g_dropped.resize(kDropRing);
    g_dropped[h % kDropRing] = d;
}


void CommandBufferInitialize(CPU& c) {  // NVNboolean (NVNcommandBuffer*, NVNdevice*)
    make<CommandBuffer>(c.x[0]);
    c.x[0] = 1;
}

void CommandBufferFinalize(CPU& c) { destroy(c.x[0]); }
void SetMemoryCallback(CPU& c) {
    if (auto* cb = get<CommandBuffer>(c.x[0])) { std::lock_guard lock(cb->lock); cb->memory_callback = c.x[1]; }
}
void SetMemoryCallbackData(CPU& c) {
    if (auto* cb = get<CommandBuffer>(c.x[0])) { std::lock_guard lock(cb->lock); cb->memory_callback_data = c.x[1]; }
}

void CommandBufferBeginRecording(CPU& c) {
    if (auto* cb = get<CommandBuffer>(c.x[0])) {
        std::lock_guard lk(cb->lock);
        cb->recording = std::make_shared<CmdList>();
        cb->command_mark = cb->control_mark = 0;
    }
}

void AddCommandMemory(CPU& c) {  // (cmd, const NVNmemoryPool*, ptrdiff_t offset, size_t size)
    if (auto* cb = get<CommandBuffer>(c.x[0])) {
        std::lock_guard lk(cb->lock);
        note_command_range(*cb, false);
        cb->command_size = c.x[3];
        cb->command_base = 0;
        cb->command_mark = cb->recording->cmds.size();
        cb->command_memory = command_memory_address(c.x[1], c.x[2]);
        cb->command_pool = c.x[1];
    }
}

void AddControlMemory(CPU& c) {  // (cmd, void* memory, size_t size)
    if (auto* cb = get<CommandBuffer>(c.x[0])) {
        std::lock_guard lk(cb->lock);
        cb->control_size = c.x[2];
        cb->control_base = 0;
        cb->control_mark = cb->recording->cmds.size();
    }
}

template <bool Command, int What>  // What: 0 used, 1 size, 2 free
void memory_query(CPU& c) {
    auto* cb = get<CommandBuffer>(c.x[0]);
    if (!cb) {
        c.x[0] = 0;
        return;
    }
    std::lock_guard lk(cb->lock);
    uint64_t size = Command ? cb->command_size : cb->control_size;
    uint64_t u = used(size, Command ? cb->command_base : cb->control_base, cb->recording->cmds.size() - (Command ? cb->command_mark : cb->control_mark),
                      Command ? kCommandBytes : kControlBytes);
    c.x[0] = What == 0 ? u : What == 1 ? size : size - u;
}

std::atomic<uint64_t> g_ends{0}, g_end_cmds{0}, g_calls{0}, g_no_cb{0};
std::atomic<uint64_t> g_skin_lists{0}, g_skin_calls{0}, g_skin_replays{0};

bool skinned_program(uint64_t program) {
    if (const auto* sh = program_shaders(program))
        for (const ShaderInfo* s : *sh)
            if (s->stage == ShaderStage::Vertex)
                for (const ShaderIo& in : s->refl.inputs)
                    if (in.location == 6) return true;
    return false;
}

void CommandBufferEndRecording(CPU& c) {  // NVNcommandHandle
    std::shared_ptr<CmdList> list = std::make_shared<CmdList>();
    uint64_t start = 0, end = 0, pool = 0;
    const uint64_t cb_guest = c.x[0];
    if (auto* cb = get<CommandBuffer>(c.x[0])) {
        std::lock_guard lk(cb->lock);
        note_command_range(*cb, true);
        size_t n = cb->recording->cmds.size() + 1;  // incluye el final de lista
        start = cb->command_memory + cb->command_base;
        pool = cb->command_pool;
        cb->command_base = used(cb->command_size, cb->command_base, n - cb->command_mark, kCommandBytes);
        end = std::max(cb->command_memory + cb->command_base, start + 1);
        cb->control_base = used(cb->control_size, cb->control_base, n - cb->control_mark, kControlBytes);
        list.swap(cb->recording);
        cb->command_mark = cb->control_mark = 0;
    }
    list->epoch = current_epoch();
    g_ends++;
    g_end_cmds += list->cmds.size();
    uint64_t h = g_next_handle++;
    list->handle = h;
    if (skin_lists_on()) {
        size_t draws = 0, calls = 0;
        for (const Cmd& k : list->cmds) {
            list->skinned += k.op == Cmd::BindProgram && skinned_program(k.a[0]);
            draws += k.op == Cmd::DrawArrays || k.op == Cmd::DrawElements;
            calls += k.op == Cmd::CallList;
        }
        if (list->skinned && g_skin_lists++ < 40)
            log("skinlists: recorded handle %llx in cmd %llx: %u skinned binds, %zu draws, %zu calls, %zu cmds",
                (unsigned long long)h, (unsigned long long)c.x[0], list->skinned, draws, calls, list->cmds.size());
    }
    if ((g_ends & 4095) == 0)
        log("nvn: %llu lists ended (%llu commands), %llu CallCommands, %llu records into unknown command buffers%s",
            (unsigned long long)g_ends.load(), (unsigned long long)g_end_cmds.load(), (unsigned long long)g_calls.load(),
            (unsigned long long)g_no_cb.load(),
            skin_lists_on() ? (" ; skinned lists " + std::to_string(g_skin_lists.load()) + " recorded, " +
                               std::to_string(g_skin_calls.load()) + " calls to them recorded, " +
                               std::to_string(g_skin_replays.load()) + " replays")
                                  .c_str()
                            : "");
    static const bool trace_lists = std::getenv("NX_NVN_LISTS") != nullptr;
    if (trace_lists && g_ends > 20000 && g_ends < 20200) {
        size_t draws = 0, calls = 0;
        for (auto& k : list->cmds) {
            draws += k.op == Cmd::DrawArrays || k.op == Cmd::DrawElements;
            calls += k.op == Cmd::CallList;
        }
        log("nvn list: end %llx -> handle %llu: %zu cmds, %zu draws, %zu calls, lr %s", (unsigned long long)c.x[0],
            (unsigned long long)h, list->cmds.size(), draws, calls, describe(c.x[30]).c_str());
    }
    {
        std::lock_guard lk(g_handles_lock);
        uint64_t now = ++g_handle_clock;
        // Una lista puede ocupar varios bloques repuestos por el callback. Conservar todos sus rangos:
        // reutilizar o liberar cualquiera de ellos invalida la lista completa.
        std::vector<uint64_t> overlapping;
        for (const auto& range : list->command_ranges) {
            auto r = g_regions.lower_bound(range.start);
            if (r != g_regions.begin()) --r;
            for (; r != g_regions.end() && r->first < range.end; ++r)
                if (r->second.first > range.start) overlapping.push_back(r->second.second);
        }
        std::sort(overlapping.begin(), overlapping.end());
        overlapping.erase(std::unique(overlapping.begin(), overlapping.end()), overlapping.end());
        for (auto old_handle : overlapping) {
            auto old = g_handles.find(old_handle);
            if (old == g_handles.end()) continue;
            note_drop(old->first, old->second, h, start, end, pool, cb_guest);
            erase_handle_regions(old_handle);
            g_handles.erase(old);
        }
        for (const auto& range : list->command_ranges) g_regions[range.start] = {range.end, h};
        g_handles[h] = {list, now, 0, start, end, pool, cb_guest, seconds_now(), 0};
        g_handle_order.push_back({h, now});
        // The safety net (see handles_old): over the limit of live lists, the oldest not called since they were
        // queued go. The queue's entries of lists already dropped are compacted away when they outnumber the live.
        const bool old_rule = handles_old();
        if (!old_rule && g_handle_order.size() > 2 * g_handles.size() + 4096) {
            std::deque<std::pair<uint64_t, uint64_t>> live;
            for (auto& e : g_handle_order)
                if (auto it = g_handles.find(e.first); it != g_handles.end() && it->second.queued == e.second)
                    live.push_back(e);
            g_handle_order.swap(live);
        }
        const size_t keep = keep_handles();
        for (size_t turns = g_handle_order.size();
             (old_rule ? g_handle_order.size() : g_handles.size()) > keep && turns && !g_handle_order.empty(); turns--) {
            auto [old, queued] = g_handle_order.front();
            g_handle_order.pop_front();
            auto it = g_handles.find(old);
            if (it == g_handles.end() || it->second.queued != queued) continue;  // dropped, or queued again later
            if (it->second.used > queued || !it->second.used) {  // in use, or not called yet: queue it again
                it->second.queued = ++g_handle_clock;
                g_handle_order.push_back({old, it->second.queued});
            } else {
                erase_handle_regions(old);
                note_drop(old, it->second, 0, 0, 0, 0, 0);
                g_handles.erase(it);
            }
        }
        if ((g_ends & 4095) == 0)
            log("nvn: handles: %zu live, %zu queued, %llu dropped by overlap, %llu with their pool, %llu by the safety "
                "net (%llu of them called before)",
                g_handles.size(), g_handle_order.size(), (unsigned long long)g_overlap_drops,
                (unsigned long long)g_pool_drops, (unsigned long long)g_aged, (unsigned long long)g_aged_called);
    }
    c.x[0] = h;
}

void SetRenderTargets(CPU& c) {
    Cmd cmd{Cmd::SetRenderTargets};
    uint32_t n = std::min<uint32_t>(uint32_t(c.x[1]), 8);
    cmd.n = n;
    for (uint32_t k = 0; k < n && c.x[2]; k++) cmd.a[k] = texture_identity_address(ld<uint64_t>(c.x[2] + 8 * k));
    if (std::getenv("NX_TRACE_PRESENT")) {
        static std::atomic<uint64_t> calls{0};
        if (++calls < 200) for (uint32_t k = 0; k < n; ++k) {
            auto* t = get<Texture>(cmd.a[k]);
            log("nvn: destino %llx, %ux%u memoria %llx target %u formato %u muestras %u, lr %s", (unsigned long long)cmd.a[k],
                t ? t->width : 0, t ? t->height : 0, (unsigned long long)(t ? t->memory : 0),
                t ? t->target : 0, t ? t->format : 0, t ? t->samples : 0, describe(c.x[30]).c_str());
        }
    }
    cmd.a[8] = texture_identity_address(c.x[4]);  // depth/stencil
    // The views pick the level and layer rendered to (a bloom chain renders each mip of one texture in turn).
    TargetViews tv;
    for (uint32_t k = 0; k < n && c.x[3]; k++) {
        ViewInfo v = read_view(c, ld<uint64_t>(c.x[3] + 8 * k));
        tv.level[k] = uint8_t(v.base_level), tv.layer[k] = uint16_t(v.min_layer), tv.format[k] = uint16_t(v.format);
    }
    ViewInfo dv = read_view(c, c.x[5]);
    tv.level[8] = uint8_t(dv.base_level), tv.layer[8] = uint16_t(dv.min_layer), tv.format[8] = uint16_t(dv.format);
    record(c.x[0], cmd, &tv, sizeof tv);
}

void ClearColor(CPU& c) {
    Cmd cmd{Cmd::ClearColor};
    cmd.n = uint32_t(c.x[1]);
    if (c.x[2]) std::memcpy(cmd.f, reinterpret_cast<const void*>(c.x[2]), 16);
    cmd.mask = uint32_t(c.x[3]);
    record(c.x[0], cmd);
}

void ClearDepthStencil(CPU& c) {
    Cmd cmd{Cmd::ClearDepthStencil};
    cmd.f[0] = c.v[0].f32[0];
    cmd.mask = (c.x[1] & 1) | ((uint32_t(c.x[3]) & 0xFF) << 8);  // depth mask, stencil mask
    cmd.i[0] = int32_t(c.x[2]);
    record(c.x[0], cmd);
}

void SetViewport(CPU& c) {
    Cmd cmd{Cmd::SetViewport};
    for (int k = 0; k < 4; k++) cmd.i[k] = int32_t(c.x[1 + k]);
    record(c.x[0], cmd);
    static std::atomic<int> logged{0};
    if ((cmd.i[3] < 0 || cmd.i[2] < 0) && logged++ < 10)
        log("nvn: SetViewport with a negative size: %d %d %d %d", cmd.i[0], cmd.i[1], cmd.i[2], cmd.i[3]);
}

void SetScissor(CPU& c) {
    Cmd cmd{Cmd::SetScissor};
    for (int k = 0; k < 4; k++) cmd.i[k] = int32_t(c.x[1 + k]);
    static std::atomic<int> trace_count{0};
    if (std::getenv("NX_TRACE_PRESENT") && trace_count++ < 32)
        log("nvn: scissor %d,%d %dx%d caller=%s", cmd.i[0], cmd.i[1], cmd.i[2], cmd.i[3], describe(c.x[30]).c_str());
    record(c.x[0], cmd);
}

void CallCommands(CPU& c) {
    uint32_t n = uint32_t(c.x[1]);
    g_calls += n;
    for (uint32_t k = 0; k < n; k++) {
        Cmd cmd{Cmd::CallList};
        cmd.a[0] = ld<uint64_t>(c.x[2] + 8 * k);
        record(c.x[0], cmd);
        if (skin_lists_on())
            if (auto l = peek_list(cmd.a[0]); l && l->skinned && g_skin_calls++ < 40)
                log("skinlists: handle %llx called from cmd %llx", (unsigned long long)cmd.a[0],
                    (unsigned long long)c.x[0]);
    }
}

}  // namespace

void command_set_copy_stride(uint64_t guest, bool image, uint64_t stride) {
    if (auto* cb = get<CommandBuffer>(guest)) {
        std::lock_guard lock(cb->lock);
        (image ? cb->copy_image_stride : cb->copy_row_stride) = stride;
    }
}
std::pair<uint64_t, uint64_t> command_copy_strides(uint64_t guest) {
    if (auto* cb = get<CommandBuffer>(guest)) {
        std::lock_guard lock(cb->lock);
        return {cb->copy_row_stride, cb->copy_image_stride};
    }
    return {};
}

void record(uint64_t guest, Cmd cmd, const void* blob, size_t size) {
    auto* cb = get<CommandBuffer>(guest);
    if (!cb) {
        g_no_cb++;
        return;
    }
    std::unique_lock lk(cb->lock);
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        size_t count = cb->recording->cmds.size();
        bool command = used(cb->command_size, cb->command_base, count - cb->command_mark + 2, kCommandBytes) + 64 >= cb->command_size;
        bool control = used(cb->control_size, cb->control_base, count - cb->control_mark + 2, kControlBytes) + 64 >= cb->control_size;
        if (!command && !control) break;
        auto fn = cb->memory_callback, data = cb->memory_callback_data;
        if (!fn || attempt == 2) fatal("nvn: memoria de grabación agotada sin reposición del callback");
        const auto expected_recording = cb->recording;
        lk.unlock();
        guest_call(current_cpu(), fn, {guest, command ? 0ull : 1ull, command ? 128ull : 80ull, data});
        if (get<CommandBuffer>(guest) != cb) fatal("nvn: el callback finalizó o reemplazó su command buffer");
        lk.lock();
        if (cb->recording != expected_recording) fatal("nvn: el callback sustituyó la lista que agotó memoria");
    }
    if (blob) {
        cmd.blob = uint32_t(cb->recording->blobs.size());
        auto* p = static_cast<const uint8_t*>(blob);
        cb->recording->blobs.emplace_back(p, p + size);
    }
    cb->recording->cmds.push_back(cmd);
}

// A handle's list for the traces, without marking it used (which would keep it from being dropped).
std::shared_ptr<CmdList> peek_list(uint64_t handle) {
    std::lock_guard lk(g_handles_lock);
    auto it = g_handles.find(handle);
    return it == g_handles.end() ? nullptr : it->second.list;
}

bool skin_lists_on() {
    static const bool on = std::getenv("NX_TRACE_SKINLISTS") != nullptr;
    return on;
}

void skin_list_replayed(const CmdList& list, int depth) {
    if (list.skinned && g_skin_replays++ < 40)
        log("skinlists: handle %llx replayed at depth %d", (unsigned long long)list.handle, depth);
    // The first skinned lists' commands: P<vs> program binds (* skinned), D draws, C<handle> calls, op numbers else.
    static int dumped = 0;
    if (!list.skinned || dumped >= 3) return;
    dumped++;
    std::string s;
    for (const Cmd& k : list.cmds) {
        char b[64];
        if (k.op == Cmd::BindProgram) {
            const auto* sh = program_shaders(k.a[0]);
            const ShaderInfo* vs = nullptr;
            for (const ShaderInfo* x : sh ? *sh : std::vector<const ShaderInfo*>{})
                if (x->stage == ShaderStage::Vertex) vs = x;
            std::snprintf(b, sizeof b, " P%s%.8s/%x", skinned_program(k.a[0]) ? "*" : "", vs ? vs->hash.c_str() : "-",
                          k.mask);
        } else if (k.op == Cmd::DrawArrays || k.op == Cmd::DrawElements) {
            std::snprintf(b, sizeof b, " D%u", uint32_t(k.op == Cmd::DrawElements ? k.i[0] : k.i[1]));
        } else if (k.op == Cmd::CallList) {
            auto l = peek_list(k.a[0]);
            size_t draws = 0, progs = 0;
            for (const Cmd& j : l ? l->cmds : std::vector<Cmd>{})
                draws += j.op == Cmd::DrawArrays || j.op == Cmd::DrawElements, progs += j.op == Cmd::BindProgram;
            if (l)
                std::snprintf(b, sizeof b, " C%llx(%zuc%zud%zup)", (unsigned long long)k.a[0], l->cmds.size(),
                              draws, progs);
            else
                std::snprintf(b, sizeof b, " C%llx(GONE)", (unsigned long long)k.a[0]);
        } else {
            std::snprintf(b, sizeof b, " %u", unsigned(k.op));
        }
        s += b;
        if (s.size() > 3000) {
            log("skinlists: %llx:%s", (unsigned long long)list.handle, s.c_str());
            s.clear();
        }
    }
    log("skinlists: %llx:%s", (unsigned long long)list.handle, s.c_str());
}

std::shared_ptr<CmdList> command_list(uint64_t handle) {
    std::lock_guard lk(g_handles_lock);
    auto it = g_handles.find(handle);
    if (it == g_handles.end()) return nullptr;
    it->second.used = ++g_handle_clock;
    it->second.calls++;
    return it->second.list;
}

// A memory pool finalized: the lists recorded in its memory are gone with it (on the GPU they are invalid). GPU
// addresses are never reused (gpu_alloc), so without this nothing records over them again and every course load's
// lists stayed live for the session (+3,000-7,000 per course entry), until the safety net had to run again
// (DEBUG-2 #91). NX_CMD_POOL_DROP=0: they stay.
void command_memory_released(uint64_t gpu, uint64_t size) {
    static const bool off = [] {
        const char* e = std::getenv("NX_CMD_POOL_DROP");
        return e && e[0] == '0';
    }();
    if (off || !gpu) return;
    std::lock_guard lk(g_handles_lock);
    size_t n = 0;
    std::vector<uint64_t> released;
    for (auto r = g_regions.lower_bound(gpu); r != g_regions.end() && r->first - gpu < size; ++r)
        released.push_back(r->second.second);
    std::sort(released.begin(), released.end());
    released.erase(std::unique(released.begin(), released.end()), released.end());
    for (auto handle : released) {
        auto it = g_handles.find(handle);
        if (it == g_handles.end()) continue;
        note_drop(it->first, it->second, ~0ull, gpu, gpu + size, 0, 0);
        erase_handle_regions(handle);
        g_handles.erase(it); n++;
    }
    static int logged = 0;
    if (n && logged++ < 20)
        log("nvn: memory pool at gpu %llx (%llu KB) finalized: %zu command lists in it dropped", (unsigned long long)gpu,
            (unsigned long long)(size >> 10), n);
}

// For the "handle ... is unknown" lines: what dropped it (NX_CMD_DROPS=1), or why that is not known.
std::string explain_handle(uint64_t handle) {
    std::lock_guard lk(g_handles_lock);
    char b[400];
    if (handle >= g_next_handle.load()) return "never recorded";
    auto aged = g_aged_lists.find(handle);
    if (aged == g_aged_lists.end() &&
        (!drops_on() || g_dropped.empty() || g_dropped[handle % kDropRing].handle != handle))
        return drops_on() ? "dropped by overlap (not in the ring any more)" : "dropped (NX_CMD_DROPS=1 tells by what)";
    const Dropped& d = aged != g_aged_lists.end() ? aged->second : g_dropped[handle % kDropRing];
    if (d.by == ~0ull)
        std::snprintf(b, sizeof b, "its memory pool (gpu %llx-%llx) was finalized at %.1f s (recorded at %.1f s in cmd "
                      "%llx, %llu calls)", (unsigned long long)d.by_start, (unsigned long long)d.by_end, d.t_drop, d.t,
                      (unsigned long long)d.cb, (unsigned long long)d.calls);
    else if (!d.by)
        std::snprintf(b, sizeof b,
                      "aged out by the safety net at %.1f s (recorded at %.1f s in cmd %llx, %llu calls, memory %llx-%llx "
                      "pool %llx)",
                      d.t_drop, d.t, (unsigned long long)d.cb, (unsigned long long)d.calls, (unsigned long long)d.start,
                      (unsigned long long)d.end, (unsigned long long)d.pool);
    else
        std::snprintf(b, sizeof b,
                      "dropped at %.1f s by list %llx (cmd %llx, memory %llx-%llx pool %llx); it was recorded at %.1f s "
                      "in cmd %llx, %llu calls, memory %llx-%llx pool %llx",
                      d.t_drop, (unsigned long long)d.by, (unsigned long long)d.by_cb, (unsigned long long)d.by_start,
                      (unsigned long long)d.by_end, (unsigned long long)d.by_pool, d.t, (unsigned long long)d.cb,
                      (unsigned long long)d.calls, (unsigned long long)d.start, (unsigned long long)d.end,
                      (unsigned long long)d.pool);
    return b;
}

NX_HLE("nvnCommandBufferInitialize", CommandBufferInitialize);
NX_HLE("nvnCommandBufferFinalize", CommandBufferFinalize);
NX_HLE("nvnCommandBufferSetMemoryCallback", SetMemoryCallback);
NX_HLE("nvnCommandBufferSetMemoryCallbackData", SetMemoryCallbackData);
NX_HLE("nvnCommandBufferBeginRecording", CommandBufferBeginRecording);
NX_HLE("nvnCommandBufferEndRecording", CommandBufferEndRecording);
NX_HLE("nvnCommandBufferSetRenderTargets", SetRenderTargets);
NX_HLE("nvnCommandBufferClearColor", ClearColor);
NX_HLE("nvnCommandBufferClearDepthStencil", ClearDepthStencil);
NX_HLE("nvnCommandBufferSetViewport", SetViewport);
NX_HLE("nvnCommandBufferSetScissor", SetScissor);
NX_HLE("nvnCommandBufferCallCommands", CallCommands);
NX_HLE("nvnCommandBufferAddCommandMemory", AddCommandMemory);
NX_HLE("nvnCommandBufferAddControlMemory", AddControlMemory);
NX_HLE("nvnCommandBufferGetCommandMemoryUsed", (memory_query<true, 0>));
NX_HLE("nvnCommandBufferGetCommandMemorySize", (memory_query<true, 1>));
NX_HLE("nvnCommandBufferGetCommandMemoryFree", (memory_query<true, 2>));
NX_HLE("nvnCommandBufferGetControlMemoryUsed", (memory_query<false, 0>));
NX_HLE("nvnCommandBufferGetControlMemorySize", (memory_query<false, 1>));
NX_HLE("nvnCommandBufferGetControlMemoryFree", (memory_query<false, 2>));

}  // namespace nx::nvn
