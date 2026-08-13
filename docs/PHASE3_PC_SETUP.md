# Phase 3 PC setup — gain / bias USB capture

**Goal:** Run issue [#1](https://github.com/hardcoreerik/esp-rtl-sdr/issues/1) captures on a Windows desk PC.  
**Authority procedure:** [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md).  
**This machine’s tool tree:** `C:\Tools\esp-rtl-sdr-lab\` (see that folder’s README).

---

## What plugs where

| Device | Where | Why |
|---|---|---|
| **RTL-SDR Blog V4** | **This PC** USB host | USBPcap sees control traffic |
| ESP32-P4 | Optional later | Re-verify CAP after we implement from captures |
| TinySA / multimeter | Optional same session | Relative RF / bias DC |

Yes: for #1 the dongle goes into **this PC**, not the P4.

---

## Software roles

| Need | Tool | Install method on this PC |
|---|---|---|
| USB sniffer | Wireshark + **USBPcap** | winget |
| Packet stack | Npcap | installer |
| Bias ON/OFF | `rtl_biast` (Blog V4 release) | `C:\Tools\esp-rtl-sdr-lab\bin` |
| Device present? | `rtl_test` | same |
| Gain steps UI | SDR# and/or SDR++ | winget |
| Driver fix if open fails | Zadig → WinUSB on interface 0 | winget |

Desktop RTL software is **stimulus only**. Never copy EP0 tables from librtlsdr source into this repo.

---

## PATH (any shell)

User PATH includes:

- `C:\Tools\esp-rtl-sdr-lab\bin`
- `C:\Program Files\Wireshark`

Or:

```bat
C:\Tools\esp-rtl-sdr-lab\cmd\labenv.cmd
```

Verify (new shell after reboot):

```powershell
where.exe rtl_biast,rtl_test,tshark,wireshark
rtl_biast -h
tshark -D
```

---

## One-time after install

1. **Reboot** so USBPcap loads.  
2. Plug Blog V4 into PC.  
3. If `rtl_test` fails to open:

   - Run **Zadig** as admin  
   - Options → List All Devices  
   - Select Bulk-In, Interface 0 (or “RTL2838…”)  
   - Install **WinUSB**  
   - Retry `rtl_test -t`

4. In Wireshark, capture interfaces should list **USBPcapN** after reboot.

---

## Session recipe — bias capture

1. Close any app using the dongle.  
2. Wireshark → capture on the **USBPcap** device for that bus/port.  
3. Start capture **before** opening the dongle.  
4. In a shell:

   ```powershell
   rtl_biast -d 0 -b 1
   Start-Sleep -Seconds 2
   rtl_biast -d 0 -b 0
   ```

5. Stop capture → save  
   `C:\Tools\esp-rtl-sdr-lab\captures\bias_on_off.pcapng`  
6. Multimeter: SMA center vs shield with bias ON (~4.5 V class if product DS).  
7. Fill NOTES (serial, software versions, SHA-256) per [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md).

## Session recipe — gain capture

1. Wireshark USBPcap capture running first.  
2. Open **SDR#** or **SDR++**, select RTL-SDR, fixed LO (e.g. 100 MHz), fixed rate.  
3. Switch AGC off; step manual gain across several values; note UI dB ↔ time.  
4. Save `gain_steps.pcapng` + NOTES.

---

## Evidence package → repo

Preferred public layout (when ready):

```text
docs/captures/
  bias_on_off.pcapng   # or hash + private path if large
  gain_steps.pcapng
  NOTES.md             # SHA-256, serial (redact mid), tool versions
```

Then implement profile EP0 from **measured** control URBs only; enable CAP bits last.

---

## What “ready for #1” means

| Ready | Not yet |
|---|---|
| Tools installed on this PC | Capture files committed |
| Reboot + USBPcap interfaces | CAP_GAIN / CAP_BIAS on |
| Dongle opens with `rtl_test` | P4 re-verify of new tables |

Physical: **you** plug the Blog V4 and run the capture session. Software side is prepared.
