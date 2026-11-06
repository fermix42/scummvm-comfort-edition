#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the explicit M2 local completion commands sequentially; no external API."""
import argparse, datetime, hashlib, json, subprocess, sys
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for key in ('exe','game','artifacts'):ap.add_argument('--'+key,type=Path,required=True)
 args=ap.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True)
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
 summary={'status':'IN PROGRESS','executable_sha256':sha(exe),'tasks':[],'artifacts':str(root)};reports={}
 def persist():(root/'local-final-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 def run(name,script,extra,game_arg=True):
  dest=root/name;argv=[sys.executable,str(Path(__file__).with_name(script)),'--exe',str(exe)]
  if game_arg:argv+=['--game',str(game)]
  argv+=['--artifacts',str(dest),*map(str,extra)]
  task={'name':name,'status':'IN PROGRESS','argv':argv};summary['tasks'].append(task);persist();print('START '+name,flush=True)
  with (root/(name+'.out.log')).open('wb') as log:result=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT)
  task['exit']=result.returncode
  candidates=list(dest.glob('*/'+('failure-verification.json' if script=='test_failures.py' else 'verification.json')))
  if result.returncode or len(candidates)!=1:task['status']='IN PROGRESS';persist();raise SystemExit('FAILED '+name+'; inspect '+str(root/(name+'.out.log')))
  report=candidates[0];data=json.loads(report.read_text());assert data['status']=='VERIFIED' and data['executable_sha256']==summary['executable_sha256']
  task.update(status='VERIFIED',report=str(report),report_sha256=sha(report));reports[name]=report.parent;persist();print('VERIFIED '+name,flush=True)
 run('resources','test_resources.py',['--base',game/'Nox Archaist HDV (v119).HDV','--expansion',game/'Nox Archaist Lord of Storms HDV (v137).HDV','--native-video'],False)
 run('m1-migration','test_migration.py',['--m1-evidence',game/'conversion/ce-m1-final/20261015-123959'])
 for mode in ('modern','classic'):
  classic=['--classic'] if mode=='classic' else []
  full=['--native-video','--observed-enhancements','--host-lifecycle','--interfaces','--chooser-ui','--callers','--metadata']
  if mode=='modern':full+=['--launcher-ui']
  run('gameplay-'+mode,'test_gameplay.py',full+classic)
  run('grid-'+mode,'test_gameplay.py',['--native-video','--grid','--metadata','--chooser-ui']+classic)
  run('default-owners-'+mode,'test_gameplay.py',['--native-video','--mode-defaults','--enhancement-fixture']+classic)
  run('dialogue-'+mode,'test_dialogue.py',['--lifecycle','--legacy-assistance']+classic)
  run('offline-base-'+mode,'test_gameplay.py',['--native-video','--observed-enhancements']+classic)
  run('management-'+mode,'test_session_management.py',['--baseline',reports['offline-base-'+mode]])
  run('save-history-'+mode,'test_save_history.py',['--baseline',reports['gameplay-'+mode]])
  run('io-'+mode,'test_io_failures.py',['--baseline',reports['dialogue-'+mode]])
  run('interrupted-'+mode,'test_failures.py',['--enhancement-fixture']+classic)
  run('owner-profiles-'+mode,'test_state_profiles.py',['--baseline',reports['dialogue-'+mode]])
 run('backoff-loopback','test_backoff.py',[])
 assert sha(exe)==summary['executable_sha256'];summary['status']='VERIFIED';persist();print('FINAL LOCAL VERIFIED '+str(root),flush=True)
if __name__=='__main__':main()
