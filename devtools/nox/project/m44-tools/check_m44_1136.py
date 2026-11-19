"""One isolated M4.4 allocation-rule batch; no campaign or existing-save edits."""
from pathlib import Path
import configparser,os,sys,shutil,subprocess,json,hashlib
ROOT=Path(os.environ.get('NOX_M44_CONVERSION',str(Path(__file__).resolve().parent)))
CE=Path(r'D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated')
sys.path.insert(0,str(CE/'devtools/nox'))
from test_resources import run_validation_process
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
label=sys.argv[1] if len(sys.argv)>1 else 'focused'
assert label.isalnum()
root=ROOT/'m44-functional1136'/label;root.mkdir(exist_ok=False);saves=root/'saves';saves.mkdir()
build=CE/'build-scummvm/Releasex64';exe=root/'scummvm.exe';shutil.copy2(build/'scummvm-m44-1136.exe',exe)
for p in build.glob('*.dll'):shutil.copy2(p,root/p.name)
keys=root/'keys.bin';keys.write_bytes(b'')
config=configparser.ConfigParser();config['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
config['nox-m44']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(ROOT.parent),'nox_native_startup_keys':str(keys),'nox_native_m44_awards1136':'true','nox_sound_mode':'0','ce_playthrough_tracking':'false'}
ini=root/'scummvm.ini'
with ini.open('w') as f:config.write(f)
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
argv=[str(exe),'--config='+str(ini),'--savepath='+str(saves),'--logfile='+str(root/'engine.log'),'--no-console','nox-m44']
result=run_validation_process(argv,root,dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startup,timeout=60)
(root/'stdout.txt').write_bytes(result.stdout);(root/'stderr.txt').write_bytes(result.stderr)
report=saves/'nox-native-m44-awards1136.tsv';assert result.returncode==0 and report.exists(),(result.returncode,result.stderr[-1000:])
assert 'cases=6' in report.read_text()
proof=dict(status='VERIFIED',scope='Allocation rule failure atomicity and adjacent success/merge only; no earned reachability, ordinary gameplay or campaign claim',executable_sha256=sha(exe),argv=argv,exit=result.returncode,report=report.read_text(),source_sha256={str(p.relative_to(CE)).replace('\\','/'):sha(p) for p in (CE/'engines/nox').iterdir() if p.is_file()})
(root/'verification.json').write_text(json.dumps(proof,indent=2)+'\n');print(report.read_text())
