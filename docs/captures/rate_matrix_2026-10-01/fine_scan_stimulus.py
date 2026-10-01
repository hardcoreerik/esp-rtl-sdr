import ctypes as C,json,sys,time
from pathlib import Path
r=Path(sys.argv[1]);start,stop,step=int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4])
l=C.CDLL(r'C:\Tools\esp-rtl-sdr-lab\v4l-driver-1.4.0\x64\rtlsdr.dll');d=C.c_void_p();ev=[]
def mark(stage,**k):
    ev.append(dict(t=time.time(),stage=stage,**k))
m,p,s=(C.create_string_buffer(256) for _ in range(3));l.rtlsdr_get_device_usb_strings(0,m,p,s);assert l.rtlsdr_get_device_count()==1
mark('device',product=p.value.decode())
rc=l.rtlsdr_open(C.byref(d),0);assert rc==0
try:
    mark('rate',rc=l.rtlsdr_set_sample_rate(d,int(sys.argv[5]) if len(sys.argv)>5 else 2048000))
    for f in range(start,stop+1,step):
        t=time.time();rc=l.rtlsdr_set_center_freq(d,f);mark('tune_scan_%d'%f,t_start=t,rc=rc,rf=f);time.sleep(0.03)
finally:
    mark('close',rc=l.rtlsdr_close(d));(r/'events.json').write_text(json.dumps(ev))

