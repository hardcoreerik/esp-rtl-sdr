import os
import ctypes as C,json,time,subprocess,pathlib
root=pathlib.Path(os.environ.get('ESP_RTL_SDR_PC_OUTPUT_ROOT', 'C:/Tools/esp-rtl-sdr-lab/captures/e4000-xtr-20261031'));out=root/'controls-verified';out.mkdir(exist_ok=False)
ts=r'C:\Program Files\Wireshark\tshark.exe'
log=(out/'capture.log').open('w');cap=subprocess.Popen([ts,'-i',r'\\.\USBPcap1','-a','duration:22','-s','256','-w',str(out/'controls.pcapng')],stdout=log,stderr=log)
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
 t=time.time();rc=l.rtlsdr_open(C.byref(d),0);mark('open',t_start=t,rc=rc);assert rc==0
 mark('identity',manufacturer=m.value.decode(),product=p.value.decode(),serial=s.value.decode(),tuner=l.rtlsdr_get_tuner_type(d))
 call('rate_initial','rtlsdr_set_sample_rate',2400000)
 call('tune_initial','rtlsdr_set_center_freq',100000000)
 n=l.rtlsdr_get_tuner_gains(d,None);assert 0<n<128;g=(C.c_int*n)();l.rtlsdr_get_tuner_gains(d,g);mark('gain_table',gains=list(g))
 call('gain_manual','rtlsdr_set_tuner_gain_mode',1)
 for v in g:call('gain_'+str(v),'rtlsdr_set_tuner_gain',v)
 for v in [0,1,0]:call('gain_mode_'+str(v),'rtlsdr_set_tuner_gain_mode',v)
 for v in [0,1,0]:call('rtl_agc_'+str(v),'rtlsdr_set_agc_mode',v)
 for v in [0,200000,500000,1000000,2400000]:call('bandwidth_'+str(v),'rtlsdr_set_tuner_bandwidth',v)
 for v in [64000000,100000000,300000000,433920000,906875000,1090000000,1135000000,1500000000,1700000000]:call('tune_'+str(v),'rtlsdr_set_center_freq',v)
 call('tune_restore','rtlsdr_set_center_freq',100000000)
 for rate in [250000,960000,1024000,2048000,2400000,2560000,2880000,3200000]:
  rc=call('rate_'+str(rate),'rtlsdr_set_sample_rate',rate)
  mark('rate_readback',requested=rate,actual=l.rtlsdr_get_sample_rate(d))
  if rc:continue
  l.rtlsdr_reset_buffer(d);buf=(C.c_ubyte*262144)();n=C.c_int();l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n));t=time.monotonic();total=0;reads=0;rr=0
  while time.monotonic()-t<1:
   rr=l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n));total+=n.value;reads+=1
   if rr:break
  elapsed=time.monotonic()-t;mark('iq_delivery',rate=rate,bytes=total,elapsed=elapsed,measured_sps=total/2/elapsed,reads=reads,rc=rr,scope='host delivery throughput, not calibrated sample clock or loss proof')
 for v in [1,0,2,0]:
  call('direct_'+str(v),'rtlsdr_set_direct_sampling',v);mark('direct_readback',requested=v,actual=l.rtlsdr_get_direct_sampling(d))
finally:
 if d.value:call('close','rtlsdr_close')
 cap.wait();log.close()
print('capture_exit',cap.returncode)
