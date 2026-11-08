param([switch]$NoxFocused, [switch]$UnitTests, [switch]$WarningAudit)

$ErrorActionPreference = 'Stop'
$buildDirectory = if ($UnitTests) { 'build-tests' } elseif ($NoxFocused) { 'build-nox-focused' } else { 'build-scummvm' }
$buildLogName = if ($UnitTests) { 'msbuild-tests.log' } elseif ($NoxFocused) { 'msbuild-nox-focused.log' } else { 'msbuild-release.log' }

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = Split-Path -Parent $repoRoot
$repoName = Split-Path -Leaf $repoRoot
$localVcpkgInstalled = Join-Path $repoRoot 'vcpkg_installed'

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
if ($WarningAudit) {
    & 'C:\Users\aaron\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' devtools/ce/prepare_warning_repro.py
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    foreach ($engine in @('sci', 'fool', 'phoenixvr')) {
        & 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' "build-scummvm\acceptance-warning-$engine.vcxproj" /t:ClCompile /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnabled=false /p:VcpkgEnableManifest=false /p:VcpkgManifestInstall=false /v:minimal "/flp:logfile=$repoRoot\warning-repro-$engine.log;verbosity=normal"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    exit 0
}
$solutionName = if ($UnitTests) { 'scummvm-tests' } else { 'scummvm' }
if ($UnitTests) {
    $shimDirectory = Join-Path $repoRoot 'build-python-shim'
    New-Item -ItemType Directory -Path $shimDirectory -Force | Out-Null
    Set-Content (Join-Path $shimDirectory 'python3.cmd') '@"C:\Users\aaron\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" %*'
    $env:Path = "$shimDirectory;$env:Path"
}

$projectNeedsRefresh = -not (Test-Path "$buildDirectory\$solutionName.sln")
if (-not $projectNeedsRefresh) {
    $projectNeedsRefresh = -not (Get-Content "$buildDirectory\$solutionName.vcxproj" -Raw).Contains('<CharacterSet>Unicode</CharacterSet>')
}
if (-not $projectNeedsRefresh) {
    $globalPropsPath = "$buildDirectory\ScummVM_Globalx64.props"
    $projectNeedsRefresh = -not (Test-Path $globalPropsPath)
    if (-not $projectNeedsRefresh) {
        $globalPropsContent = Get-Content -LiteralPath $globalPropsPath -Raw
        $projectNeedsRefresh = (-not $NoxFocused -and -not $globalPropsContent.Contains('ENABLE_MONKEY4')) -or -not $globalPropsContent.Contains('ENABLE_NOX') -or (-not $NoxFocused -and -not $globalPropsContent.Contains('ENABLE_PHOENIXVR')) -or (-not $NoxFocused -and -not $globalPropsContent.Contains('ENABLE_GOOKA'))
    }
}
if (-not $projectNeedsRefresh -and (Test-Path "$buildDirectory\scummvm.vcxproj")) {
    $projectContent = Get-Content -LiteralPath "$buildDirectory\scummvm.vcxproj" -Raw
    $projectNeedsRefresh = -not $projectContent.Contains('..\engines\ce_achievements.cpp') -or -not $projectContent.Contains('..\engines\ce_test_cli.cpp')
}
if (-not $projectNeedsRefresh -and (Test-Path "$buildDirectory\cine.vcxproj")) {
    $cineProjectContent = Get-Content -LiteralPath "$buildDirectory\cine.vcxproj" -Raw
    $projectNeedsRefresh = -not $cineProjectContent.Contains('..\engines\cine\achievements.cpp')
}
if (-not $projectNeedsRefresh -and -not $NoxFocused -and -not $UnitTests) {
    $gookaProjectPath = "$buildDirectory\gooka.vcxproj"
    $projectNeedsRefresh = -not (Test-Path $gookaProjectPath)
    if (-not $projectNeedsRefresh) {
        $projectNeedsRefresh = (Get-Item 'engines\gooka\module.mk').LastWriteTimeUtc -gt (Get-Item $gookaProjectPath).LastWriteTimeUtc
    }
}
if (-not $projectNeedsRefresh) {
    $noxProjectPath = "$buildDirectory\nox.vcxproj"
    $projectNeedsRefresh = -not (Test-Path $noxProjectPath)
    if (-not $projectNeedsRefresh) {
        $noxProjectContent = Get-Content -LiteralPath $noxProjectPath -Raw
        $projectNeedsRefresh = -not $noxProjectContent.Contains('..\engines\nox\session_validation.cpp') -or -not $noxProjectContent.Contains('..\engines\nox\runtime_state.cpp') -or -not $noxProjectContent.Contains('..\engines\nox\enhancements.cpp') -or -not $noxProjectContent.Contains('..\engines\nox\runtime_observers.cpp')
    }
}

if ($projectNeedsRefresh -or $UnitTests) {
    if (-not (Test-Path 'build-create-project')) {
        New-Item -ItemType Directory -Path 'build-create-project' | Out-Null
    }

    cmake -S devtools/create_project/cmake -B build-create-project
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    cmake --build build-create-project --config Release -j 2
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    if (-not (Test-Path $buildDirectory)) {
        New-Item -ItemType Directory -Path $buildDirectory | Out-Null
    }

    Push-Location $buildDirectory
    try {
        if ($UnitTests) {
            ..\build-create-project\Release\create_project.exe .. --msvc --vcpkg --msvc-version 17 --use-windows-unicode --tests
        } elseif ($NoxFocused) {
            ..\build-create-project\Release\create_project.exe .. --msvc --vcpkg --msvc-version 17 --use-windows-unicode --disable-all-engines --enable-engine=nox
        } else {
            ..\build-create-project\Release\create_project.exe .. --msvc --vcpkg --msvc-version 17 --use-windows-unicode --enable-gif --enable-engine=monkey4,nox,gooka
        }
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
	}
}

$propsFiles = @(
	"$buildDirectory\ScummVM_Globalx86.props",
    "$buildDirectory\ScummVMTests_Globalx86.props",
    "$buildDirectory\ScummVMTests_Globalx64.props",
    "$buildDirectory\ScummVMTests_Globalarm64.props",
	"$buildDirectory\ScummVM_Globalx64.props",
	"$buildDirectory\ScummVM_Globalarm64.props"
)

foreach ($propsFile in $propsFiles) {
	if (Test-Path $propsFile) {
		$propsContent = Get-Content -LiteralPath $propsFile -Raw
		$originalPropsContent = $propsContent
		$vcpkgIncludePath = "$substVcpkgInstalled\x64-windows\include"
		$vcpkgLibraryPath = "$substVcpkgInstalled\x64-windows\lib"
		$vcpkgLibraries = 'SDL2.lib;SDL2_net.lib;zlib.lib;mad.lib;fribidi.lib;ogg.lib;vorbis.lib;vorbisfile.lib;FLAC.lib;libpng16.lib;mpeg2.lib;theoradec.lib;theora.lib;freetype.lib;jpeg.lib;libfluidsynth-3.lib;libcurl.lib;brotlidec.lib;brotlicommon.lib;bz2.lib;iconv.lib;intl.lib;charset.lib;glib-2.0.lib;gobject-2.0.lib;gmodule-2.0.lib;gthread-2.0.lib;pcre2-8.lib;ffi.lib;OpenAL32.lib;gif.lib;%(AdditionalDependencies)'
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

		if ($propsContent -cne $originalPropsContent) {
			Set-Content -LiteralPath $propsFile -Value $propsContent -NoNewline
		}
	}
}

& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' "$buildDirectory\$solutionName.sln" /m:1 /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnabled=false /p:VcpkgEnableManifest=false /p:VcpkgManifestInstall=false /p:VcpkgRoot=S:\vcpkg\ /p:VcpkgTriplet=x64-windows /p:VcpkgInstalledDir="$substVcpkgInstalled" /p:PreferredToolArchitecture=x64 /v:normal "/flp:logfile=$repoRoot\$buildLogName;verbosity=detailed"
$buildExitCode = $LASTEXITCODE

if ($buildExitCode -eq 0) {
	$vcpkgBinPath = "$substVcpkgInstalled\x64-windows\bin"
	$outputPath = "$buildDirectory\Releasex64"

	if (Test-Path $vcpkgBinPath) {
		Get-ChildItem -Path "$vcpkgBinPath\*.dll" | Copy-Item -Destination $outputPath -Force
	}
}

if ($UnitTests -and $buildExitCode -eq 0) {
    # The generated upstream post-build ignores test failures; run explicitly.
    Push-Location $buildDirectory
    try {
        & ".\Releasex64\scummvm-tests.exe"
        $buildExitCode = $LASTEXITCODE
    } finally { Pop-Location }
}
if ($UnitTests -and $buildExitCode -eq 0) {
    cmake -S devtools/ce -B build-ce-tests
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build build-ce-tests --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-ce-tests -C Release --output-on-failure
    $buildExitCode = $LASTEXITCODE
}
exit $buildExitCode
