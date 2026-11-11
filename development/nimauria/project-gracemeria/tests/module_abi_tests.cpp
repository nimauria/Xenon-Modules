#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "xenon/cpu/flat_memory.hpp"
#include "xenon/cpu/runtime.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
using Binder = void (*)(xenon::cpu::ExecutionContext&);

void* open_library(const char* path) {
#if defined(_WIN32)
  return reinterpret_cast<void*>(LoadLibraryA(path));
#else
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* resolve_symbol(void* handle, const char* name) {
#if defined(_WIN32)
  return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
  return dlsym(handle, name);
#endif
}

void close_library(void* handle) {
#if defined(_WIN32)
  if (handle) FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
  if (handle) dlclose(handle);
#endif
}
}

int main(int argc, char** argv) {
  assert(argc == 2);
  void* library = open_library(argv[1]);
  if (!library) {
#if !defined(_WIN32)
    std::cerr << dlerror() << '\n';
#endif
    return 1;
  }

  auto* raw = resolve_symbol(library, "Xenon_BindCompiledRegistry");
  assert(raw != nullptr);
  auto binder = reinterpret_cast<Binder>(raw);

  xenon::cpu::CpuState state{};
  xenon::cpu::FlatMemory memory(0x1000u, 0x82000000u);
  xenon::cpu::NullRuntimeServices runtime;
  xenon::cpu::ExecutionContext context(state, memory, runtime);
  assert(context.compiled_lookup == nullptr);

  binder(context);
  assert(context.compiled_lookup != nullptr);
  const auto entry = context.lookup_compiled(0x82000000u, xenon::cpu::CompiledLookupKind::Call);
  assert(entry != nullptr);
  assert(context.lookup_compiled(0x82000008u, xenon::cpu::CompiledLookupKind::Call) == nullptr);

  const auto result = entry(context);
  assert(result.reason == xenon::cpu::FlowReason::Branch);
  assert(result.next_address == 0x82000004u);

  close_library(library);
  return 0;
}
