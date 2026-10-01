import json,subprocess
d='ratematrix-v4-run2-20261001/';ev=json.load(open(d+'events.json'))
o=subprocess.run([r'C:\Program Files\Wireshark\tshark.exe','-r',d+'matrix.pcapng','-Y','usb.bus_id==2 && usb.device_address==10 && usb.bmRequestType==0x40','-T','fields','-e','frame.time_epoch','-e','usb.setup.wValue','-e','usb.setup.wIndex','-e','usb.data_fragment'],capture_output=True,text=True).stdout.splitlines()
W=[l.split('\t') for l in o]

for rate in (960000,2048000,3200000):
    t0=[e for e in ev if e['stage']=='rate_%d'%rate][0]['t_start']
    tuneA=[e for e in ev if e['stage']=='tune_%d_96100000'%rate][0]['t_start']
    print('=== rate',rate,'  (rate call at +0, first tune at +%.3f s)'%(tuneA-t0))
    for r in W:
        if len(r)<4: r=r+['']*(4-len(r))
        t=float(r[0])
        if t0-0.02<=t<=tuneA+0.4:
            wv,wi,h=r[1],r[2],r[3].strip()
            if wi in ('0x0011','17') and wv in('0x9f20','0xa120','0x1920','0x1a20','0x1b20'): print('  +%.3f demod %s = %s'%(t-t0,wv,h))
            if wi in ('0x0610','1552') and len(h)==4 and h[:2] in('0a','0b','10','14','15','16','17','1a','1b'): print('  +%.3f tuner reg %s = %s'%(t-t0,h[:2],h[2:]))
