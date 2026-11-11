param(
  [string]$TraceName = "allocator-c4-full-trace"
)

$ErrorActionPreference = "Stop"

$projectRoot = "C:\Users\Elean\Documents\Development\Xbox360-Recomp\Project-Gracemeria"
$xenonRoot = "C:\Users\Elean\Documents\Development\Xbox360-Recomp\Xenon-Recomp"
$sourcePath = Join-Path $projectRoot "generated\ac6-us\functions\shard_2184475848.cpp"
$symbolPath = Join-Path $projectRoot "build\generated\ac6-us"
$launchConfig = Join-Path $projectRoot "runtime\ac6-startup\launch.json"
$sessionDir = Join-Path $projectRoot "runtime\ac6-startup\session"
$runtimeHost = Join-Path $xenonRoot "build\gen10\launcher\xenon_runtime_host.exe"
$cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
$commandFile = Join-Path $sessionDir "$TraceName.cdb"
$debuggerLog = Join-Path $sessionDir "$TraceName.txt"
$runtimeLog = Join-Path $sessionDir "$TraceName-runtime.txt"

foreach ($required in @($sourcePath, $launchConfig, $runtimeHost, $cdb)) {
  if (-not (Test-Path -LiteralPath $required)) {
    throw "Required allocator-trace input is missing: $required"
  }
}

$functionNames = @(
  "xenon_fn_823474C8_dispatch_v2",
  "xenon_fn_82347598_dispatch_v2",
  "xenon_fn_82347718_dispatch_v2",
  "xenon_fn_823478D8_dispatch_v2",
  "xenon_fn_82347998_dispatch_v2",
  "xenon_fn_82347A98_dispatch_v2",
  "xenon_fn_82347BE0_dispatch_v2",
  "xenon_fn_82347EC8_dispatch_v2",
  "xenon_fn_82348100_dispatch_v2",
  "xenon_fn_823483E8_dispatch_v2",
  "xenon_fn_823485F0_dispatch_v2",
  "xenon_fn_823486C8_dispatch_v2",
  "xenon_fn_82348900_dispatch_v2"
)

$sourceLines = Get-Content -LiteralPath $sourcePath
$selectedLabels = [System.Collections.Generic.List[object]]::new()

foreach ($functionName in $functionNames) {
  $start = -1
  for ($index = 0; $index -lt $sourceLines.Count; ++$index) {
    if ($sourceLines[$index] -match ("^static ExecutionResult " + [regex]::Escape($functionName) + "\(")) {
      $start = $index
      break
    }
  }
  if ($start -lt 0) {
    throw "Could not find generated allocator function $functionName"
  }

  $end = $sourceLines.Count - 1
  for ($index = $start + 1; $index -lt $sourceLines.Count; ++$index) {
    if ($sourceLines[$index] -match "^static ExecutionResult ") {
      $end = $index - 1
      break
    }
  }

  for ($index = $start; $index -le $end; ++$index) {
    if ($sourceLines[$index] -match "^L_([0-9A-F]+):") {
      $selectedLabels.Add([pscustomobject]@{
          Function = $functionName
          Address = $matches[1]
          Line = $index + 1
        })
    }
  }
}

$commands = [System.Collections.Generic.List[string]]::new()
$commands.Add("sxd eh")
$commands.Add(".lines -e")
$commands.Add("r @`$t0=0")
$commands.Add("r @`$t1=0")
$commands.Add('bu xenon_game_module!rex_sub_82348720_dispatch_v2 ".if (poi(poi(@rdx)+0x20)==0xc4) { r @$t0=poi(@rdx); r @$t1=@$tid; be *; bd 0; .echo ==== C4 TRACE ARMED ====; .printf \"thread=%x state=%p entry=%p r3=%p size=%p align=%p lr=%p cia=%p nia=%p\\n\", @$tid, @$t0, @r8, poi(@$t0+0x18), poi(@$t0+0x20), poi(@$t0+0x28), poi(@$t0+0xa00), dwo(@$t0+0xa38), dwo(@$t0+0xa3c); dq @$t0 L20; gc } .else { gc }"')

foreach ($functionName in $functionNames) {
  $commands.Add(('bu xenon_game_module!{0} ".if (@$t0!=0) {{ .if (poi(@rdx)==@$t0) {{ .echo ==== ENTER {0} ====; .printf \"thread=%x entry=%p lr=%p ctr=%p cr=%x xer=%x cia=%x nia=%x\\n\", @$tid, @r8, poi(@$t0+0xa00), poi(@$t0+0xa08), dwo(@$t0+0xa10), dwo(@$t0+0xa14), dwo(@$t0+0xa38), dwo(@$t0+0xa3c); dq @$t0 L20; gc }} .else {{ gc }} }} .else {{ gc }}"' -f $functionName))
}

$tick = [char]96
foreach ($label in $selectedLabels) {
  $location = "{0}{1}:{2}{0}" -f $tick, $sourcePath, $label.Line
  $commands.Add(('bu {0} ".if (@$t0!=0) {{ .if (@$tid==@$t1) {{ .echo ---- BB {1} {2} source-line {3} ----; .printf \"r3=%p r4=%p r5=%p r6=%p r7=%p r8=%p r9=%p r10=%p r11=%p r26=%p r27=%p r28=%p r29=%p r30=%p r31=%p lr=%p ctr=%p cr=%x xer=%x cia=%x nia=%x\\n\", poi(@$t0+0x18), poi(@$t0+0x20), poi(@$t0+0x28), poi(@$t0+0x30), poi(@$t0+0x38), poi(@$t0+0x40), poi(@$t0+0x48), poi(@$t0+0x50), poi(@$t0+0x58), poi(@$t0+0xd0), poi(@$t0+0xd8), poi(@$t0+0xe0), poi(@$t0+0xe8), poi(@$t0+0xf0), poi(@$t0+0xf8), poi(@$t0+0xa00), poi(@$t0+0xa08), dwo(@$t0+0xa10), dwo(@$t0+0xa14), dwo(@$t0+0xa38), dwo(@$t0+0xa3c); dq @$t0 L20; .if (({1}==82348688) & (poi(@$t0+0x18)==0)) {{ .echo ==== NULL LOOKUP CONFIRMED AT 82348688 ====; kb; q }} .else {{ gc }} }} .else {{ gc }} }} .else {{ gc }}"' -f $location, $label.Address, $label.Function, $label.Line))
}

$commands.Add("bd *")
$commands.Add("be 0")
$commands.Add("g")

New-Item -ItemType Directory -Force -Path $sessionDir | Out-Null
[System.IO.File]::WriteAllLines($commandFile, $commands, [System.Text.UTF8Encoding]::new($false))

$existingRuntimeLog = Join-Path $sessionDir "log.txt"
if (Test-Path -LiteralPath $existingRuntimeLog) {
  Copy-Item -LiteralPath $existingRuntimeLog -Destination (Join-Path $sessionDir "$TraceName-before-runtime.txt") -Force
}

$env:_NT_SYMBOL_PATH = $symbolPath
& $cdb -o -G -logo $debuggerLog -cf $commandFile $runtimeHost --launch-config $launchConfig
$debuggerExitCode = $LASTEXITCODE

if (Test-Path -LiteralPath $existingRuntimeLog) {
  Copy-Item -LiteralPath $existingRuntimeLog -Destination $runtimeLog -Force
}

Write-Host "Debugger exit code: $debuggerExitCode"
Write-Host "Debugger trace: $debuggerLog"
Write-Host "Runtime trace:  $runtimeLog"
exit $debuggerExitCode
