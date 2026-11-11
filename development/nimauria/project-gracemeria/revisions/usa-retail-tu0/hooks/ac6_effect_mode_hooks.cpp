// AC6 effect job mode race - the one-frame effects/clouds flicker.
//
// The effect UPDATE and the effect DRAW are two jobs on the game's second
// worker pool. Both enter the effect manager through the same vtable slot,
// vtbl[+0x04] = 0x822B3248, which tells them apart only by a plain mode word
// at [manager + 0xA1E84]:
//
//     mode 0  -> f1 = dt, 0x822B2E20   update
//     mode 1  -> f1 = dt, 0x822B0848
//     mode 2  -> vtbl[+0x100]          draw
//     mode 3+ -> return
//
// The submitter writes the mode and then submits through JobPoolB::Submit
// (0x821D4A78); the worker reads it when it runs the job. On the 360 the two
// jobs never overlapped, but on a PC host a second submit can overwrite the
// word before the first job reads it. Both jobs then read mode 2: the update
// never runs and the effects and clouds are drawn twice for a frame.
//
// The fix keeps the mode each job was submitted with. The hook at Submit's
// entry records (job, arg) -> mode for every effect-manager job; the hook at
// the dispatcher's entry runs the recorded mode's target itself and returns.
// The live word is never read on that path, so a racing write cannot change
// what a job does. A dispatch with no record, or a draw slot neither manager
// class has, runs the original dispatcher and is counted.
//
// Mechanism, addresses and the (job, arg) keying are from sal063/AC6_recomp
// (BSD-3-Clause, src/ac6_backend_fixes/ac6_effect_mode_fix.cpp), verified
// against this executable's disassembly; this implementation is Gracemeria's
// own, written against Xenon's instruction-hook ABI. Enabled by
// analysis/hooks.json.

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <mutex>

#include "xenon/cpu/runtime.hpp"

namespace gracemeria::ac6 {
// ac6_guest_call.cpp
bool call_guest_function(xenon::cpu::ExecutionContext& context, xenon::cpu::GuestAddress target,
                         const char* site);
}  // namespace gracemeria::ac6

namespace {

using xenon::cpu::ExecutionContext;
using xenon::cpu::GuestAddress;

constexpr std::uint32_t kModeOffset = 0xA1E84;
constexpr GuestAddress kDispatcher = 0x822B3248u;
constexpr GuestAddress kUpdate = 0x822B2E20u;      // mode 0
constexpr GuestAddress kModeOneStep = 0x822B0848u;  // mode 1
// The two manager classes share the dispatcher but not the draw.
constexpr GuestAddress kDrawX360 = 0x820B25D0u;  // CX360EffectManager
constexpr GuestAddress kDrawAce6 = 0x822B0AD8u;  // CAce6EffectManager

[[nodiscard]] bool in_image(GuestAddress address) noexcept {
  return address >= 0x82000000u && address < 0x82A00000u;
}

struct Submission {
  std::uint32_t job{};
  std::uint32_t arg{};
  std::uint32_t mode{};
  bool pending{};
};

// Pool jobs are dispatched from per-thread queues, so dispatch order is not
// submit order: a dispatch claims the newest pending record with its own
// (job, arg), never the next one in line.
struct SubmissionTable {
  std::mutex mutex;
  std::array<Submission, 16> entries{};
  std::size_t next{};
};

SubmissionTable g_table;
std::atomic<std::uint64_t> g_corrections{};
std::atomic<std::uint64_t> g_unmatched{};
std::atomic<std::uint64_t> g_dropped{};
std::atomic<std::uint64_t> g_key_conflicts{};
std::atomic<std::uint64_t> g_unknown_draws{};
std::atomic<bool> g_reported_active{};

// A job is an effect manager iff its vtable's +4 slot is the dispatcher.
[[nodiscard]] bool is_effect_manager(ExecutionContext& context, GuestAddress job) {
  if (job < 0x82000000u || job >= 0xC0000000u) return false;
  const auto vtable = context.memory.read32_be(job);
  return in_image(vtable) && context.memory.read32_be(vtable + 4u) == kDispatcher;
}

}  // namespace

// JobPoolB::Submit(tag, thread, job, arg) entry: r5 = job, r6 = arg.
void ac6EffectModeSubmitHook(ExecutionContext& context) {
  const auto job = static_cast<GuestAddress>(context.state.gpr[5]);
  if (!is_effect_manager(context, job)) return;
  const auto arg = static_cast<std::uint32_t>(context.state.gpr[6]);
  const auto mode = context.memory.read32_be(job + kModeOffset);
  std::lock_guard lock(g_table.mutex);
  for (const auto& other : g_table.entries) {
    if (!other.pending || other.job != job || other.arg != arg || other.mode == mode) continue;
    // (job, arg) must identify a pending submission; two with different
    // modes would let a dispatch take the wrong one.
    if (g_key_conflicts.fetch_add(1u, std::memory_order_relaxed) < 8u)
      std::fprintf(stderr,
                   "[Gracemeria] effect-mode: job 0x%08X arg 0x%08X submitted with mode %u while "
                   "mode %u is still pending\n",
                   job, arg, mode, other.mode);
  }
  auto& slot = g_table.entries[g_table.next];
  if (slot.pending) g_dropped.fetch_add(1u, std::memory_order_relaxed);
  slot = {job, arg, mode, true};
  g_table.next = (g_table.next + 1u) % g_table.entries.size();
}

// Dispatcher entry: r3 = manager, r4 = arg (dt's float bits). Returns true
// when it ran the job itself and the dispatcher must return to its caller.
bool ac6EffectModeDispatchHook(ExecutionContext& context) {
  const auto self = static_cast<GuestAddress>(context.state.gpr[3]);
  const auto arg = static_cast<std::uint32_t>(context.state.gpr[4]);
  std::uint32_t mode = 0;
  bool found = false;
  {
    std::lock_guard lock(g_table.mutex);
    const auto size = g_table.entries.size();
    for (std::size_t i = 0; i < size && !found; ++i) {
      auto& slot = g_table.entries[(g_table.next + size - 1u - i) % size];
      if (!slot.pending || slot.job != self || slot.arg != arg) continue;
      mode = slot.mode;
      slot.pending = false;
      found = true;
    }
  }
  if (!found) {
    g_unmatched.fetch_add(1u, std::memory_order_relaxed);
    return false;
  }
  const auto live = context.memory.read32_be(self + kModeOffset);
  if (live != mode) {
    const auto corrections = g_corrections.fetch_add(1u, std::memory_order_relaxed) + 1u;
    if (corrections <= 8u || (corrections & 0xFFu) == 0u)
      std::fprintf(stderr,
                   "[Gracemeria] effect-mode: mode word reads %u but job 0x%08X was submitted with %u "
                   "(%llu corrected, %llu unmatched, %llu dropped)\n",
                   live, self, mode, static_cast<unsigned long long>(corrections),
                   static_cast<unsigned long long>(g_unmatched.load(std::memory_order_relaxed)),
                   static_cast<unsigned long long>(g_dropped.load(std::memory_order_relaxed)));
  }
  if (!g_reported_active.exchange(true, std::memory_order_relaxed))
    std::fprintf(stderr, "[Gracemeria] effect-mode: dispatching effect jobs from their submitted mode\n");

  if (mode >= 3u) return true;  // the original returns without dispatching
  if (mode == 2u) {
    const auto vtable = context.memory.read32_be(self);
    const auto draw = in_image(vtable) ? context.memory.read32_be(vtable + 0x100u) : 0u;
    if (draw != kDrawX360 && draw != kDrawAce6) {
      if (g_unknown_draws.fetch_add(1u, std::memory_order_relaxed) < 8u)
        std::fprintf(stderr,
                     "[Gracemeria] effect-mode: manager 0x%08X draw slot 0x%08X is not a known draw; "
                     "running the original dispatcher\n",
                     self, draw);
      return false;
    }
    // r3 = manager and r4 = arg are already the draw's arguments.
    gracemeria::ac6::call_guest_function(context, draw, "effect-mode draw");
    return true;
  }
  // The dispatcher passes dt as f1: the single-precision bits it was given
  // in r4, loaded with lfs.
  context.state.set_fpr(1, static_cast<double>(std::bit_cast<float>(arg)));
  gracemeria::ac6::call_guest_function(context, mode == 0u ? kUpdate : kModeOneStep,
                                       "effect-mode update");
  return true;
}
