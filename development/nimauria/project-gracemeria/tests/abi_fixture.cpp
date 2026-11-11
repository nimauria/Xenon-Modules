#include "xenon/cpu/runtime.hpp"

namespace {
xenon::cpu::ExecutionResult fixture_entry(xenon::cpu::ExecutionContext&) {
  return {xenon::cpu::FlowReason::Branch, 0x82000004u, 0u};
}

xenon::cpu::NativeCompiledEntry lookup(void*, xenon::cpu::ExecutionContext&,
                                       xenon::cpu::GuestAddress target,
                                       xenon::cpu::CompiledLookupKind) {
  return target == 0x82000000u ? &fixture_entry : nullptr;
}
}

#if defined(_WIN32)
#define GRACEMERIA_EXPORT extern "C" __declspec(dllexport)
#else
#define GRACEMERIA_EXPORT extern "C" __attribute__((visibility("default")))
#endif

GRACEMERIA_EXPORT void Xenon_BindCompiledRegistry(xenon::cpu::ExecutionContext& context) {
  context.compiled_registry = nullptr;
  context.compiled_lookup = &lookup;
}
