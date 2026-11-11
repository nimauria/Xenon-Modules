# Ace Combat 6 memory/config import report

Source: `C:/Users/Elean/Documents/Development/AC6-Extracted/default.xex`

Evidence: Xenon's recompilation pipeline regenerated `generated/ac6-us/imports.cpp`
from that XEX. The manifest contains 468 thunk records (two thunk addresses for
each logical import) and 151 unique `xboxkrnl.exe` ordinals. The companion JSON
file is the machine-readable targeted report.

| Export | Ordinal | Imported by AC6 | Xenon status after this pass |
|---|---:|:---:|---|
| ExAllocatePool | `0x09` | yes | missing |
| ExAllocatePoolWithTag | `0x0A` | no | missing |
| ExAllocatePoolTypeWithTag | `0x0B` | no | missing |
| ExFreePool | `0x0F` | yes | missing |
| ExGetXConfigSetting | `0x10` | yes | implemented |
| MmAllocatePhysicalMemory | `0xB9` | no | missing |
| MmAllocatePhysicalMemoryEx | `0xBA` | yes | implemented |
| MmFreePhysicalMemory | `0xBD` | yes | implemented |
| MmGetPhysicalAddress | `0xBE` | yes | implemented |
| MmQueryAddressProtect | `0xC4` | yes | implemented |
| MmQueryAllocationSize | `0xC5` | no | implemented |
| MmQueryStatistics | `0xC6` | yes | partial: real aggregate counts, unavailable subdivisions zero |
| MmSetAddressProtect | `0xC7` | yes | implemented |
| NtAllocateVirtualMemory | `0xCC` | yes | implemented |
| NtFreeVirtualMemory | `0xDC` | yes | missing |
| NtProtectVirtualMemory | `0xE1` | no | missing |
| NtQueryVirtualMemory | `0xEE` | yes | missing |

“Imported” only establishes a thunk in this revision. It does not establish
that the title executes that thunk on the failing startup path. The export
flight recorder is the runtime evidence source for that distinction.
