$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$source = Join-Path $root 'source/xrd/xrdshaders.h'
$output = Join-Path $root 'source/resources'
$cache = Join-Path $root 'build/droplet-shaders'
$compiler = Join-Path $PSScriptRoot 'x86/fxc.exe'

# Projects share these embedded resources. Serialize generation so a parallel
# solution build cannot feed the resource compiler a partially written shader.
function FileHash([string]$file) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToBase64String($sha.ComputeHash([IO.File]::ReadAllBytes($file))) }
    finally { $sha.Dispose() }
}
$key = (FileHash $source) + (FileHash $PSCommandPath) + (FileHash $compiler)
$mutex = New-Object System.Threading.Mutex($false, 'Local\XboxRainDropletsShaderBuild')
$locked = $false
try {
    try { $locked = $mutex.WaitOne(120000) }
    catch [System.Threading.AbandonedMutexException] { $locked = $true }
    if (!$locked) { throw 'Timed out waiting for the droplet shader build.' }
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    $stamp = Join-Path $cache 'inputs.txt'
    $jobs = @(
        @('D3D9Source', 'PSMain', 'ps_3_0', 'dropPS9'),
        @('D3D9LightSource', 'PSMain', 'ps_3_0', 'lightPS9'),
        @('D3D11Source', 'VSMain', 'vs_4_0', 'dropVS10'),
        @('D3D11Source', 'PSMain', 'ps_4_0', 'dropPS10'),
        @('D3D11Source', 'VSMain', 'vs_5_0', 'dropVS12'),
        @('D3D11Source', 'PSMain', 'ps_5_0', 'dropPS12')
    )
    $missing = @($jobs | Where-Object { !(Test-Path -LiteralPath (Join-Path $output ($_[3] + '.cso'))) })
    if (!$missing.Count -and (Test-Path -LiteralPath $stamp) -and [IO.File]::ReadAllText($stamp) -eq $key) { return }
    $text = [IO.File]::ReadAllText($source)
    foreach ($job in $jobs) {
        $match = [regex]::Match($text, '(?s)\b' + $job[0] + '\s*=\s*R"\((.*?)\)";')
        if (!$match.Success) { throw "Shader source not found: $($job[0])" }
        $hlsl = Join-Path $cache ($job[3] + '.hlsl')
        $binary = Join-Path $cache ($job[3] + '.cso')
        [IO.File]::WriteAllText($hlsl, $match.Groups[1].Value)
        & $compiler /nologo /O3 /T $job[2] /E $job[1] /Fo $binary $hlsl
        if ($LASTEXITCODE -ne 0) { throw "Failed to compile $($job[3])" }
    }
    # Publish only after every profile compiled successfully.
    foreach ($job in $jobs) {
        $binary = Join-Path $cache ($job[3] + '.cso')
        $destination = Join-Path $output ($job[3] + '.cso')
        if (!(Test-Path -LiteralPath $destination) -or (FileHash $binary) -ne (FileHash $destination)) {
            [IO.File]::Copy($binary, $destination, $true)
        }
    }
    [IO.File]::WriteAllText($stamp, $key)
}
finally {
    if ($locked) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
