import json,subprocess,csv,sys,collections
PCAP,EVF,BUS,ADDR,OUT=sys.argv[1:6]
ev=json.load(open(EVF));ts=r'C:\Program Files\Wireshark\tshark.exe'
def q(filt,fields):
    o=subprocess.run([ts,'-r',PCAP,'-Y',f'usb.bus_id=={BUS} && usb.device_address=={ADDR} && usb.bmRequestType==0x40 && '+filt,'-T','fields']+sum([['-e',f] for f in fields],[]),capture_output=True,text=True).stdout
    return [l.split('\t') for l in o.splitlines()]
dem=[(float(a),wv,wl,h.strip()) for a,wv,wl,h in [r+['']*(4-len(r)) for r in q('usb.setup.wIndex==0x0011',['frame.time_epoch','usb.setup.wValue','usb.setup.wLength','usb.data_fragment'])]]
tun_all=q('usb.setup.wIndex==0x0610 && usb.setup.wLength==2',['frame.time_epoch','usb.setup.wValue','usb.data_fragment'])
tun=[(float(a),wv,h.strip()) for a,wv,h in [r+['']*(3-len(r)) for r in tun_all] if len(h.strip())==4 and wv not in('0x00c6','0x00c8')]
TV=collections.Counter(t[1] for t in tun).most_common(1)[0][0];tun=[t[::2] if False else (t[0],t[2][:2],t[2][2:]) for t in tun if t[1]==TV]
XT=28800000.0
rows=[];tunes=[e for e in ev if e['stage'].startswith('tune_')]
rate_start={e['rate']:e['t_start'] for e in ev if e['stage'].startswith('rate_')}
for e in tunes:
    # Each column is the register STATE at the end of this tune's window: the last value written since the rate was set
    # (t0) up to 0.40 s after this tune call (t1), never anything from a later tune or another rate. The DLL writes the
    # filter (0a/0b) and demod IF once per rate, in the first tune's window, and later tunes at that rate do not rewrite
    # them, so those cells hold the value the hardware keeps; they are not writes made by that tune.
    rate,rf=e['rate'],e['rf'];t0=rate_start[rate]-0.05;t1=e['t_start']+0.40
    st={}
    for t,r,v in tun:
        if t0<=t<=t1:st['t'+r]=v
    d={}
    for t,wv,wl,h in dem:
        if t0<=t<=t1 and wv in('0x9f20','0xa120','0xa220','0x1920','0x1a20','0x1b20','0x1820'): d[wv]=h
    ratio=None
    if '0x9f20' in d and '0xa120' in d:
        ratio=int((d['0x9f20']+d['0xa120']),16)
    ifw=None
    if all(k in d for k in('0x1920','0x1a20','0x1b20')): ifw=int(d['0x1920']+d['0x1a20']+d['0x1b20'],16)
    ifhz=None if ifw is None else (-ifw & 0x3fffff)*XT/4194304
    imp=None if not ratio else XT*4194304/ratio
    rows.append(dict(rate=rate,rf_hz=rf,ratio_hex='%08x'%ratio if ratio else '',implied_rate_hz=round(imp) if imp else '',demod_if_hex='%06x'%ifw if ifw is not None else '',demod_if_hz=round(ifhz) if ifhz is not None else '',r0a=st.get('t0a',''),r0b=st.get('t0b',''),r17=st.get('t17',''),r1a=st.get('t1a',''),r1b=st.get('t1b',''),r10=st.get('t10',''),r14=st.get('t14',''),r15=st.get('t15',''),r16=st.get('t16','')))
with open(OUT,'w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
print('tuner wValue',TV,'rows',len(rows))
