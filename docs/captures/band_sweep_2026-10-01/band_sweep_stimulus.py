import ctypes as C,json,sys,time
from pathlib import Path
r=Path(sys.argv[1]);l=C.CDLL(r'C:\Tools\esp-rtl-sdr-lab\v4l-driver-1.4.0\x64\rtlsdr.dll');d=C.c_void_p();ev=[]
IF=3_570_000
B=[50,55,60,65,70,75,80,90,100,110,120,140,180,220,250,280,310,450,588,650]
pts=[26_430_000,40_000_000]
for b in B: pts+= [b*1_000_000-IF-300_000, b*1_000_000-IF+300_000]
pts+=[433_920_000,915_000_000,1_000_000_000,1_500_000_000,1_700_000_000]
pts=sorted(set(pts))
def mark(stage,**k):
    x=dict(t=time.time(),stage=stage,**k);ev.append(x);(r/'events.json').write_text(json.dumps(ev,indent=1));print(json.dumps(x),flush=True)
m,p,s=(C.create_string_buffer(256) for _ in range(3));l.rtlsdr_get_device_usb_strings(0,m,p,s);assert l.rtlsdr_get_device_count()==1;mark('device',manufacturer=m.value.decode(),product=p.value.decode(),serial=s.value.decode())
t=time.time();rc=l.rtlsdr_open(C.byref(d),0);mark('open',t_start=t,rc=rc);assert rc==0
try:
    mark('tuner_type',type=l.rtlsdr_get_tuner_type(d))
    mark('rate',rc=l.rtlsdr_set_sample_rate(d,2048000))
    for pass_,order in (('asc',pts),('desc',pts[::-1])):
        for f in order:
            t=time.time();rc=l.rtlsdr_set_center_freq(d,f);mark('tune_%s_%d'%(pass_,f),t_start=t,rc=rc,rf=f)
            time.sleep(0.4)
finally:
    mark('close',rc=l.rtlsdr_close(d))

