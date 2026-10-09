$ErrorActionPreference = 'Stop'

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

$projectNeedsRefresh = -not (Test-Path 'build-scummvm\scummvm.sln')
if (-not $projectNeedsRefresh -and (Test-Path 'build-scummvm\scummvm.vcxproj')) {
    $projectContent = Get-Content -LiteralPath 'build-scummvm\scummvm.vcxproj' -Raw
    $projectNeedsRefresh = -not $projectContent.Contains('..\engines\ce_achievements.cpp')
}
if (-not $projectNeedsRefresh -and (Test-Path 'build-scummvm\cine.vcxproj')) {
    $cineProjectContent = Get-Content -LiteralPath 'build-scummvm\cine.vcxproj' -Raw
    $projectNeedsRefresh = -not $cineProjectContent.Contains('..\engines\cine\achievements.cpp')
}

if ($projectNeedsRefresh) {
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

    if (-not (Test-Path 'build-scummvm')) {
        New-Item -ItemType Directory -Path 'build-scummvm' | Out-Null
    }

    Push-Location 'build-scummvm'
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
	'build-scummvm\ScummVM_Globalx86.props',
	'build-scummvm\ScummVM_Globalx64.props',
	'build-scummvm\ScummVM_Globalarm64.props'
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

& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe' build-scummvm\scummvm.sln /m:1 /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnableManifest=true /p:VcpkgTriplet=x64-windows /p:VcpkgInstalledDir="$substVcpkgInstalled" /p:PreferredToolArchitecture=x64 /v:normal "/flp:logfile=$repoRoot\msbuild-release.log;verbosity=detailed"
$buildExitCode = $LASTEXITCODE

if ($buildExitCode -eq 0) {
	$vcpkgBinPath = "$substVcpkgInstalled\x64-windows\bin"
	$outputPath = 'build-scummvm\Releasex64'

	if (Test-Path $vcpkgBinPath) {
		Get-ChildItem -Path "$vcpkgBinPath\*.dll" | Copy-Item -Destination $outputPath -Force
	}
}

exit $buildExitCode
