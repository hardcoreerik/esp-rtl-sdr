import ctypes as C,json,sys,time
from pathlib import Path
r=Path(sys.argv[1]);l=C.CDLL(r'C:\Tools\esp-rtl-sdr-lab\v4l-driver-1.4.0\x64\rtlsdr.dll');ev=[]
RATES=[250000,256000,400000,600000,900000,960000,1024000,1400000,1800000,1920000,2000000,2048000,2400000,2560000,2800000,2880000,3200000,3300000,3600000,4000000]
FREQS=[96100000,433920000,1090000000]
def mark(stage,**k):
    ev.append(dict(t=time.time(),stage=stage,**k));(r/'events.json').write_text(json.dumps(ev,indent=0))
m,p,s=(C.create_string_buffer(256) for _ in range(3));l.rtlsdr_get_device_usb_strings(0,m,p,s);assert l.rtlsdr_get_device_count()==1
mark('device',product=p.value.decode(),serial=s.value.decode())
for rate in RATES:
    d=C.c_void_p();rc=l.rtlsdr_open(C.byref(d),0)
    if rc!=0: mark('open_failed',rate=rate,rc=rc);continue
    try:
        t=time.time();src=l.rtlsdr_set_sample_rate(d,rate);mark('rate_%d'%rate,t_start=t,rate=rate,rc=src,got=l.rtlsdr_get_sample_rate(d))
        for f in FREQS:
            t=time.time();rc=l.rtlsdr_set_center_freq(d,f);mark('tune_%d_%d'%(rate,f),t_start=t,rate=rate,rf=f,rc=rc);time.sleep(0.4)
            if f==FREQS[0]:
                l.rtlsdr_reset_buffer(d);n=C.c_int();buf=(C.c_ubyte*524288)();t=time.time();rr=l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n));el=time.time()-t
                mark('iq_%d'%rate,t_start=t,rate=rate,rc=rr,bytes=n.value,elapsed=el)
    finally:
        mark('close_%d'%rate,t_start=time.time(),rc=l.rtlsdr_close(d));time.sleep(0.5)
