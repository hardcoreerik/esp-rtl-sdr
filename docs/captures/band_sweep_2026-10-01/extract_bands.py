import json,subprocess,csv,sys,collections
PCAP,EVF,BUS,ADDR,OUT=sys.argv[1:6]
ev=json.load(open(EVF))
out=subprocess.run([r'C:\Program Files\Wireshark\tshark.exe','-r',PCAP,'-Y',f'usb.bus_id=={BUS} && usb.device_address=={ADDR} && usb.bmRequestType==0x40 && usb.setup.wIndex==0x0610 && usb.setup.wLength==2','-T','fields','-e','frame.time_epoch','-e','usb.setup.wValue','-e','usb.data_fragment'],capture_output=True,text=True).stdout
W=[]
for ln in out.splitlines():
    t,wv,h=ln.split('\t');h=h.strip()
    if len(h)==4 and wv not in ('0x00c6','0x00c8'):W.append((float(t),h[:2],h[2:],wv))
TV=collections.Counter(w[3] for w in W).most_common(1)[0][0];print('tuner i2c wValue',TV)
W=[w[:3] for w in W if w[3]==TV]
tunes=[e for e in ev if e['stage'].startswith('tune_')]
IF=3569993.59
B=[(0,8,2,0xdf),(50,8,2,0xbe),(55,8,2,0x8b),(60,8,2,0x7b),(65,8,2,0x69),(70,8,2,0x58),(75,0,2,0x44),(80,0,2,0x44),(90,0,2,0x34),(100,0,2,0x34),(110,0,2,0x24),(120,0,2,0x24),(140,0,2,0x14),(180,0,2,0x13),(220,0,2,0x13),(250,0,2,0x11),(280,0,2,0),(310,0,0x41,0),(450,0,0x41,0),(588,0,0x40,0),(650,0,0x40,0)]
def row(mhz):
    r=B[0]
    for x in B:
        if mhz>=x[0]:r=x
    return r
rows=[];bad=0
for i,e in enumerate(tunes):
    lo_t=e['t_start']-0.01;hi_t=tunes[i+1]['t_start']-0.01 if i+1<len(tunes) else e['t_start']+0.45
    reg={}
    for t,r,v in W:
        if lo_t<=t<hi_t:reg[r]=v
    lo=(e['rf']+IF)/1e6;rw=row(int(lo))
    g=lambda k:int(reg[k],16) if k in reg else None
    r17,r1a,r1b=g('17'),g('1a'),g('1b')
    obs=(None if r17 is None else (r17&8), None if r1a is None else (r1a&0xc3), r1b)
    ok= obs==(rw[1],rw[2],rw[3]);bad+=not ok
    rows.append(dict(pass_=e['stage'].split('_')[1],rf_hz=e['rf'],lo_mhz_assumed_if357=round(lo,3),r820t2_table_row=rw[0],r17=reg.get('17'),r1a=reg.get('1a'),r1b=reg.get('1b'),matches_r820t2_table=ok))
with open(OUT,'w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
print('points',len(rows),'matching the R820T2 table',len(rows)-bad)
