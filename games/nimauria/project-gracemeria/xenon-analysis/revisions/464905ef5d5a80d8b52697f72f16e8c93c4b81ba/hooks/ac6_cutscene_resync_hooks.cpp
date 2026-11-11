// AC6 cutscene audio/video resync.
//
// The in-engine cutscene sequencers - CAce6DemoManager::Exec ("DD",
// 0x82184460) and CX360DemoManagerEM::Exec ("EM", 0x821856F8, the in-mission
// story cutscenes) - advance their timeline one 30 fps tick per rendered
// frame. The soundtrack runs on the audio clock, which keeps time through
// render hitches: XAudioGetRenderDriverTic counts consumed samples, underrun
// silence included. One long frame puts the picture permanently behind the
// sound, and a sustained slow render turns the cutscene into slow motion
// against its soundtrack. The 360 held 30 fps; a PC host may not.
//
// Audio stays the master clock and is never touched. Per Exec call the
// deficit is the ticks the audio clock says should have run minus the ticks
// issued; while it is at least 2 (steady-state jitter is +/-1, so normal
// 30 fps playback never triggers), Exec runs again from the same inputs, up
// to kMaxExtraTicks extra times per frame - a bounded timeline frame-skip.
//
// Each hook sits at its Exec's first instruction with action "return": it
// runs Exec itself (the hook firing again inside that call sees the
// per-thread guard and lets the original body run), adds the catch-up calls,
// and returns to Exec's caller.
//
// Mechanism and sites are from sal063/AC6_recomp (BSD-3-Clause,
// src/ac6_backend_fixes/ac6_cutscene_resync.cpp), verified against this
// executable's disassembly; this implementation is Gracemeria's own, written
// against Xenon's instruction-hook ABI. Enabled by analysis/hooks.json.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>

#include "xenon/cpu/runtime.hpp"

namespace gracemeria::ac6 {
// ac6_guest_call.cpp
bool call_guest_function(xenon::cpu::ExecutionContext& context, xenon::cpu::GuestAddress target,
                         const char* site);
}  // namespace gracemeria::ac6

namespace {

using xenon::cpu::CpuState;
using xenon::cpu::ExecutionContext;
using xenon::cpu::GuestAddress;

constexpr GuestAddress kExecDD = 0x82184460u;
constexpr GuestAddress kExecEM = 0x821856F8u;
// xboxkrnl XAudioGetRenderDriverTic: consumed 48 kHz samples.
constexpr std::uint32_t kRenderDriverTicOrdinal = 0x34C;
constexpr std::uint64_t kSamplesPerTick = 48000u / 30u;
// A gap this long between Exec calls ends the cutscene session (menus or
// gameplay in between); a real mid-cutscene stall is shorter and keeps it.
constexpr auto kSessionReset = std::chrono::milliseconds(1500);
// Extra ticks per frame: holds sync down to 30 / (1 + 3) = 7.5 fps.
constexpr std::int64_t kMaxExtraTicks = 3;

struct Session {
  GuestAddress site{};
  std::chrono::steady_clock::time_point last_exec{};
  bool clock_valid{};
  std::uint32_t baseline{};
  std::uint64_t ticks_issued{};
};

// Exec runs only on the guest game thread, so the session needs no lock.
Session g_session;
thread_local bool t_inside_exec = false;

// The guest-visible audio clock, read through the same export the title
// calls. False until audio has consumed its first samples.
[[nodiscard]] bool read_audio_clock(ExecutionContext& context, std::uint32_t& samples) {
  CpuState scratch = context.state;
  if (!context.runtime.external_call("xboxkrnl.exe", kRenderDriverTicOrdinal, scratch, context.memory))
    return false;
  samples = static_cast<std::uint32_t>(scratch.gpr[3]);
  return samples != 0u;
}

bool exec_with_resync(ExecutionContext& context, GuestAddress exec, const char* site) {
  // The hook fires again when Exec itself is called below: run the body.
  if (t_inside_exec) return false;

  const auto now = std::chrono::steady_clock::now();
  if (g_session.site != exec || g_session.last_exec == std::chrono::steady_clock::time_point{} ||
      now - g_session.last_exec > kSessionReset)
    g_session = Session{exec};
  g_session.last_exec = now;

  std::uint32_t samples = 0;
  const bool clock_ok = read_audio_clock(context, samples);
  if (clock_ok && !g_session.clock_valid) {
    // This frame's own tick is "now" on the audio timeline.
    g_session.clock_valid = true;
    g_session.baseline = samples;
    g_session.ticks_issued = 0;
  }

  // The catch-up ticks start from Exec's own inputs, not the registers the
  // first call left behind.
  const CpuState inputs = context.state;
  // Cleared however the calls end, including a thread termination unwinding
  // through them.
  struct InsideExec {
    InsideExec() noexcept { t_inside_exec = true; }
    ~InsideExec() { t_inside_exec = false; }
  } inside_exec;
  bool ok = gracemeria::ac6::call_guest_function(context, exec, site);
  ++g_session.ticks_issued;

  if (ok && clock_ok && g_session.clock_valid) {
    // Wrapping subtraction: the tic is a 32-bit counter.
    const auto expected = static_cast<std::uint64_t>(samples - g_session.baseline) / kSamplesPerTick;
    const auto deficit = static_cast<std::int64_t>(expected) - static_cast<std::int64_t>(g_session.ticks_issued);
    if (deficit >= 2) {
      const auto extras = std::min(deficit, kMaxExtraTicks);
      for (std::int64_t i = 0; i < extras && ok; ++i) {
        context.state = inputs;
        ok = gracemeria::ac6::call_guest_function(context, exec, site);
        ++g_session.ticks_issued;
      }
      static bool reported = false;
      if (!reported) {
        reported = true;
        std::fprintf(stderr,
                     "[Gracemeria] cutscene-resync %s: timeline %lld tick(s) behind its audio, ran %lld "
                     "catch-up tick(s)\n",
                     site, static_cast<long long>(deficit), static_cast<long long>(extras));
      }
    }
  }
  return true;
}

}  // namespace

// CAce6DemoManager::Exec entry.
bool ac6CutsceneResyncHookDD(ExecutionContext& context) {
  return exec_with_resync(context, kExecDD, "cutscene-resync DD");
}

// CX360DemoManagerEM::Exec entry.
bool ac6CutsceneResyncHookEM(ExecutionContext& context) {
  return exec_with_resync(context, kExecEM, "cutscene-resync EM");
}
