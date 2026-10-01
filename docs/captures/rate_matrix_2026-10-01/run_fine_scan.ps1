param([Parameter(Mandatory)][string]$Label,[int]$Start=30000000,[int]$Stop=700000000,[int]$Step=250000,[int]$Rate=2048000,[Parameter(Mandatory)][int]$N,[Parameter(Mandatory)][string]$Bus,[Parameter(Mandatory)][string]$Addr)
$ts='C:\Program Files\Wireshark\tshark.exe'; $out="C:\Tools\esp-rtl-sdr-lab\captures\finescan-$Label-$Start-$Step-r$Rate"; New-Item -ItemType Directory -Force $out | Out-Null
$p=Start-Process $ts -ArgumentList '-i',"\\.\USBPcap$N",'-w',"$out\scan.pcapng" -PassThru -WindowStyle Hidden
Start-Sleep 3; python C:\Tools\esp-rtl-sdr-lab\captures\fine_scan_stimulus.py $out $Start $Stop $Step $Rate *> "$out\stim.log"; Start-Sleep 2; Stop-Process $p.Id; Start-Sleep 1
python C:\Tools\esp-rtl-sdr-lab\captures\nooelec-v5-bandsweep-20260930\extract_bands_any.py "$out\scan.pcapng" "$out\events.json" $Bus $Addr "$out\scan.csv"
Get-Item "$out\scan.pcapng" | % Length

