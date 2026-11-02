import os
import ctypes as C,json,time,pathlib,subprocess,hashlib
import numpy as np,serial
r=(pathlib.Path(os.environ.get('ESP_RTL_SDR_PC_OUTPUT_ROOT','C:/Tools/esp-rtl-sdr-lab/captures/e4000-xtr-20261031'))/'tinysa-paused-matrix');r.mkdir(exist_ok=False)
p=serial.Serial(port=None,baudrate=115200,timeout=.15,write_timeout=2);p.dtr=False;p.rts=False;p.port='COM7';p.open()
events=[];d=C.c_void_p();cap=None;clog=None
l=C.CDLL(r'C:\Tools\esp-rtl-sdr-lab\v4l-driver-1.4.0\x64\rtlsdr.dll')
for n in ['rtlsdr_close','rtlsdr_set_center_freq','rtlsdr_set_sample_rate','rtlsdr_set_tuner_gain','rtlsdr_set_tuner_gain_mode','rtlsdr_set_agc_mode','rtlsdr_set_tuner_bandwidth','rtlsdr_set_bias_tee','rtlsdr_reset_buffer']:
 getattr(l,n).argtypes=[C.c_void_p]+([] if n in ['rtlsdr_close','rtlsdr_reset_buffer'] else [C.c_int])
l.rtlsdr_read_sync.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.POINTER(C.c_int)]
def mark(stage,**kw):
 e=dict(t=time.time(),stage=stage,**kw);events.append(e);(r/'events.json').write_text(json.dumps(events,indent=2));print(json.dumps(e),flush=True)
def cmd(s):
 if s.startswith('sweep cw '):cmd('pause')
 p.write((s+'\r').encode());end=time.monotonic()+4;data=b''
 while time.monotonic()<end:
  data+=p.read(4096)
  if data.endswith(b'ch> '):break
 resp=data.decode(errors='replace');mark('source_command',command=s,response=resp)
 if not data.endswith(b'ch> '):raise RuntimeError('tinySA prompt timeout: '+s)
 if s.startswith('sweep cw '):cmd('resume')
 return resp
def call(stage,name,*args):
 t=time.time();rc=getattr(l,name)(d,*args);mark(stage,t_start=t,rc=rc,args=list(args));assert rc==0,(name,rc)
def measure(stage,offset=100000,**meta):
 time.sleep(.15);l.rtlsdr_reset_buffer(d);buf=(C.c_ubyte*524288)();n=C.c_int()
 for _ in range(2):
  rc=l.rtlsdr_read_sync(d,buf,len(buf),C.byref(n));assert rc==0 and n.value==len(buf),(rc,n.value)
 b=bytes(buf);name=f'{len(events):03d}-{stage}.cu8';(r/name).write_bytes(b)
 a=np.frombuffer(b,np.uint8);z=(a[0::2].astype(float)-127.5+1j*(a[1::2].astype(float)-127.5))/128
 N=65536;w=np.hanning(N);ft=np.fft.fft(z[:N]*w)/w.sum();freq=np.fft.fftfreq(N,1/2400000);k=np.where(np.abs(freq-offset)<2500)[0];peak=k[np.argmax(np.abs(ft[k]))]
 mark(stage,artifact=name,sha256=hashlib.sha256(b).hexdigest(),tone_hz=float(freq[peak]),tone_dbfs=float(20*np.log10(max(abs(ft[peak]),1e-12))),rms_dbfs=float(10*np.log10(max(np.mean(abs(z)**2),1e-12))),clip_fraction=float(np.mean((a==0)|(a==255))),**meta)
 return events[-1]
try:
 cmd('output off');cmd('version');cmd('info')
 assert l.rtlsdr_get_device_count()==1
 m,prod,s=[C.create_string_buffer(256) for _ in range(3)];l.rtlsdr_get_device_usb_strings(0,m,prod,s);assert s.value==b'82115932'
 clog=(r/'usb-capture.log').open('w');cap=subprocess.Popen([r'C:\Program Files\Wireshark\tshark.exe','-i',r'\\.\USBPcap1','-a','duration:45','-s','256','-w',str(r/'controls.pcapng')],stdout=clog,stderr=clog);time.sleep(2)
 t=time.time();rc=l.rtlsdr_open(C.byref(d),0);mark('open',t_start=t,rc=rc);assert rc==0
 call('bias_off','rtlsdr_set_bias_tee',0);call('rate','rtlsdr_set_sample_rate',2400000);call('tune','rtlsdr_set_center_freq',100000000);call('manual','rtlsdr_set_tuner_gain_mode',1);call('gain','rtlsdr_set_tuner_gain',190);call('agc_off','rtlsdr_set_agc_mode',0);call('bw_auto','rtlsdr_set_tuner_bandwidth',0)
 cmd('pause');cmd('mode low output');cmd('pause');cmd('level -40');cmd('modulation off');cmd('sweep cw 100100000');cmd('output off');measure('source_off',source_dbm=-40,gain_tenth_db=190)
 cmd('resume');cmd('output on');on=measure('source_on',source_dbm=-40,gain_tenth_db=190)
 off=next(e for e in events if e['stage']=='source_off');assert on['tone_dbfs']-off['tone_dbfs']>15,'No verified source tone: stop RF matrix'
 for g in [-10,15,40,65,90,115,140,165,190,215,240,290,340,420]:
  call('gain_set','rtlsdr_set_tuner_gain',g)
  e=measure('gain',source_dbm=-40,gain_tenth_db=g)
  if e['clip_fraction']>.01:mark('gain_stop_clipping');break
 call('gain_restore','rtlsdr_set_tuner_gain',190)
 for level in [-50,-40,-30]:
  cmd('level '+str(level));measure('level',source_dbm=level,gain_tenth_db=190)
 cmd('level -40')
 for bw in [0,200000,500000,1000000,2400000]:
  call('bandwidth_set','rtlsdr_set_tuner_bandwidth',bw)
  for offset in [20000,300000,-300000,900000]:
   cmd('sweep cw '+str(100000000+offset));measure('bandwidth',offset=offset,requested_bandwidth_hz=bw,offset_hz=offset,source_dbm=-40,gain_tenth_db=190)
 cmd('output off');call('bw_restore','rtlsdr_set_tuner_bandwidth',0);cmd('sweep cw 100100000');measure('source_off_final',source_dbm=-40,gain_tenth_db=190)
finally:
 try:cmd('output off')
 finally:
  p.close()
  if d.value:mark('close',rc=l.rtlsdr_close(d))
  if cap:cap.wait()
  if clog:clog.close()
print('capture_exit',cap.returncode if cap else None)
