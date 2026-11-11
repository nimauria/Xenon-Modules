// Calling an AC6 guest function from a Gracemeria instruction hook.
//
// A hook placed at a function's first instruction with action "return" stands
// in for the whole function: it may run guest functions itself, then the
// generated code returns to the caller's LR. call_guest_function() runs one
// guest function the way a `bl` would: LR holds the return address the
// callee's `blr` goes back to, its compiled entry runs (or Xenon's dynamic
// fallback for code the module did not compile), and a tail branch out of it
// is followed. On return LR is restored, as the callee's epilogue leaves it.
//
// Hooks declare it themselves: Xenon compiles a revision's hooks/*.cpp and
// keys the prepared module on their text, so the helper is a source file
// rather than a header.

#include <cstdint>
#include <cstdio>

#include "xenon/cpu/runtime.hpp"

namespace gracemeria::ac6 {

// True when the callee returned normally. Anything else (a trap, a longjmp
// out of the callee, an unresolvable branch) cannot be carried out of a hook;
// it is reported and the caller treats the call as failed.
bool call_guest_function(xenon::cpu::ExecutionContext& context, xenon::cpu::GuestAddress target,
                         const char* site) {
  using xenon::cpu::CompiledLookupKind;
  using xenon::cpu::FlowReason;
  const auto return_address = static_cast<xenon::cpu::GuestAddress>(context.state.lr & ~3ull);
  xenon::cpu::ExecutionResult result{};
  if (auto* native = context.lookup_compiled(target, CompiledLookupKind::Call)) {
    result = context.follow_tail_branches(native(context));
  } else {
    const auto fallback = context.try_dynamic_fallback(target, CompiledLookupKind::Call);
    if (!fallback.handled) {
      std::fprintf(stderr, "[Gracemeria] %s: guest function 0x%08X has no compiled or fallback entry\n",
                   site, target);
      return false;
    }
    result = context.follow_tail_branches(fallback.result);
  }
  context.state.lr = return_address;
  if ((result.reason == FlowReason::Return && result.next_address == return_address) ||
      result.reason == FlowReason::Fallthrough)
    return true;
  std::fprintf(stderr,
               "[Gracemeria] %s: guest function 0x%08X ended with flow reason %u at 0x%08X instead of "
               "returning to 0x%08X\n",
               site, target, static_cast<unsigned>(result.reason), result.next_address, return_address);
  return false;
}

}  // namespace gracemeria::ac6
