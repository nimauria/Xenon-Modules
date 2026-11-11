#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "xenon/core/session.hpp"
#include "xenon/xbox/xex_loader.hpp"

namespace {
const char* status_name(const xenon::core::ExportDescriptor* desc) {
  if (!desc) return "missing";
  using R = xenon::core::ExportRequirement;
  if (desc->requirement == R::Stubbed) return "stub";
  return desc->handler ? "registered" : "missing";
}
}

int main(int argc, char** argv) {
  if (argc != 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    std::cout << "usage: gracemeria-import-coverage <default.xex>\n";
    return argc == 2 ? 0 : 1;
  }
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::cerr << "unable to open XEX\n";
    return 2;
  }
  const std::vector<char> raw((std::istreambuf_iterator<char>(file)), {});
  std::vector<std::byte> bytes(raw.size());
  std::transform(raw.begin(), raw.end(), bytes.begin(), [](char c) {
    return static_cast<std::byte>(static_cast<unsigned char>(c));
  });

  xenon::xbox::XexImage image;
  std::string error;
  if (!xenon::xbox::parse_xex_image(bytes, image, &error)) {
    std::cerr << error << '\n';
    return 3;
  }

  xenon::core::XenonSession session;
  xenon::core::SessionConfig config;
  // Match a real Normal Play launch config (see docs/runtime/RUNTIME_HOST.md)
  // so this report reflects what a running AC6 session actually has
  // registered - graphics/input/audio/network all register a meaningful
  // slice of xam.xex/xboxkrnl.exe exports (XamInput*, Vd*, XAudio*/XMA*,
  // NetDll_*), so disabling them here previously made this report show a
  // large number of exports as "missing" that a real launch already has.
  // SessionConfig defaults every enable_* flag to false, so all four must be
  // set explicitly - relying on the struct's defaults would silently
  // reproduce the same blind spot this comment describes fixing.
  config.enable_graphics = true;
  config.enable_input = true;
  // This tool's own build now enables XENON_ENABLE_AUDIO (see this project's
  // CMakeLists.txt), reusing the same managed SDL2/bundled-FFmpeg dependency
  // Xenon-Recomp's own gen10 build resolves, so audio::register_xbox_audio_exports()
  // is linked in and XAudio*/XMA* ordinals (src/audio/exports.cpp) resolve
  // for real here instead of being cross-checked only against a live launch.
  config.enable_audio = true;
  config.enable_network = true;
  const auto initialized = session.initialize(config);
  if (!initialized.success) {
    std::cerr << initialized.message << '\n';
    return 4;
  }

  std::size_t registered = 0, stubbed = 0, missing = 0;
  std::cout << "library\tordinal\tname\tstatus\n";
  for (const auto& item : image.imports) {
    // Mirror XenonSession::resolve_xex_imports()'s own real-session logic
    // exactly, rather than always resolving through the function-export
    // namespace: a type-0 FunctionAddress record is paired metadata for a
    // type-1 FunctionThunk and is never separately resolved, and a true
    // Variable import resolves through the separate guest-backed variable
    // registry (resolve_variable()/contains_variable()), not resolve().
    // Treating every import as a function-namespace lookup made every
    // variable import (XboxKrnlVersion, KeTimeStampBundle, ...) report
    // "missing" even when XenonSession::init_kernel_variable_exports()
    // registers it correctly - a tool-only false negative, not a real gap.
    if (item.is_function_address()) continue;

    if (item.is_variable()) {
      const bool resolved = !item.symbol.empty()
                                 ? session.exports()->resolve_variable(item.module, item.symbol).has_value()
                                 : session.exports()->contains_variable(item.module, item.ordinal);
      const char* status = resolved ? "registered" : "missing";
      if (resolved) ++registered; else ++missing;
      std::cout << item.module << '\t' << item.ordinal << '\t' << item.symbol << '\t'
                << status << " (variable)\n";
      continue;
    }

    const auto* desc = item.symbol.empty()
                           ? session.exports()->resolve(item.module, item.ordinal)
                           : session.exports()->resolve(item.module, item.symbol);
    if (!desc && !item.symbol.empty()) desc = session.exports()->resolve(item.module, item.ordinal);
    const auto* status = status_name(desc);
    if (std::string_view(status) == "registered") ++registered;
    else if (std::string_view(status) == "stub") ++stubbed;
    else ++missing;
    std::cout << item.module << '\t' << item.ordinal << '\t'
              << (item.symbol.empty() ? (desc ? desc->name : std::string{}) : item.symbol)
              << '\t' << status << '\n';
  }
  std::cout << "summary\tregistered=" << registered << "\tstub=" << stubbed
            << "\tmissing=" << missing << '\n';
  session.shutdown();
  return missing == 0 ? 0 : 5;
}
