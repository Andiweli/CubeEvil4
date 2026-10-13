param([string]$Generator = 'Ninja', [string]$BuildName = 'host')
$ErrorActionPreference = 'Stop'
$taskPortRoot = Split-Path -Parent $PSScriptRoot
if ($BuildName -notmatch '^[A-Za-z0-9_-]+$') { throw 'BuildName must be a simple directory name.' }
$taskBuild = Join-Path $taskPortRoot "build\$BuildName"
& cmake -S $taskPortRoot -B $taskBuild -G $Generator -DCMAKE_BUILD_TYPE=Release -DRE4_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'Host CMake configure failed. Initialize the compiler environment first.' }
& cmake --build $taskBuild --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
& ctest --test-dir $taskBuild -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Host verification failed.' }
