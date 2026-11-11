param(
  [string]$TraceName = "resource-path-trace"
)

$ErrorActionPreference = "Stop"

$projectRoot = "C:\Users\Elean\Documents\Development\Xbox360-Recomp\Project-Gracemeria"
$xenonRoot = "C:\Users\Elean\Documents\Development\Xbox360-Recomp\Xenon-Recomp"
$symbolPath = Join-Path $projectRoot "build\generated\ac6-us"
$launchConfig = Join-Path $projectRoot "runtime\ac6-startup\launch.json"
$sessionDir = Join-Path $projectRoot "runtime\ac6-startup\session"
$runtimeHost = Join-Path $xenonRoot "build\gen10\launcher\xenon_runtime_host.exe"
$cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
$commandFile = Join-Path $sessionDir "$TraceName.cdb"
$debuggerLog = Join-Path $sessionDir "$TraceName.txt"
$runtimeLog = Join-Path $sessionDir "$TraceName-runtime.txt"

$traceFunctions = @(
  [pscustomobject]@{
    Path = Join-Path $projectRoot "generated\ac6-us\functions\shard_2184475848.cpp"
    Name = "rex_sub_82348720_dispatch_v2"
    Addresses = @("82348720", "82348758", "82348764", "8234876C")
  },
  [pscustomobject]@{
    Path = Join-Path $projectRoot "generated\ac6-us\functions\shard_2184505608.cpp"
    Name = "xenon_fn_8234FC38_dispatch_v2"
    Addresses = @("8234FD2C", "8234FD38", "8234FD50", "8234FD68", "8234FD74", "8234FD90")
  },
  [pscustomobject]@{
    Path = Join-Path $projectRoot "generated\ac6-us\functions\shard_2184514968.cpp"
    Name = "xenon_fn_82352178_dispatch_v2"
    Addresses = @("82352178", "82352180", "823521A4", "823521B0", "823521C0", "823521C8", "823521D8", "823521DC", "823521E4")
  },
  [pscustomobject]@{
    Path = Join-Path $projectRoot "generated\ac6-us\functions\shard_2184537664.cpp"
    Name = "xenon_fn_82356E50_dispatch_v2"
    Addresses = @("82356E50", "82356E58", "82356E70", "82356E78", "82356E84", "82356E8C", "82356E94")
  }
)

foreach ($required in @($launchConfig, $runtimeHost, $cdb)) {
  if (-not (Test-Path -LiteralPath $required)) {
    throw "Required resource-path trace input is missing: $required"
  }
}

$labels = [System.Collections.Generic.List[object]]::new()
foreach ($function in $traceFunctions) {
  if (-not (Test-Path -LiteralPath $function.Path)) {
    throw "Generated trace source is missing: $($function.Path)"
  }

  $sourceLines = Get-Content -LiteralPath $function.Path
  foreach ($address in $function.Addresses) {
    $pattern = "^L_$($address):"
    $line = -1
    for ($index = 0; $index -lt $sourceLines.Count; ++$index) {
      if ($sourceLines[$index] -match $pattern) {
        $line = $index + 1
        break
      }
    }
    if ($line -lt 0) {
      throw "Could not find generated label L_$address in $($function.Path)"
    }
    $labels.Add([pscustomobject]@{
        Path = $function.Path
        Function = $function.Name
        Address = $address
        Line = $line
      })
  }
}

$commands = [System.Collections.Generic.List[string]]::new()
$commands.Add("sxd eh")
$commands.Add(".lines -e")
$commands.Add("r @`$t0=0")
$commands.Add("r @`$t1=0")
$commands.Add('bu xenon_game_module!rex_sub_82348720_dispatch_v2 ".if (poi(poi(@rdx)+0x20)==0xc4) { r @$t0=poi(@rdx); r @$t1=@$tid; be *; bd 0; .echo ==== C4 RESOURCE PATH TRACE ARMED ====; .printf \"thread=%x state=%p entry=%p r3=%p size=%p align=%p lr=%p cia=%x nia=%x\\n\", @$tid, @$t0, @r8, poi(@$t0+0x18), poi(@$t0+0x20), poi(@$t0+0x28), poi(@$t0+0xa00), dwo(@$t0+0xa38), dwo(@$t0+0xa3c); dq @$t0 L20; gc } .else { gc }"')

$tick = [char]96
foreach ($label in $labels) {
  $location = "{0}{1}:{2}{0}" -f $tick, $label.Path, $label.Line
  $isTerminal = $label.Address -eq "82356E94"
  $tail = if ($isTerminal) {
    '.echo ==== PRECISE NULL-DEREFERENCE INPUT REACHED ====; kb; q'
  } else {
    'gc'
  }
  $commands.Add(('bu {0} ".if (@$t0!=0) {{ .if (@$tid==@$t1) {{ .echo ---- PATH {1} {2} source-line {3} ----; .printf \"thread=%x r0=%p r1=%p r2=%p r3=%p r4=%p r5=%p r6=%p r7=%p r8=%p r9=%p r10=%p r11=%p r12=%p r13=%p r14=%p r15=%p r16=%p r17=%p r18=%p r19=%p r20=%p r21=%p r22=%p r23=%p r24=%p r25=%p r26=%p r27=%p r28=%p r29=%p r30=%p r31=%p lr=%p ctr=%p cr=%x xer=%x cia=%x nia=%x\\n\", @$tid, poi(@$t0+0x00), poi(@$t0+0x08), poi(@$t0+0x10), poi(@$t0+0x18), poi(@$t0+0x20), poi(@$t0+0x28), poi(@$t0+0x30), poi(@$t0+0x38), poi(@$t0+0x40), poi(@$t0+0x48), poi(@$t0+0x50), poi(@$t0+0x58), poi(@$t0+0x60), poi(@$t0+0x68), poi(@$t0+0x70), poi(@$t0+0x78), poi(@$t0+0x80), poi(@$t0+0x88), poi(@$t0+0x90), poi(@$t0+0x98), poi(@$t0+0xa0), poi(@$t0+0xa8), poi(@$t0+0xb0), poi(@$t0+0xb8), poi(@$t0+0xc0), poi(@$t0+0xc8), poi(@$t0+0xd0), poi(@$t0+0xd8), poi(@$t0+0xe0), poi(@$t0+0xe8), poi(@$t0+0xf0), poi(@$t0+0xf8), poi(@$t0+0xa00), poi(@$t0+0xa08), dwo(@$t0+0xa10), dwo(@$t0+0xa14), dwo(@$t0+0xa38), dwo(@$t0+0xa3c); dq @$t0 L20; {4} }} .else {{ gc }} }} .else {{ gc }}"' -f $location, $label.Address, $label.Function, $label.Line, $tail))
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
