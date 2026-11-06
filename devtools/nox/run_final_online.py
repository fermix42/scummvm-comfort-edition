#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded final M2 Play Session verification with unconditional credential cleanup."""
import argparse, datetime, hashlib, json, subprocess, sys, time
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for key in ('exe','game','local','private','identity','modern','classic','legacy-modern','legacy-classic','artifacts'):ap.add_argument('--'+key,type=Path,required=True)
 ap.add_argument('--prior',type=Path,help='Reuse only verified same-executable cases after confirmed credential cleanup')
 args=ap.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);private=args.private.resolve(strict=True);identity=args.identity.resolve(strict=True)
 local=json.loads((args.local/'local-final-summary.json').read_text());assert local['status']=='VERIFIED' and local['executable_sha256']==sha(exe)
 tasks={t['name']:Path(t['report']).parent for t in local['tasks']}
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
 summary={'status':'IN PROGRESS','executable_sha256':sha(exe),'local_suite':str(args.local),'cases':[],'credential_revoked':False,'artifacts':str(root)}
 def persist():(root/'online-final-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 def invoke(name,script,extra):
  argv=[sys.executable,str(Path(__file__).with_name(script)),*map(str,extra)]
  with (root/(name+'.out.log')).open('wb') as log:r=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT)
  assert r.returncode==0,'Failed '+name+'; inspect safe log '+str(root/(name+'.out.log'))
  return argv
 workflows=[('ordinary-modern',args.modern,False),('recovered-classic',args.legacy_classic,True),('ordinary-classic',args.classic,False),('recovered-modern',args.legacy_modern,True)]
 prior={}
 if args.prior:
  previous=json.loads(args.prior.read_text());assert previous['credential_revoked'] and previous['executable_sha256']==summary['executable_sha256']
  prior={case['name']:case for case in previous['cases'] if case['status']=='VERIFIED'}
 owner_sets=[]
 for name,base,recovered in workflows:
  source=base/'delete-keep' if recovered else base
  records=json.loads((source/'saves/ce-playthroughs.json').read_text())['playthroughs'];ids={r['client_playthrough_id'] for r in records}
  assert len(ids)==2 and all(not ids&old for old in owner_sets),'Online workflows must have independent owners'
  owner_sets.append(ids)
 persist()
 try:
  invoke('provision','provision_hermesbox.py',['--identity',identity,'--game',game,'--private',private]);print('DEDICATED CREDENTIAL ACTIVE',flush=True)
  for index,(name,base,recovered) in enumerate(workflows):
   if name in prior:
    case=dict(prior[name]);f=Path(case['report']);report=json.loads(f.read_text());assert sha(f)==case['report_sha256'] and report['status']=='VERIFIED' and report['executable_sha256']==summary['executable_sha256'] and Path(report['fixture']).resolve()==base.resolve()
    case['reused_verified_report']=True;summary['cases'].append(case);persist();print('REUSE VERIFIED '+name+'; no resend',flush=True);continue
   if index==2:
    print('RATE WINDOW: wait 61 seconds after completed first pair; no requests during wait',flush=True)
    end=time.monotonic()+61
    while time.monotonic()<end:time.sleep(max(0,min(1,end-time.monotonic())))
   extra=['--exe',exe,'--baseline',base,'--private',private,'--identity',identity,'--artifacts',root/name]
   if recovered:extra+=['--recovered-terminal']
   print('START '+name,flush=True);argv=invoke(name,'test_offline_link.py',extra)
   f=next((root/name).glob('*/verification.json'));report=json.loads(f.read_text());assert report['status']=='VERIFIED' and report['executable_sha256']==summary['executable_sha256'] and report['http_requests']<=24
   summary['cases'].append({'name':name,'status':'VERIFIED','report':str(f),'report_sha256':sha(f),'http_requests':report['http_requests'],'budget':24,'argv':argv});persist();print('VERIFIED '+name+' requests='+str(report['http_requests']),flush=True)
  summary['status']='VERIFIED'
 finally:
  argv=invoke('revoke','revoke_hermesbox.py',['--identity',identity,'--private',private,'--report',root/'credential-cleanup.json'])
  cleanup=json.loads((root/'credential-cleanup.json').read_text());assert cleanup['dedicated_credential_revoked'] and cleanup['non_admin']
  summary['credential_revoked']=True;summary['cleanup_sha256']=sha(root/'credential-cleanup.json');persist();print('DEDICATED CREDENTIAL REVOKED',flush=True)
 print('FINAL ONLINE VERIFIED '+str(root),flush=True)
if __name__=='__main__':main()
