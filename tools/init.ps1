$ErrorActionPreference = "Stop"

$IS_WINDOWS = $PSVersionTable.Platform -eq "Win32NT" -or $PSVersionTable.PSEdition -eq "Desktop"

$TOOLS_DIR = $PSScriptRoot
$MONGOC_DIR = Split-Path -Parent $TOOLS_DIR

$USER_CACHES_DIR = $env:USER_CACHES_DIR
if ([String]::IsNullOrEmpty($USER_CACHES_DIR)) {
    if ($IS_WINDOWS) {
        $USER_CACHES_DIR = $env:LOCALAPPDATA
    }
    elseif (-not [string]::IsNullOrEmpty( $env:XDG_CACHES_HOME)) {
        $USER_CACHES_DIR = $env:XDG_CACHES_HOME
    }
    elseif (Test-Path "$env:HOME/Library/Caches") {
        $USER_CACHES_DIR = "$env:HOME/Library/Caches"
    }
    else {
        $USER_CACHES_DIR = "$env:HOME/.cache"
    }
}

$BUILD_CACHE_BUST = $env:BUILD_CACHE_BUST
if ([String]::IsNullOrEmpty($BUILD_CACHE_BUST)) {
    $BUILD_CACHE_BUST = "1"
}
$BUILD_CACHE_DIR = Join-Path $USER_CACHES_DIR "mongoc.$BUILD_CACHE_BUST"
Write-Debug "Calculated mongoc build cache directory to be [$BUILD_CACHE_DIR]"

$EXE_SUFFIX = ""
if ($IS_WINDOWS) {
    $EXE_SUFFIX = ".exe"
}

function Test-Command {
    param (
        [string[]]$Name,
        [System.Management.Automation.CommandTypes]$CommandType = 'All'
    )
    $found = @(Get-Command -Name:$Name -CommandType:$CommandType -ErrorAction Ignore -TotalCount 1)
    if ($found.Length -ne 0) {
        return $true
    }
    return $false
}

function Find-Python {
    if (Test-Command "py" -CommandType Application -and (& py -c "(x:=0)")) {
        $found = "py"
    }
    else {
        foreach ($n in 20..8) {
            $cand = "python3.$n"
            Write-Debug "Try Python: [$cand]"
            if (!(Test-Command $cand -CommandType Application)) {
                continue;
            }
            if (& "$cand" -c "(x:=0)") {
                $found = "$cand"
                break
            }
        }
    }
    $found = (Get-Command "$found" -CommandType Application).Source
    $ver = (& $found -c "import sys; print(sys.version)" | Out-String).Trim()
    Write-Debug "Found Python: [$found] (Version $ver)"
    return $found
}



function Test-TruthyString([string]$s) {
    return (-not [string]::IsNullOrEmpty($s) -and ($s.ToLower() -ne "off") -and ($s -ne "0") -and ($s.ToLower() -ne "false"));
}

function Build-CMakeProject {
    [CmdletBinding(PositionalBinding = $false)]
    param (
        # The source directory to build
        [Parameter(Mandatory, Position = 0)]
        [string]
        $SourceDir,
        # The build directory to use
        [string]
        $BuildDir,
        # The directory in which to install
        [string]
        $InstallDir,
        # Additional flags to pass to CMake
        [string[]]
        $ConfigureOptions,
        # The CMake executable to use
        [string]
        $CMake,
        # Number of parallel jobs to run
        $Jobs,
        # The configuration to build
        [string]
        $Config = "Debug",
        # If true, execute CTest
        [switch]
        $Test,
        # If true, remove the CMake cache before building
        [switch]
        $CleanConfigure,
        # The CMake generator to use for configuring the project
        [string]
        $Generator
    )
    $ErrorActionPreference = "Stop"
    $SourceDir = [System.IO.Path]::GetFullPath($SourceDir)
    if ([string]::IsNullOrEmpty($BuildDir)) {
        Join-Path $SourceDir "_build"
    }
    if ($CleanConfigure) {
        Remove-Item $BuildDir/CMakeCache.txt -ErrorAction Ignore
        Remove-Item $BuildDir/CMakeFiles -ErrorAction Ignore -Recurse
    }
    if (-not [string]::IsNullOrEmpty($Generator)) {
        $ConfigureOptions += "-G$Generator"
    }
    if ([string]::IsNullOrEmpty($CMake)) {
        $CMake = (Get-Command cmake -CommandType Application).Source
        Write-Debug "Using CMake from PATH: [$cmake]"
    }
    if (-not [string]::IsNullOrEmpty($InstallDir)) {
        $ConfigureOptions += "-DCMAKE_INSTALL_PREFIX=$InstallDir"
    }
    Write-Debug "Executing CMake configure with source directory [$SourceDir] and build directory [$BuildDir]"
    Write-Debug "  Env: CC=$env:CC"
    Write-Debug "  Env: CXX=$env:CXX"
    & $CMake -S $SourceDir -B $BuildDir -D CMAKE_BUILD_TYPE=$Config @ConfigureOptions
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configure failed [Source=$SourceDir, Build=$BuildDir, $LASTEXITCODE]"
        return
    }
    if ($null -eq $Jobs ) {
        if (@(Get-Command nproc -ErrorAction Ignore).Length) {
            $Jobs = [int]::Parse((& nproc).Trim()) + 2
        }
        else {
            $Jobs = [int](Get-CimInstance Win32_Processor).NumberOfLogicalProcessors + 2
        }
    }
    & $CMake --build $BuildDir --parallel $Jobs --config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake build failed [Source=$SourceDir, Build=$BuildDir, $LASTEXITCODE]"
        return
    }
    if (-not [string]::IsNullOrEmpty($InstallDir)) {
        & $CMake --install $BuildDir --config $Config
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake install failed [Source=$SourceDir, Build=$BuildDir, $LASTEXITCODE]"
            return
        }
    }
    if ($Test) {
        $cm_bin = Split-Path -Parent $CMake
        $ctest = Join-Path $cm_bin "ctest$EXE_SUFFIX"
        Assert-Path "$BuildDir/CTestTestfile.cmake" -PathType Leaf
        & $CMake -E chdir $BuildDir $ctest -C $Config --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CTest execution returned non-zero [$LASTEXITCODE]"
            return
        }
    }
    return
}


function Assert-Path {
    [CmdletBinding(PositionalBinding = $false)]
    param (
        # The paths to check for
        [Parameter(Mandatory, Position = 0)]
        [string[]]
        $Path,
        # Invert the condition
        [switch]
        $Not,
        # The type of file to check for
        [Parameter(Mandatory)]
        [Microsoft.PowerShell.Commands.TestPathType]
        $PathType,
        # Check each relative path relative to this base path
        [string]
        $BasePath
    )
    foreach ($item in $Path) {
        if (![string]::IsNullOrEmpty($BasePath)) {
            $item = Join-Path $BasePath $item
        }
        Write-Debug "Testing for path [$Path] (Expect-absent? $Not)"
        if (Test-Path $Path -PathType:$PathType) {
            if ($Not) {
                Write-Error "Expected path [$Path] to be absent"
            }
        }
        elseif (!$Not) {
            Write-Error "Expected path [$Path] to be present"
        }
    }
}

function Assert-InstalledFiles {
    [CmdletBinding(PositionalBinding = $false)]
    param(
        # The install directory to check
        [Parameter(Mandatory)]
        [string]
        $InstallDir,
        # If true, check as a MinGW build
        [switch]
        $MinGW
    )
    $ErrorActionPreference = "Stop"
    if ($MinGW) {
        Assert-Path $InstallDir/bin/libmongoc-1.0.dll
        Assert-Path -Not $InstallDir/bin/mongoc-1.0.dll
    }
    else {
        Assert-Path $InstallDir/bin/mongoc-1.0.dll
        Assert-Path -Not $InstallDir/bin/libmongoc-1.0.dll
    }
    Assert-Path `
        $InstallDir/lib/pkgconfig/libmongoc-1.0.pc, `
        $InstallDir/lib/pkgconfig/libmongoc-static-1.0.pc, `
        $InstallDir/lib/cmake/mongoc-1.0/mongoc-1.0-config.cmake, `
        $InstallDir/lib/cmake/mongoc-1.0/mongoc-1.0-config-version.cmake, `
        $InstallDir/lib/cmake/mongoc-1.0/mongoc-targets.cmake
}

