[CmdletBinding(PositionalBinding = $False)]
param (
    # CMake to use for the test
    [string]
    $CMake,
    # The directory in which to write scratch data
    [string]
    $ScratchDir = "_build/link-test",
    # Load this VS version for the build
    [string]
    $VSVersion,
    # Load this VS arch for the build
    [string]
    $VSArch = "amd64",
    # Number of parallel jobs to run
    $Jobs,
    # Whether we are testing static linking
    [switch]
    $StaticLink,
    # Build with Snappy compression
    [switch]
    $EnableSnappy,
    # Enable SSL for the test
    [switch]
    $EnableSSL
)
$ErrorActionPreference = "Stop"

$repo_root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tools_dir = Join-Path $repo_root "tools"

if (-not [string]::IsNullOrEmpty($VSVersion)) {
    $vs_env_run = Join-Path $tools_dir "vs-env-run.ps1"
    $script = $MyInvocation.InvocationName
    Write-Debug "Re-running with VS $VSVersion $VSArch"
    $outer_args = $args
    return & $vs_env_run -Version:$VSVersion -TargetArch:$VSArch -ScratchDir:$ScratchDir -Command {
        $env:CC = "cl.exe"
        $env:CXX = "cl.exe"
        $ret = & $script @outer_args
        return $ret
    }.GetNewClosure()
}

if ([string]::IsNullOrEmpty($CMake)) {
    $CMake = Get-Command -Name "cmake" -CommandType Application | Select-Object -First 1
}

. $tools_dir/init.ps1

Write-Debug "Using CMake [$CMake]"
$ScratchDir = [System.IO.Path]::GetFullPath($ScratchDir)
Write-Debug "Using scratch directory [$ScratchDir]"
Remove-Item -Recurse $ScratchDir -ErrorAction Ignore

$install_dir = Join-Path $ScratchDir "install-dir"
Write-Debug "Using install directory [$instlal_dir]"
Remove-Item $install_dir -Recurse -ErrorAction Ignore
New-Item $install_dir -ItemType Directory | Out-Null

$snappy_bool = "OFF"
if ($EnableSnappy) {
    $snappy_version = "1.1.7"
    Invoke-WebRequest -MaximumRetryCount 5 -UseBasicParsing `
        -Uri "https://github.com/google/snappy/archive/$snappy_version.tar.gz" `
        -OutFile $ScratchDir/snappy.tar.gz

    & $CMake -E chdir $ScratchDir `
        $CMake -E tar xf $ScratchDir/snappy.tar.gz "snappy-$snappy_version"
    if ($LASTEXITCODE -ne 0) { throw "tar extraction failed" }

    Build-CMakeProject -SourceDir "$ScratchDir/snappy-$snappy_version" `
        -BuildDir (Join-Path $ScratchDir "snappy-build") `
        -InstallDir $install_dir `
        -Jobs:$Jobs
    $snappy_bool = "ON"
}

$ssl_opt = if ($EnableSSL) {
    if ($IS_WINDOWS) {
        "WINDOWS"
    }
    else {
        "OPENSSL"
    }
}
else {
    "OFF"
}

$mongoc_build = Join-Path $ScratchDir "mongoc"
Remove-Item -Recurse -ErrorAction Ignore $mongoc_build

Build-CMakeProject -SourceDir $MONGOC_DIR `
    -BuildDir $mongoc_build `
    -ConfigureOptions  @( `
        "-DENABLE_SSL=$ssl_opt", `
        "-DENABLE_SNAPPY=$snappy_bool" ) `
    -InstallDir $install_dir

if ($StaticLink) {
    $ex_name = "find_package_static"
}
else {
    $ex_name = "find_package"
}

if ($EnableSSL) {
    $env:MONGODB_EXAMPLE_URI = "mongodb://localhost/?ssl=true&sslclientcertificatekeyfile=client.pem&sslcertificateauthorityfile=ca.pem&sslallowinvalidhostnames=true"
}

$example = Join-Path $repo_root "src/libmongoc/examples/cmake/$ex_name"

$example_build = Join-Path $ScratchDir "example-build"
Build-CMakeProject -SourceDir $example -BuildDir $example_build `
    -ConfigureOptions "-DCMAKE_PREFIX_PATH=$install_dir" `
    -Test
