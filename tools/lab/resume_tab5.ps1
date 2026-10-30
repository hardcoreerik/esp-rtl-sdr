param([Parameter(Mandatory)][string]$ConfigPath)
$ErrorActionPreference = 'Stop'
$config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
if ($config.port -ne 'COM39') { throw 'Only the enrolled COM39 test device may be resumed' }
$log = Join-Path $config.output 'resume-status.log'
& $config.runner -Port $config.port -PairingKeyPath $config.key -SendMaxSeconds 20 `
  -SendCommand @('RTL_TUNE LORA 906875000','RTL_DRIVER GAIN 0','RTL_DRIVER GAINMODE AUTO',
    'RTL_DRIVER RTLAGC OFF','RTL_LORA_GAIN GUARD ON','RTL_UI OPEN LORA','RTL_DRIVER STATUS','RTL_LORA_GAIN STATUS') `
  -LogPath $log *> (Join-Path $config.output 'resume-console.log')
if ($LASTEXITCODE -ne 0) { throw 'Application resume command failed' }
$text = [IO.File]::ReadAllText($log)
$line = ($text -split '\r?\n' | Where-Object { $_ -match '^RTL_DRIVER_STATUS ' } | Select-Object -Last 1)
if (!$line -or $line -notmatch 'state=STREAMING ' -or $line -notmatch 'mode=AUTO gain_tenth_db=0 ' -or
    $line -notmatch 'frequency_hz=906875000 ' -or $line -notmatch 'effective_sps=[1-9][0-9]* ' -or
    $line -notmatch 'bytes=[1-9][0-9]* ' -or $line -notmatch 'rtl_agc=0 ' -or
    $line -notmatch 'overruns=0 drops=0 shadow_ok=1 metrics_ok=1 ' -or
    $text -notmatch 'RTL_LORA_GAIN_STATUS guard=on ') { throw 'Receiver restoration not verified' }
