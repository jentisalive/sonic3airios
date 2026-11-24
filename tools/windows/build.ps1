# Run with Visual Studio 2022's C++ desktop workload and MSBuild on PATH.
[CmdletBinding()]
param([string]$OutputDirectory = (Join-Path $PWD 'dist-windows'))

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
$msbuild = (Get-Command MSBuild.exe -CommandType Application).Source

function Build-Solution($Path, $Targets = 'Build', $Configuration = 'Release', $Properties = @()) {
    & $msbuild $Path "/t:$Targets" "/p:Configuration=$Configuration" '/p:Platform=x64' /m /nologo /v:minimal @Properties
    if ($LASTEXITCODE -ne 0) { throw "MSBuild failed for $Path ($LASTEXITCODE)" }
}

function Copy-Headers($Source, $Destination) {
    New-Item -ItemType Directory -Force -Path "framework/include/$Destination" | Out-Null
    Copy-Item $Source "framework/include/$Destination/"
}

function Copy-Libraries($Source, $Destination) {
    New-Item -ItemType Directory -Force -Path "framework/lib/x64/$Destination" | Out-Null
    Copy-Item $Source "framework/lib/x64/$Destination/"
}

Push-Location $root
try {
    $ext = 'framework/external'
    $originalCl = $env:_CL_
    try {
        # The app and its static libraries use the same release CRT.
        $env:_CL_ = "$originalCl /MT".Trim()
        Build-Solution "$ext/sdl/SDL2/VisualC/SDL.sln" 'SDL2;SDL2main'
        Build-Solution "$ext/ogg-vorbis/libogg/win32/VS2015/libogg.sln" libogg
        Build-Solution "$ext/ogg-vorbis/libvorbis/win32/VS2010/vorbis_static.sln" 'libvorbis_static;libvorbisfile'
        Build-Solution "$ext/curl/curl/projects/Windows/VC14.30/curl-all.sln" libcurl 'LIB Release - DLL Windows SSPI' @('/p:RuntimeLibrary=MT_StaticRelease')
    } finally {
        $env:_CL_ = $originalCl
    }
    Build-Solution "$ext/zlib/_vstudio/zlib.sln"
    Copy-Headers "$ext/sdl/SDL2/include/*.h" sdl
    Copy-Libraries "$ext/sdl/SDL2/VisualC/x64/Release/*.lib" sdl
    Build-Solution "$ext/imgui/_vstudio/imgui.sln"

    Copy-Headers "$ext/ogg-vorbis/libogg/include/ogg/*.h" ogg
    Copy-Headers "$ext/ogg-vorbis/libvorbis/include/vorbis/*.h" vorbis
    Copy-Headers "$ext/curl/curl/include/curl/*.h" curl
    Copy-Headers "$ext/zlib/zlib/*.h" zlib
    Copy-Headers "$ext/zlib/zlib/contrib/minizip/*.h" minizip
    Copy-Headers "$ext/imgui/imgui/*.h" imgui
    Copy-Headers "$ext/imgui/imgui/backends/*.h" 'imgui/backends'

    Copy-Libraries "$ext/ogg-vorbis/libogg/win32/VS2015/x64/Release/libogg.lib" libogg
    Copy-Libraries "$ext/ogg-vorbis/libvorbis/win32/VS2010/x64/Release/*.lib" libvorbis
    Copy-Libraries "$ext/curl/curl/build/Win64/VC14.30/LIB Release - DLL Windows SSPI/libcurl.lib" curl
    Copy-Libraries "$ext/zlib/lib/Release_x64/zlib.lib" zlib
    Copy-Libraries "$ext/zlib/lib/Release_x64/minizip.lib" minizip
    Copy-Libraries "$ext/imgui/lib/Release_x64/imgui.lib" imgui

    $game = 'Oxygen/sonic3air'
    Build-Solution "$game/build/_vstudio/sonic3air.sln"

    # Match the engine's loose data layout and keep the scripts used by this build.
    # Stage the data and scripts separately from the build and save folders.
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $stage = Join-Path ([IO.Path]::GetTempPath()) "Sonic3AIR-$([Guid]::NewGuid())"
    New-Item -ItemType Directory -Path $stage | Out-Null
    try {
        Copy-Item "$game/bin/Release_x64/Sonic3AIR.exe" $stage
        Copy-Item "$game/source/external/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" $stage
        Copy-Item "$game/data", "$game/scripts" $stage -Recurse
        Copy-Item 'Oxygen/oxygenengine/data/*' "$stage/data/" -Recurse -Force
        Copy-Item "$game/_master_image_template/config.json", "$game/_master_image_template/Manual.pdf" $stage
        Copy-Item LICENSE "$stage/LICENSE.txt"
        Copy-Item "$PSScriptRoot/README.md" "$stage/README-Windows.md"

        # The existing portable-mode marker keeps this build's save states and
        # settings separate from an existing upstream Sonic 3 A.I.R. install.
        New-Item -ItemType Directory -Path "$stage/savedata" | Out-Null
        Set-Content "$stage/savedata/README.txt" 'Settings, saves, mods and replays are stored in this folder.'

        foreach ($file in @('data/metadata.json', 'data/oxygen_icon.png', 'data/images/icon.png', 'scripts/main.lemon')) {
            if (!(Test-Path "$stage/$file" -PathType Leaf)) { throw "Missing game resource: $file" }
        }
        # Exercise the Windows loader and the executable entry point with no ROM.
        $process = Start-Process "$stage/Sonic3AIR.exe" '-stop' -WorkingDirectory $stage -PassThru
        if (!$process.WaitForExit(20000)) {
            $process.Kill()
            throw 'Sonic3AIR.exe -stop timed out'
        }
        if ($process.ExitCode -ne 0) { throw "Sonic3AIR.exe could not start ($($process.ExitCode))" }

        $zip = Join-Path $output 'Sonic3AIR-Windows-x64.zip'
        Compress-Archive -Path "$stage/*" -DestinationPath $zip -Force
        Write-Host "Created $zip"
    } finally {
        Remove-Item $stage -Recurse -Force
    }
} finally {
    Pop-Location
}
