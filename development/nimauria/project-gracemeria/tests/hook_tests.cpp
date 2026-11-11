// Behaviour of the US revision's host-timing hooks, run against a FlatMemory
// guest whose "compiled functions" are recorders standing in for AC6's code.

#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "xenon/cpu/flat_memory.hpp"
#include "xenon/cpu/runtime.hpp"

using xenon::cpu::CompiledLookupKind;
using xenon::cpu::CpuState;
using xenon::cpu::ExecutionContext;
using xenon::cpu::ExecutionResult;
using xenon::cpu::FlowReason;
using xenon::cpu::GuestAddress;
using xenon::cpu::MemoryPort;
using xenon::cpu::NativeCompiledEntry;

// revisions/usa-retail-tu0/hooks/
void ac6EffectModeSubmitHook(ExecutionContext& context);
bool ac6EffectModeDispatchHook(ExecutionContext& context);
bool ac6CutsceneResyncHookDD(ExecutionContext& context);
bool ac6CutsceneResyncHookEM(ExecutionContext& context);

namespace {

constexpr GuestAddress kDrawX360 = 0x820B25D0u;
constexpr GuestAddress kDrawAce6 = 0x822B0AD8u;
constexpr GuestAddress kUpdate = 0x822B2E20u;
constexpr GuestAddress kModeOneStep = 0x822B0848u;
constexpr GuestAddress kDispatcher = 0x822B3248u;
constexpr GuestAddress kExecDD = 0x82184460u;
constexpr GuestAddress kExecEM = 0x821856F8u;
constexpr GuestAddress kCallerReturn = 0x82400010u;

struct Call {
  GuestAddress target{};
  std::uint64_t r3{};
  std::uint64_t r4{};
  double f1{};
};
std::vector<Call> g_calls;
std::uint32_t g_audio_tic = 0;

// A guest function: records its inputs, clobbers volatile registers as real
// code does, and returns to LR.
template <GuestAddress Target>
ExecutionResult recorded_function(ExecutionContext& context) {
  g_calls.push_back({Target, context.state.gpr[3], context.state.gpr[4], context.state.fpr(1)});
  context.state.gpr[3] = 0xDEADBEEFu;
  context.state.gpr[4] = 0xDEADBEEFu;
  const auto lr = static_cast<GuestAddress>(context.state.lr & ~3ull);
  return {FlowReason::Return, lr, 0};
}

// Exec as generated code runs it: the entry hook fires first and, inside the
// hook's own call, must let the body run.
template <GuestAddress Target, bool (*Hook)(ExecutionContext&)>
ExecutionResult hooked_exec(ExecutionContext& context) {
  assert(!Hook(context));
  return recorded_function<Target>(context);
}

NativeCompiledEntry lookup(void*, ExecutionContext&, GuestAddress target, CompiledLookupKind) {
  switch (target) {
    case kDrawX360: return &recorded_function<kDrawX360>;
    case kDrawAce6: return &recorded_function<kDrawAce6>;
    case kUpdate: return &recorded_function<kUpdate>;
    case kModeOneStep: return &recorded_function<kModeOneStep>;
    case kExecDD: return &hooked_exec<kExecDD, ac6CutsceneResyncHookDD>;
    case kExecEM: return &hooked_exec<kExecEM, ac6CutsceneResyncHookEM>;
    default: return nullptr;
  }
}

// XAudioGetRenderDriverTic is the only export the hooks call.
class TestRuntime final : public xenon::cpu::RuntimeServices {
 public:
  ExecutionResult call(GuestAddress target, CpuState&, MemoryPort&) override {
    return {FlowReason::Branch, target, 0};
  }
  ExecutionResult syscall(std::uint32_t level, CpuState& state, MemoryPort&) override {
    return {FlowReason::Syscall, state.cia + 4u, level};
  }
  ExecutionResult trap(std::uint32_t code, CpuState& state, MemoryPort&) override {
    return {FlowReason::Trap, state.cia, code};
  }
  std::uint64_t read_spr(std::uint32_t, const CpuState&) override { return 0; }
  void write_spr(std::uint32_t, std::uint64_t, CpuState&) override {}
  std::uint64_t read_time_base(const CpuState& state) override { return state.time_base; }
  bool external_call(std::string_view module, std::uint32_t ordinal, CpuState& state, MemoryPort&) override {
    if (module != "xboxkrnl.exe" || ordinal != 0x34Cu) return false;
    state.gpr[3] = g_audio_tic;
    return true;
  }
};

struct Guest {
  xenon::cpu::FlatMemory memory{0x01000000u, 0x82000000u};
  TestRuntime runtime;
  CpuState state{};
  ExecutionContext context{state, memory, runtime};
  Guest() {
    context.compiled_lookup = &lookup;
    state.lr = kCallerReturn;
  }
};

// An effect manager: its vtable's +4 slot is the dispatcher and its +0x100
// slot the class's draw.
constexpr GuestAddress kVtableX360 = 0x82010000u;
constexpr GuestAddress kVtableAce6 = 0x82011000u;
constexpr GuestAddress kVtableUnknownDraw = 0x82012000u;
constexpr GuestAddress kManager = 0x82800000u;
constexpr GuestAddress kModeWord = kManager + 0xA1E84u;

void make_manager(Guest& guest, GuestAddress manager, GuestAddress vtable, GuestAddress draw) {
  guest.memory.write32_be(vtable + 4u, kDispatcher);
  guest.memory.write32_be(vtable + 0x100u, draw);
  guest.memory.write32_be(manager, vtable);
}

void submit(Guest& guest, GuestAddress job, std::uint32_t arg, std::uint32_t mode) {
  guest.memory.write32_be(job + 0xA1E84u, mode);
  guest.state.gpr[5] = job;
  guest.state.gpr[6] = arg;
  ac6EffectModeSubmitHook(guest.context);
}

bool dispatch(Guest& guest, GuestAddress job, std::uint32_t arg) {
  guest.state.gpr[3] = job;
  guest.state.gpr[4] = arg;
  guest.state.lr = kCallerReturn;
  g_calls.clear();
  return ac6EffectModeDispatchHook(guest.context);
}

void test_effect_jobs_run_their_submitted_mode() {
  Guest guest;
  make_manager(guest, kManager, kVtableX360, kDrawX360);
  const auto dt = std::bit_cast<std::uint32_t>(0.5f);
  // The update is submitted, then the draw overwrites the shared word before
  // the update's worker reads it.
  submit(guest, kManager, dt, 0u);
  submit(guest, kManager, 0x1234u, 2u);
  assert(guest.memory.read32_be(kModeWord) == 2u);

  assert(dispatch(guest, kManager, dt));
  assert(g_calls.size() == 1 && g_calls[0].target == kUpdate);
  assert(g_calls[0].r3 == kManager && g_calls[0].f1 == 0.5);
  assert(guest.state.lr == kCallerReturn);

  assert(dispatch(guest, kManager, 0x1234u));
  assert(g_calls.size() == 1 && g_calls[0].target == kDrawX360);
  assert(g_calls[0].r3 == kManager && g_calls[0].r4 == 0x1234u);
  assert(guest.state.lr == kCallerReturn);

  // Each record is claimed once: a repeat dispatch has none and runs the
  // original dispatcher.
  assert(!dispatch(guest, kManager, 0x1234u) && g_calls.empty());
  std::cout << "  [PASS] effect jobs run the mode they were submitted with\n";
}

void test_effect_mode_one_and_no_dispatch() {
  Guest guest;
  make_manager(guest, kManager, kVtableX360, kDrawX360);
  const auto dt = std::bit_cast<std::uint32_t>(0.25f);
  submit(guest, kManager, dt, 1u);
  assert(dispatch(guest, kManager, dt));
  assert(g_calls.size() == 1 && g_calls[0].target == kModeOneStep && g_calls[0].f1 == 0.25);

  // Mode 3 and above: the dispatcher returns without dispatching.
  submit(guest, kManager, 7u, 3u);
  assert(dispatch(guest, kManager, 7u) && g_calls.empty());
  std::cout << "  [PASS] mode 1 runs its step and mode 3 dispatches nothing\n";
}

void test_effect_draw_follows_the_manager_class() {
  Guest guest;
  make_manager(guest, kManager, kVtableAce6, kDrawAce6);
  submit(guest, kManager, 9u, 2u);
  assert(dispatch(guest, kManager, 9u));
  assert(g_calls.size() == 1 && g_calls[0].target == kDrawAce6);

  // A draw slot neither class has is not guessed at.
  constexpr GuestAddress kOther = 0x82900000u;
  make_manager(guest, kOther, kVtableUnknownDraw, 0x82123450u);
  submit(guest, kOther, 9u, 2u);
  assert(!dispatch(guest, kOther, 9u) && g_calls.empty());
  std::cout << "  [PASS] mode 2 runs the manager class's own draw\n";
}

void test_non_effect_jobs_are_not_recorded() {
  Guest guest;
  // A job whose vtable +4 slot is some other function.
  constexpr GuestAddress kJob = 0x82700000u;
  guest.memory.write32_be(0x82013004u, 0x82222220u);
  guest.memory.write32_be(kJob, 0x82013000u);
  submit(guest, kJob, 5u, 0u);
  assert(!dispatch(guest, kJob, 5u) && g_calls.empty());
  // Heap jobs outside the image and data range are skipped without reads.
  guest.state.gpr[5] = 0x00001000u;
  guest.state.gpr[6] = 0;
  ac6EffectModeSubmitHook(guest.context);
  std::cout << "  [PASS] only effect-manager jobs are recorded\n";
}

std::size_t run_exec(Guest& guest, bool (*hook)(ExecutionContext&), std::uint64_t r3) {
  guest.state.gpr[3] = r3;
  guest.state.lr = kCallerReturn;
  g_calls.clear();
  assert(hook(guest.context));
  assert(guest.state.lr == kCallerReturn);
  for (const auto& call : g_calls) assert(call.r3 == r3);  // every tick from Exec's own inputs
  return g_calls.size();
}

void test_cutscene_timeline_catches_up_with_its_audio() {
  Guest guest;
  // No audio consumed yet: no master clock, one tick.
  g_audio_tic = 0;
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x1111u) == 1u);
  // The first clock reading anchors the timeline: one tick.
  g_audio_tic = 48000u;
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x1111u) == 1u);
  // One tick of audio per frame stays in step.
  g_audio_tic += 1600u;
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x1111u) == 1u);
  // One tick of jitter is never corrected.
  g_audio_tic += 3200u;
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x1111u) == 1u);
  // A hitch: audio is now 6 ticks ahead of the 4 issued, 2 behind after this
  // frame's tick -> 2 catch-up ticks.
  g_audio_tic += 1600u * 3u;
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x2222u) == 3u);
  // A long stall catches up at most 3 extra ticks per frame, and the rest on
  // the following frames.
  g_audio_tic += 1600u * 20u;
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x3333u) == 4u);
  assert(run_exec(guest, ac6CutsceneResyncHookDD, 0x3333u) == 4u);
  std::cout << "  [PASS] the cutscene timeline catches up with its audio, bounded per frame\n";
}

void test_cutscene_sequencer_change_starts_a_new_session() {
  Guest guest;
  g_audio_tic = 96000u;
  assert(run_exec(guest, ac6CutsceneResyncHookEM, 0x4444u) == 1u);
  // The EM sequencer's own anchor: audio that ran before it does not count.
  g_audio_tic += 1600u;
  assert(run_exec(guest, ac6CutsceneResyncHookEM, 0x4444u) == 1u);
  for (const auto& call : g_calls) assert(call.target == kExecEM);
  std::cout << "  [PASS] each sequencer is anchored to the audio clock when it starts\n";
}

}  // namespace

int main() {
  std::cout << "Gracemeria US host-timing hooks:\n";
  test_effect_jobs_run_their_submitted_mode();
  test_effect_mode_one_and_no_dispatch();
  test_effect_draw_follows_the_manager_class();
  test_non_effect_jobs_are_not_recorded();
  test_cutscene_timeline_catches_up_with_its_audio();
  test_cutscene_sequencer_change_starts_a_new_session();
  return 0;
}
