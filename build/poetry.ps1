$ErrorActionPreference = 'Stop'

# The caching directory
$mcd_dir = Join-Path $env:LOCALAPPDATA "mongo-c-build"
New-Item $mcd_dir -ItemType Container -Force | Out-Null

# Setup paths and poetry paths
$poetry_dir = Join-Path $mcd_dir "poetry"
$env:POETRY_HOME = $poetry_dir
$env:Path = "$poetry_dir\bin;$env:Path"

# Try to get poetry
$poetry = Join-Path $poetry_dir "bin\poetry.exe"

if (! (Test-Path $poetry)) {
    $py = Get-Command py -CommandType Application
    Write-Host "Note: Installing Poetry"
    $get_poetry_py = "$mcd_dir/get-poetry.py"
    Invoke-WebRequest -UseBasicParsing -Uri https://install.python-poetry.org -OutFile $get_poetry_py `
    | Out-Null
    $env:POETRY_HOME = "$mcd_dir/poetry"
    & $py.Source -u $get_poetry_py --yes
    if ($LASTEXITCODE) {
        throw "Poetry install failed [$LASTEXITCODE]"
    }
}

if (!(Test-Path $poetry)) {
    throw "Failed to install Poetry"
}

& $poetry @args

exit $LASTEXITCODE
