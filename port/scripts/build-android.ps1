param(
    [string]$SdkRoot = $env:ANDROID_HOME,
    [string]$JavaHome = $env:JAVA_HOME,
    [string]$GradleUserHome,
    [string]$Python,
    [string]$ValidationLayers,
    [switch]$Offline,
    [switch]$NativeOnly
)
$ErrorActionPreference = 'Stop'
$taskPortRoot = Split-Path -Parent $PSScriptRoot
if (-not $SdkRoot) { $SdkRoot = $env:ANDROID_SDK_ROOT }
if (-not $SdkRoot -and $env:LOCALAPPDATA) {
    $SdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
}
if (-not $SdkRoot -or -not (Test-Path -LiteralPath $SdkRoot)) {
    throw 'Android SDK missing. Pass -SdkRoot or set ANDROID_HOME.'
}
$taskNdk = Join-Path $SdkRoot 'ndk\28.2.13676358'
$taskCmake = Join-Path $SdkRoot 'cmake\4.1.2\bin\cmake.exe'
$taskNinja = Join-Path $SdkRoot 'cmake\4.1.2\bin\ninja.exe'
foreach ($taskRequired in @($taskCmake, $taskNinja,
    (Join-Path $taskNdk 'build\cmake\android.toolchain.cmake'))) {
    if (-not (Test-Path -LiteralPath $taskRequired)) {
        throw "Required pinned Android build tool missing: $taskRequired"
    }
}

$taskPythonArgs = @()
if ($Python) {
    if (-not (Test-Path -LiteralPath $Python)) { throw "Python interpreter missing: $Python" }
    $taskPythonArgs = @("-DPython3_EXECUTABLE=$Python")
}
foreach ($taskAbi in @('armeabi-v7a', 'arm64-v8a')) {
    $taskBuild = Join-Path $taskPortRoot "build\android-$taskAbi"
    & $taskCmake -S $taskPortRoot -B $taskBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$taskNinja" `
        "-DCMAKE_TOOLCHAIN_FILE=$taskNdk\build\cmake\android.toolchain.cmake" `
        "-DANDROID_ABI=$taskAbi" -DANDROID_PLATFORM=android-21 `
        -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release -DRE4_BUILD_TESTS=ON @taskPythonArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for $taskAbi" }
    & $taskCmake --build $taskBuild --parallel
    if ($LASTEXITCODE -ne 0) { throw "Native build failed for $taskAbi" }
}
if ($NativeOnly) { return }

if (-not $JavaHome -or -not (Test-Path -LiteralPath (Join-Path $JavaHome 'bin\java.exe'))) {
    throw 'JDK 17-23 required by Gradle 8.13. Pass -JavaHome or set JAVA_HOME.'
}
$taskJavaVersion = (& (Join-Path $JavaHome 'bin\java.exe') -version 2>&1 | Out-String)
if ($taskJavaVersion -notmatch 'version "(\d+)\.') {
    throw "Cannot determine Java version: $taskJavaVersion"
}
$taskJavaMajor = [int]$Matches[1]
if ($taskJavaMajor -lt 17 -or $taskJavaMajor -gt 23) {
    throw "JDK $taskJavaMajor is outside Gradle 8.13's supported 17-23 build range."
}
$env:JAVA_HOME = $JavaHome
$env:ANDROID_HOME = $SdkRoot
if ($GradleUserHome) { $env:GRADLE_USER_HOME = $GradleUserHome }
$taskGradleArgs = @('-p', (Join-Path $taskPortRoot 'android'), '--no-daemon')
if ($Offline) { $taskGradleArgs += '--offline' }
if ($Python) { $taskGradleArgs += "-Pre4.python=$Python" }
if ($ValidationLayers) { $taskGradleArgs += "-Pre4.validationLayers=$ValidationLayers" }
& (Join-Path $taskPortRoot 'android\gradlew.bat') @taskGradleArgs `
    ':app:assembleDebug' ':app:assembleDebugAndroidTest' ':app:lintDebug'
if ($LASTEXITCODE -ne 0) { throw 'Android Gradle build/tests packaging/lint failed.' }
