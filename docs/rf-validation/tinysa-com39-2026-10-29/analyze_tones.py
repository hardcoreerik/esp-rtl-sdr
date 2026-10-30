import json,hashlib,sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
index={c['name']:c for c in json.loads((root/'result.json').read_text()).get('captures',[])}
metrics=[];fig,ax=plt.subplots(figsize=(10,5))
for p in sorted(root.glob('*.cu8')):
 name=p.stem
 if not p.exists():continue
 meta=json.loads((root/(name+'.json')).read_text(encoding='utf-8-sig'));rate=meta['sample_rate_sps']
 raw=np.fromfile(p,np.uint8);pairs=raw.reshape(-1,2).astype(np.float64)-127.5;x=(pairs[:,0]+1j*pairs[:,1])/128;x-=np.mean(x)
 n=65536;count=min(len(x)//n,32);w=np.hanning(n);a=x[:count*n].reshape(count,n)
 power=np.mean(np.abs(np.fft.fftshift(np.fft.fft(a*w,axis=1),axes=1))**2,axis=0)/np.sum(w)**2
 db=10*np.log10(power+1e-30);f=np.fft.fftshift(np.fft.fftfreq(n,1/rate))
 def peak(offset):
  ids=np.flatnonzero(np.abs(f-offset)<20000);i=ids[np.argmax(db[ids])];return int(i)
 target=index.get(name,{}).get('source_hz',meta['requested_display_rf_hz']+100000)-meta['requested_display_rf_hz']
 ip=peak(target);im=peak(-target);ids=np.flatnonzero((np.abs(f)>5000)&(np.abs(f)<rate*.45));ig=ids[np.argmax(db[ids])];noise=float(np.median(db[ids]))
 row={'name':name,'bytes':int(raw.size),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'sample_rate_sps':rate,'expected_offset_hz':target,'source_on':index.get(name,{}).get('source_on'),'fft_size':n,'averaged_frames':count,'strongest_non_dc_offset_hz':float(f[ig]),'strongest_non_dc_bin_dbfs':float(db[ig]),'positive_target_offset_hz':float(f[ip]),'positive_target_bin_dbfs':float(db[ip]),'negative_target_offset_hz':float(f[im]),'negative_target_bin_dbfs':float(db[im]),'median_noise_bin_dbfs':noise,'positive_target_above_median_db':float(db[ip])-noise,'rms_dbfs':float(10*np.log10(np.mean(np.abs(x)**2)/2)),'clipped_scalar_percent':float(np.mean((raw==0)|(raw==255))*100),'clipped_pair_percent':float(np.mean(np.any(((raw==0)|(raw==255)).reshape(-1,2),axis=1))*100),'spectral_power_fraction_within_200khz':float(np.sum(power[np.abs(f)<200000])/np.sum(power))}
 metrics.append(row);ax.plot(f/1000,db,label=name,linewidth=.9)
ax.set(xlim=(-500,500),xlabel='Baseband offset (kHz)',ylabel='Window-normalized FFT bin power (dBFS)',title='COM39 Nooelec: conducted tinySA IQ spectra');[ax.axvline(t/1000,color='black',ls='--',lw=.8) for t in sorted(set(r['expected_offset_hz'] for r in metrics))];ax.grid(alpha=.25);ax.legend();fig.tight_layout();fig.savefig(out/'tone-spectra.png',dpi=150);ax.set_xlim(-rate/2000,rate/2000);fig.savefig(out/'tone-spectra-fullband.png',dpi=150);plt.close(fig)
(out/'iq-analysis.json').write_text(json.dumps(metrics,indent=2),encoding='utf-8');print(json.dumps(metrics,indent=2))
