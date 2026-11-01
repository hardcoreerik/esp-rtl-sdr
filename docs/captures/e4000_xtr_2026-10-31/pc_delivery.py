import os
import ctypes as C,json,time,subprocess,pathlib
root=pathlib.Path(os.environ.get('ESP_RTL_SDR_PC_OUTPUT_ROOT', 'C:/Tools/esp-rtl-sdr-lab/captures/e4000-xtr-20261031'));out=root/'sustained-and-boundaries';out.mkdir(exist_ok=False)
ts=r'C:\Program Files\Wireshark\tshark.exe'
log=(out/'capture.log').open('w');cap=subprocess.Popen([ts,'-i',r'\\.\USBPcap1','-a','duration:100','-s','256','-w',str(out/'controls.pcapng')],stdout=log,stderr=log)
time.sleep(2)
l=C.CDLL(r'C:\Tools\esp-rtl-sdr-lab\v4l-driver-1.4.0\x64\rtlsdr.dll');d=C.c_void_p();events=[]
for n in ['rtlsdr_close','rtlsdr_get_tuner_type','rtlsdr_get_tuner_gains','rtlsdr_set_center_freq','rtlsdr_get_center_freq','rtlsdr_set_sample_rate','rtlsdr_get_sample_rate','rtlsdr_set_tuner_gain','rtlsdr_set_tuner_gain_mode','rtlsdr_set_agc_mode','rtlsdr_set_tuner_bandwidth','rtlsdr_set_direct_sampling','rtlsdr_get_direct_sampling','rtlsdr_reset_buffer']:
 getattr(l,n).argtypes=[C.c_void_p]+({'rtlsdr_get_tuner_gains':[C.POINTER(C.c_int)]}.get(n,[] if n in ['rtlsdr_close','rtlsdr_get_tuner_type','rtlsdr_get_center_freq','rtlsdr_get_sample_rate','rtlsdr_get_direct_sampling','rtlsdr_reset_buffer'] else [C.c_uint32]))
l.rtlsdr_read_sync.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.POINTER(C.c_int)]
def mark(stage,**kw):
 e=dict(t=time.time(),stage=stage,**kw);events.append(e);(out/'events.json').write_text(json.dumps(events,indent=2));print(json.dumps(e),flush=True)
def call(stage,name,*args):
 t=time.time();rc=getattr(l,name)(d,*args);mark(stage,t_start=t,rc=rc,args=list(args));return rc
try:
 m,p,s=[C.create_string_buffer(256) for _ in range(3)];assert l.rtlsdr_get_device_count()==1;l.rtlsdr_get_device_usb_strings(0,m,p,s);assert s.value==b'82115932'
 def opened(stage):
  t=time.time();rc=l.rtlsdr_open(C.byref(d),0);mark(stage,t_start=t,rc=rc);assert rc==0
 opened('open')
 mark('identity',manufacturer=m.value.decode(),product=p.value.decode(),serial=s.value.decode(),tuner=l.rtlsdr_get_tuner_type(d))
 call('tune_100M','rtlsdr_set_center_freq',100000000)
 call('manual','rtlsdr_set_tuner_gain_mode',1);call('gain_19dB','rtlsdr_set_tuner_gain',190);call('rtl_agc_off','rtlsdr_set_agc_mode',0)
 import hashlib,numpy as np
 for rate in [250000,960000,1024000,2048000,2400000,2560000,2880000,3200000]:
  assert call('rate_'+str(rate),'rtlsdr_set_sample_rate',rate)==0
  l.rtlsdr_reset_buffer(d);buf=(C.c_ubyte*262144)();n=C.c_int();l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n))
  t=time.monotonic();total=0;reads=0;short=0;errors=[];windows=[];first=b''
  while time.monotonic()-t<5:
   t0=time.monotonic();rc=l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n));dt=time.monotonic()-t0
   if rc:errors.append(rc);break
   total+=n.value;reads+=1;short+=int(n.value!=len(buf));windows.append(dt)
   if not first:first=bytes(buf[:n.value])
  elapsed=time.monotonic()-t;a=np.frombuffer(first,dtype=np.uint8);name='iq-'+str(rate)+'.cu8';(out/name).write_bytes(first)
  mark('sustained_iq',rate=rate,bytes=total,elapsed=elapsed,measured_sps=total/2/elapsed,short_reads=short,errors=errors,reads=reads,max_read_ms=max(windows)*1000,artifact=name,sha256=hashlib.sha256(first).hexdigest(),i_mean=float(a[0::2].mean()),q_mean=float(a[1::2].mean()),std=float(a.std()),clip_fraction=float(np.mean((a==0)|(a==255))),scope='host delivery and uncalibrated no-source IQ; not loss proof')
 for v in [63000000,64000000,65000000,1080000000,1090000000,1100000000,1110000000,1120000000,1130000000,1135000000,1140000000,1150000000,1160000000,1170000000,1180000000,1190000000,1200000000,1250000000,1300000000,1500000000,1700000000]:
  call('boundary_tune_'+str(v),'rtlsdr_set_center_freq',v)
  mark('tune_readback',requested=v,actual=l.rtlsdr_get_center_freq(d))
 call('recover_100M','rtlsdr_set_center_freq',100000000)
 call('close','rtlsdr_close');d.value=None
 for cycle in range(5):
  opened('reopen_'+str(cycle));call('restart_rate','rtlsdr_set_sample_rate',2400000);call('restart_tune','rtlsdr_set_center_freq',100000000)
  l.rtlsdr_reset_buffer(d);buf=(C.c_ubyte*262144)();n=C.c_int();rc=l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n));mark('restart_iq',cycle=cycle,rc=rc,bytes=n.value)
  call('reclose_'+str(cycle),'rtlsdr_close');d.value=None
finally:
 if d.value:call('close_final','rtlsdr_close')
 cap.wait();log.close()
print('capture_exit',cap.returncode)
