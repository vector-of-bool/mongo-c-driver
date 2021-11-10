[CmdletBinding()]
param (
    # SVG Filepath to write to
    [Parameter(Mandatory)]
    [string]
    $OutFile,
    # Number of seconds to sample
    [Parameter()]
    [int]
    $Duration = 5,
    # Sample frequency (Hertz)
    [Parameter()]
    [int]
    $SampleFrequency = 99,
    # Title for the new flamegraph
    [Parameter()]
    [string]
    $Title = "Flame Graph",
    # Subtitle for the new flamegraph
    [Parameter()]
    [string]
    $SubTitle = ""
)

$ErrorActionPreference = "Stop"

if ($null -eq $Title) {
    $Title = "Flame Graph"
}

Write-Host "Sampling for $Duration seconds..."
sudo ./profile `
    --user-stacks-only `
    --pid (& pidof bench) `
    --include-idle `
    --frequency $SampleFrequency `
    --stack-storage-size (16384 * 128) `
    --folded $Duration | Out-File samples.txt

gc samples.txt `
| grep run_thread `
| perl flamegraph.pl `
    --title $Title `
    --cp `
    --colors=mongoc `
    - `
| Out-File $OutFile

Write-Host "Generated flamegraph to $OutFile"

# Old pool, 1 thread: 8100 ops/s
# Old pool, 10 threads: 3300 ops/s
# Old pool, 10 threads: 280 ops/s

# New pool, 1 thread: 7800 ops/s
# New pool, 10 threads: 3500 ops/s
# New pool, 10 threads: 320 ops/s
