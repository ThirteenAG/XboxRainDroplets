param(
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$Validator
)
# The settings description of one plugin (ASI Settings, external/FusionFixUtils/asi_settings): the
# description every plugin shares, source/settings/XboxRainDroplets.json, with what
# source/settings/games.json says about the plugin - its games, the optional settings of
# extensions.xrd.optional it reads, the shared ones it leaves out, and its own defaults - into
# build/settings/<project>.json, which source/resources/Settings.rc embeds. The file is
# only written when it changed, so the resources are not compiled again for nothing. With
# -Validator, the description and its English text are checked by asi-settings-validate
# and a broken description fails the build.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$settingsRoot = Join-Path $root 'source/settings'
$output = Join-Path $root "build/settings/$Project.json"
$utf8 = New-Object System.Text.UTF8Encoding($false)

function ReadJson([string]$file) {
    return [IO.File]::ReadAllText($file, $utf8) | ConvertFrom-Json
}

$description = ReadJson (Join-Path $settingsRoot 'XboxRainDroplets.json')
$games = ReadJson (Join-Path $settingsRoot 'games.json')
$game = $games.$Project
if ($null -eq $game) { throw "source/settings/games.json has no entry for $Project" }

# The id stays the project's name whatever the file is renamed to
$description.plugin | Add-Member -NotePropertyName id -NotePropertyValue $Project -Force

# The games: none for the wrapper, which works with any game
if ($game.games) {
    $description.plugin | Add-Member -NotePropertyName games -NotePropertyValue @($game.games)
}

# The optional settings the plugin reads, after the shared ones
$optional = @($description.extensions.xrd.optional)
$settings = [System.Collections.ArrayList]@($description.settings)
foreach ($id in @($game.settings)) {
    if (!$id) { continue }
    $setting = $optional | Where-Object { $_.id -eq $id }
    if (!$setting) { throw "$Project names $id, which isn't in extensions.xrd.optional" }
    [void]$settings.Add($setting)
}
# The shared settings the plugin doesn't use (the game decides them itself)
foreach ($id in @($game.exclude)) {
    if (!$id) { continue }
    $setting = $settings | Where-Object { $_.id -eq $id }
    if (!$setting) { throw "$Project excludes $id, which it doesn't have" }
    $settings.Remove($setting)
}
$description.settings = @($settings)
$description.PSObject.Properties.Remove('extensions')

# The plugin's own defaults; the recommended preset is the default
if ($game.defaults) {
    foreach ($property in $game.defaults.PSObject.Properties) {
        $setting = $description.settings | Where-Object { $_.id -eq $property.Name }
        if (!$setting) { throw "$Project has a default for $($property.Name), which it doesn't have" }
        $setting.default = $property.Value
        if ($setting.presets) { $setting.presets.recommended = $property.Value }
    }
}

$text = (ConvertTo-Json -InputObject $description -Depth 32).Replace("`r`n", "`n").Replace("`n", "`r`n") + "`r`n"
$previous = if (Test-Path -LiteralPath $output) { [IO.File]::ReadAllText($output, $utf8) } else { $null }
if ($text -ne $previous) {
    [IO.Directory]::CreateDirectory((Split-Path $output -Parent)) | Out-Null
    [IO.File]::WriteAllText($output, $text, $utf8)
    Write-Host "BuildSettings: wrote $output"
}

if ($Validator) {
    # Built by the XboxRainDropletsTools solution; without it the description isn't checked
    if (!(Test-Path -LiteralPath $Validator)) {
        Write-Host "BuildSettings: $Validator isn't built, the description of $Project isn't checked"
        exit 0
    }
    & $Validator $output @(Get-ChildItem -LiteralPath (Join-Path $settingsRoot 'text') -Filter *.json | ForEach-Object { $_.FullName })
    if ($LASTEXITCODE -ne 0) { throw "the settings description of $Project is broken" }
}
