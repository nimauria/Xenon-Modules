// AC6 storage job submit order - the retail title update's fix, applied to
// the base executable as Xenon instruction hooks.
//
// The save/storage layer (save.dat, replay.dat, the DLC content cache and
// CResourceLoaderDpl reads) queues jobs on the game's second worker pool via
// JobPoolB::Submit (0x821D4A78). At twelve call sites the base executable
// stores the job's operation code to [job+4] only after Submit returns:
//
//     mr   r5, r31          ; job
//     bl   JobPoolB::Submit ; a pool-B worker may run the job from here on
//     li   r10, <op>
//     stw  r10, 4(r31)      ; operation code - too late
//
// Job objects are reused, so a worker that dequeues the job first runs the
// previous operation. The retail title update moves the store before the
// call at all twelve sites. Each hook below runs before that `bl` and stores
// the site's operation code through r5 (Submit's job argument); the original
// store after the call writes the same value again.
//
// Sites and operation codes are from the offline title-update diff in
// sal063/AC6_recomp (BSD-3-Clause, src/ac6_backend_fixes/
// ac6_storage_submit_order_fix.cpp); this implementation is Gracemeria's own,
// written against Xenon's instruction-hook ABI. Enabled by
// analysis/hooks.json.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

#include "xenon/cpu/runtime.hpp"

namespace {

using xenon::cpu::ExecutionContext;
using xenon::cpu::GuestAddress;

constexpr std::array<GuestAddress, 12> kSites = {
    0x821D3C8Cu, 0x821D3D0Cu, 0x821D3DC8u, 0x821D3E58u, 0x821D3F70u, 0x821D4030u,
    0x821D40F0u, 0x821C7538u, 0x821C8EC4u, 0x821C8F78u, 0x821C9298u, 0x821C5A78u,
};
std::array<std::atomic<bool>, kSites.size()> g_reported{};

void store_operation_early(ExecutionContext& context, std::size_t site, std::uint32_t operation) {
  const auto job = static_cast<GuestAddress>(context.state.gpr[5]);
  if (job == 0u) {
    // r5 is a live job at every site; a null one means this executable is
    // not the one these sites were taken from.
    if (!g_reported[site].exchange(true, std::memory_order_relaxed))
      std::fprintf(stderr, "[Gracemeria] storage-order hook 0x%08X: null job, store skipped\n",
                   kSites[site]);
    return;
  }
  context.memory.write32_be(job + 4u, operation);
  if (!g_reported[site].exchange(true, std::memory_order_relaxed))
    std::fprintf(stderr, "[Gracemeria] storage-order hook 0x%08X: job 0x%08X op 0x%X stored before submit\n",
                 kSites[site], job, operation);
}

}  // namespace

void ac6StorageOrderHookOp2(ExecutionContext& context) { store_operation_early(context, 0, 0x2u); }
void ac6StorageOrderHookOp3(ExecutionContext& context) { store_operation_early(context, 1, 0x3u); }
void ac6StorageOrderHookOp6(ExecutionContext& context) { store_operation_early(context, 2, 0x6u); }
void ac6StorageOrderHookOp7(ExecutionContext& context) { store_operation_early(context, 3, 0x7u); }
void ac6StorageOrderHookOpA(ExecutionContext& context) { store_operation_early(context, 4, 0xAu); }
void ac6StorageOrderHookOpB(ExecutionContext& context) { store_operation_early(context, 5, 0xBu); }
void ac6StorageOrderHookOpC(ExecutionContext& context) { store_operation_early(context, 6, 0xCu); }
void ac6StorageOrderHookOpD(ExecutionContext& context) { store_operation_early(context, 7, 0xDu); }
void ac6StorageOrderHookOp8(ExecutionContext& context) { store_operation_early(context, 8, 0x8u); }
void ac6StorageOrderHookOp9(ExecutionContext& context) { store_operation_early(context, 9, 0x9u); }
void ac6StorageOrderHookOp10(ExecutionContext& context) { store_operation_early(context, 10, 0x10u); }

// 0x821C5A78: the caller computes the operation code into r29 (nonvolatile,
// live across the call; the original post-call store is `stw r29,4(r31)`).
void ac6StorageOrderHookVar(ExecutionContext& context) {
  store_operation_early(context, 11, static_cast<std::uint32_t>(context.state.gpr[29]));
}
