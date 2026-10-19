$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
cmake -S . -B build/ci -A x64 -DMULTISQUEEZE_BUILD_TESTS=ON -DMULTISQUEEZE_LAYOUT_TUNER=OFF
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build build/ci --config Release --target MultiSqueeze_VST3 MultiSqueeze_Standalone MultiSqueezeTests MultiSqueezeProcessorTests --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
ctest --test-dir build/ci -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
$versionLine = Get-Content build/ci/CMakeCache.txt | Where-Object { $_ -match '^CMAKE_PROJECT_VERSION:STATIC=' }
$version = ($versionLine -split '=', 2)[1]
$source = (Resolve-Path 'build/ci/MultiSqueeze_artefacts/Release').Path
$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
if (!(Test-Path $iscc)) { throw 'Inno Setup 6 is required' }
New-Item -ItemType Directory -Force dist | Out-Null
$destination = (Resolve-Path dist).Path
& $iscc "/DAppVersion=$version" "/DSourceRoot=$source" "/O$destination" packaging/windows/MultiSqueeze.iss
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed' }
if (!(Test-Path dist/installer.exe) -or (Get-Item dist/installer.exe).Length -eq 0) { throw 'Installer missing' }
