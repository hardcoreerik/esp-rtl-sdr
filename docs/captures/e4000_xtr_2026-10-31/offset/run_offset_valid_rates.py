"""First-party E4000 offset-tuning PC stimulus; no source disassembly."""
import ctypes as C,json,time,subprocess,pathlib,hashlib,os
root=pathlib.Path(os.environ.get('ESP_RTL_SDR_OFFSET_OUTPUT', 'C:/Tools/esp-rtl-sdr-lab/captures/e4000-xtr-20261031/offset-valid-rate-controls'));root.mkdir(exist_ok=False)
l=C.CDLL(r'C:\Tools\esp-rtl-sdr-lab\v4l-driver-1.4.0\x64\rtlsdr.dll');d=C.c_void_p();events=[];cap=None;log=None
for name in ['rtlsdr_set_sample_rate','rtlsdr_set_center_freq','rtlsdr_set_offset_tuning']:
 getattr(l,name).argtypes=[C.c_void_p,C.c_uint32]
for name in ['rtlsdr_get_offset_tuning','rtlsdr_get_center_freq','rtlsdr_reset_buffer','rtlsdr_close']:
 getattr(l,name).argtypes=[C.c_void_p]
l.rtlsdr_get_center_freq.restype=C.c_uint32
l.rtlsdr_read_sync.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.POINTER(C.c_int)]
def mark(stage,**kw):
 e=dict(t=time.time(),stage=stage,**kw);events.append(e);(root/'events.json').write_text(json.dumps(events,indent=2));print(json.dumps(e),flush=True)
def call(stage,name,*args):
 time.sleep(.25);start=time.time();rc=getattr(l,name)(d,*args);mark(stage,t_start=start,rc=rc,args=list(args));return rc
try:
 assert l.rtlsdr_get_device_count()==1,'Expected only enrolled XTR on PC'
 m,p,s=[C.create_string_buffer(256) for _ in range(3)];assert l.rtlsdr_get_device_usb_strings(0,m,p,s)==0;assert s.value==b'82115932';mark('identity',manufacturer=m.value.decode(),product=p.value.decode(),serial=s.value.decode())
 log=(root/'capture.log').open('w');cap=subprocess.Popen([r'C:\Program Files\Wireshark\tshark.exe','-i',r'\\.\USBPcap1','-a','duration:55','-s','256','-w',str(root/'offset.pcapng')],stdout=log,stderr=log);time.sleep(2);assert cap.poll() is None,'Capture failed before opening receiver'
 start=time.time();rc=l.rtlsdr_open(C.byref(d),0);mark('open',t_start=start,rc=rc);assert rc==0
 for rate in [240000,250000,256000,900001,960000,1024000,1400000,1800000,1920000,2000000,2048000,2400000,2560000,2800000,2880000,3200000]:
  assert call('rate_'+str(rate),'rtlsdr_set_sample_rate',rate)==0
  assert call('center_'+str(rate),'rtlsdr_set_center_freq',96100000)==0
  for enabled in [0,1,0]:
   rc=call('offset_'+str(rate)+'_'+str(enabled),'rtlsdr_set_offset_tuning',enabled)
   mark('offset_readback',rate=rate,requested=enabled,actual=l.rtlsdr_get_offset_tuning(d),center_hz=l.rtlsdr_get_center_freq(d))
   if rc:continue
finally:
 if d.value:call('close','rtlsdr_close')
 if cap:cap.wait();print('capture_exit',cap.returncode)
 if log:log.close()

