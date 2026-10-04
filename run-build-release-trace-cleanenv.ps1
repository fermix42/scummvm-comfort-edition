$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = Split-Path -Parent $repoRoot
$repoName = Split-Path -Leaf $repoRoot
$localVcpkgInstalled = Join-Path $repoRoot 'vcpkg_installed'
$traceBuildDir = 'build-scummvm-trace'
$traceOutputPath = Join-Path $traceBuildDir 'Releasex64'

if (-not (Test-Path 'S:\')) {
    cmd /c "subst S: `"$workspaceRoot`""
}

$substRepoRoot = "S:\$repoName"
$substVcpkgInstalled = if (Test-Path $localVcpkgInstalled) {
    "$substRepoRoot\vcpkg_installed"
} else {
    'S:\scummvm-private-feature-usable-desktop-window-layout\vcpkg_installed'
}

$pathValue = @(
    'C:\Users\aaron\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell',
    'S:\vcpkg\downloads\tools\7zip-24.09-windows',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja',
    [Environment]::GetEnvironmentVariable('Path', 'Machine'),
    [Environment]::GetEnvironmentVariable('Path', 'User')
) -join ';'

Get-ChildItem Env: | Where-Object { $_.Name -ceq 'PATH' -or $_.Name -ceq 'Path' } | Remove-Item
$env:Path = $pathValue
$env:VCPKG_ROOT = 'S:\vcpkg'
$env:VCPKG_DEFAULT_TRIPLET = 'x64-windows'
$env:VCPKG_INSTALLED_DIR = $substVcpkgInstalled
$env:VCPKG_OVERLAY_TRIPLETS = "$substRepoRoot\.github\windows-layout-triplets"
$env:VCPKG_OVERLAY_PORTS = "$substRepoRoot\.github\vcpkg-ports"

Set-Location $substRepoRoot

if (-not (Test-Path 'build-create-project')) {
    New-Item -ItemType Directory -Path 'build-create-project' | Out-Null
}

if (-not (Test-Path 'build-create-project\Release\create_project.exe')) {
    cmake -S devtools/create_project/cmake -B build-create-project
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    cmake --build build-create-project --config Release -j 2
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path "$traceBuildDir\scummvm.sln")) {
    if (-not (Test-Path $traceBuildDir)) {
        New-Item -ItemType Directory -Path $traceBuildDir | Out-Null
    }

    Push-Location $traceBuildDir
    try {
        ..\build-create-project\Release\create_project.exe .. --msvc --vcpkg --msvc-version 17
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
}

$propsFiles = @(
    "$traceBuildDir\ScummVM_Globalx86.props",
    "$traceBuildDir\ScummVM_Globalx64.props",
    "$traceBuildDir\ScummVM_Globalarm64.props"
)

foreach ($propsFile in $propsFiles) {
    if (Test-Path $propsFile) {
        $propsContent = Get-Content -LiteralPath $propsFile -Raw
        $vcpkgIncludePath = "$substVcpkgInstalled\x64-windows\include"
        $vcpkgLibraryPath = "$substVcpkgInstalled\x64-windows\lib"
        $vcpkgLibraries = 'SDL2.lib;SDL2_net.lib;zlib.lib;mad.lib;fribidi.lib;ogg.lib;vorbis.lib;vorbisfile.lib;FLAC.lib;libpng16.lib;mpeg2.lib;theoradec.lib;theora.lib;freetype.lib;jpeg.lib;libfluidsynth-3.lib;libcurl.lib;brotlidec.lib;brotlicommon.lib;bz2.lib;iconv.lib;intl.lib;charset.lib;glib-2.0.lib;gobject-2.0.lib;gmodule-2.0.lib;gthread-2.0.lib;pcre2-8.lib;ffi.lib;OpenAL32.lib;%(AdditionalDependencies)'
        $oldIncludePath = '$(_ZVcpkgCurrentInstalledDir)include\SDL2%(AdditionalIncludeDirectories)'
        $newIncludePath = "$vcpkgIncludePath;$vcpkgIncludePath\SDL2;%(AdditionalIncludeDirectories)"
        $macroIncludePath = '$(_ZVcpkgCurrentInstalledDir)include;$(_ZVcpkgCurrentInstalledDir)include\SDL2%(AdditionalIncludeDirectories)'

        if ($propsContent.Contains($oldIncludePath) -and -not $propsContent.Contains($newIncludePath)) {
            $propsContent = $propsContent.Replace($oldIncludePath, $newIncludePath)
        } elseif ($propsContent.Contains($macroIncludePath) -and -not $propsContent.Contains($newIncludePath)) {
            $propsContent = $propsContent.Replace($macroIncludePath, $newIncludePath)
        }

        if (-not $propsContent.Contains('<AdditionalLibraryDirectories>')) {
            $linkStart = "`t`t<Link>`r`n"
            $linkSettings = "`t`t<Link>`r`n`t`t`t<AdditionalLibraryDirectories>$vcpkgLibraryPath;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>`r`n`t`t`t<AdditionalDependencies>$vcpkgLibraries</AdditionalDependencies>`r`n"
            $propsContent = $propsContent.Replace($linkStart, $linkSettings)
        }

        Set-Content -LiteralPath $propsFile -Value $propsContent -NoNewline
    }
}

$releaseProps = "$traceBuildDir\ScummVM_Releasex64.props"
if (Test-Path $releaseProps) {
    $releasePropsContent = Get-Content -LiteralPath $releaseProps -Raw
    if (-not $releasePropsContent.Contains('CINE_TRACE_BUILD')) {
        $releasePropsContent = $releasePropsContent.Replace(
            '<PreprocessorDefinitions>',
            '<PreprocessorDefinitions>CINE_TRACE_BUILD;DUMP_SCRIPTS;'
        )
    }
    if (-not $releasePropsContent.Contains('MACVENTURE_TRACE_BUILD')) {
        $releasePropsContent = $releasePropsContent.Replace(
            '<PreprocessorDefinitions>',
            '<PreprocessorDefinitions>MACVENTURE_TRACE_BUILD;'
        )
    }
    Set-Content -LiteralPath $releaseProps -Value $releasePropsContent -NoNewline
}

& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' "$traceBuildDir\scummvm.sln" /m:1 /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnableManifest=true /p:VcpkgTriplet=x64-windows /p:VcpkgInstalledDir="$substVcpkgInstalled" /p:PreferredToolArchitecture=x64 /v:normal "/flp:logfile=$repoRoot\msbuild-release-trace.log;verbosity=detailed"
$buildExitCode = $LASTEXITCODE

if ($buildExitCode -eq 0) {
    $vcpkgBinPath = "$substVcpkgInstalled\x64-windows\bin"

    if (Test-Path $vcpkgBinPath) {
        Get-ChildItem -Path "$vcpkgBinPath\*.dll" | Copy-Item -Destination $traceOutputPath -Force
    }

    Copy-Item -LiteralPath "$traceOutputPath\scummvm.exe" -Destination "$traceOutputPath\scummvm-trace.exe" -Force

    $hashInputs = @(
        'engines\cine\cine.h',
        'engines\cine\prc.cpp',
        'engines\cine\rel.cpp',
        'engines\cine\script.h',
        'engines\cine\script_fw.cpp',
        'engines\cine\object.cpp',
        'engines\cine\various.cpp',
        'engines\macventure\macventure.h',
        'engines\macventure\macventure.cpp',
        'engines\macventure\script.h',
        'engines\macventure\script.cpp',
        'engines\macventure\world.h',
        'engines\macventure\world.cpp'
    )
    $manifestPath = "$traceOutputPath\trace-build-source-hashes.txt"
    $manifest = @()
    $manifest += "trace_exe=$substRepoRoot\$traceOutputPath\scummvm-trace.exe"
    $manifest += "built_at=$((Get-Date).ToString('s'))"
    $manifest += "git_head=$(git rev-parse HEAD)"
    $manifest += "defines=CINE_TRACE_BUILD;DUMP_SCRIPTS;MACVENTURE_TRACE_BUILD"
    $manifest += ''
    foreach ($hashInput in $hashInputs) {
        if (Test-Path $hashInput) {
            $hash = Get-FileHash -LiteralPath $hashInput -Algorithm SHA256
            $manifest += "$($hash.Hash)  $hashInput"
        }
    }
    Set-Content -LiteralPath $manifestPath -Value ($manifest -join "`r`n")

    @(
        'Trace build for Operation Stealth/Cine and Deja Vu/MacVenture diagnostics.',
        '',
        'Pin scummvm-trace.exe, not scummvm.exe, if you want the diagnostic build.',
        'It writes cine-trace.log, macventure-trace.log, and DUMP_SCRIPTS text files into the process working directory.',
        'MacVenture tracing records script opcodes, script ids/offsets, branches, calls, random rolls, object/global writes, text, sound, and object queue activity.',
        'For ScummVM command-line script dumps, use -u/--dump-scripts after creating/choosing a writable working directory.'
    ) | Set-Content -LiteralPath "$traceOutputPath\TRACE_BUILD_README.txt"
}

exit $buildExitCode
