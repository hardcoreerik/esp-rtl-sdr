param([Parameter(Mandatory)][string]$Label)
$ErrorActionPreference='Stop'
$lab='C:\Tools\esp-rtl-sdr-lab\captures\nooelec-v5-bandsweep-20260930'
$out="C:\Tools\esp-rtl-sdr-lab\captures\bandsweep-$Label-20261001"; New-Item -ItemType Directory -Force $out | Out-Null
$ts='C:\Program Files\Wireshark\tshark.exe'
# 1. find the USBPcap interface/bus/address the dongle is on
$found=$null
foreach($n in 1..4){
 $p=Start-Process $ts -ArgumentList '-i',"\\.\USBPcap$n",'-a','duration:6','-w',"$out\pre$n.pcapng" -PassThru -WindowStyle Hidden
 Start-Sleep 2; python "$lab\..\nooelec-v5-bandsweep-20260930\open1.py" *>$null; $p.WaitForExit()
 $g=& $ts -r "$out\pre$n.pcapng" -Y 'usb.bmRequestType==0x40 && usb.setup.wIndex==0x0610' -T fields -e usb.bus_id -e usb.device_address 2>$null | sort | group | sort Count -desc | select -first 1
 if($g){ $found=@{n=$n;ba=($g.Name -split '\s+')}; break }
}
if(-not $found){ throw 'no tuner traffic on any USBPcap interface' }
"interface USBPcap$($found.n) bus $($found.ba[0]) addr $($found.ba[1])"
# 2. real capture
$p=Start-Process $ts -ArgumentList '-i',"\\.\USBPcap$($found.n)",'-w',"$out\bandsweep.pcapng" -PassThru -WindowStyle Hidden
Start-Sleep 3; python "$lab\band_sweep_stimulus.py" $out *> "$out\stim.log"; $stimExit=$LASTEXITCODE; Start-Sleep 2; Stop-Process $p.Id; Start-Sleep 1
if($stimExit -ne 0){ throw "stimulus failed with exit code $stimExit (see $out\stim.log)" }
python "$lab\extract_bands_any.py" "$out\bandsweep.pcapng" "$out\events.json" $found.ba[0] $found.ba[1] "$out\band_registers.csv"
if($LASTEXITCODE -ne 0){ throw "extraction failed with exit code $LASTEXITCODE" }
Get-FileHash "$out\bandsweep.pcapng","$out\events.json","$out\band_registers.csv" | % { "$($_.Hash) $(Split-Path $_.Path -Leaf)" }
