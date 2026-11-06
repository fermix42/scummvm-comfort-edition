"""Select real CE service tests in two separate processes; no network token is configured."""
from pathlib import Path
import argparse
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True)
args=p.parse_args(); build=args.build.resolve(strict=True)
for method in ('testWriteOfflineAchievementFixture','testReadOfflineAchievementFixture'):
    result=subprocess.run([str(build/'Releasex64/scummvm-tests.exe'),'CEColdProcessTestSuite',method],
                          cwd=build,capture_output=True,timeout=30)
    (build/f'{method}.log').write_bytes(result.stdout+result.stderr)
    assert result.returncode==0,(method,result.returncode,result.stdout,result.stderr)
    assert b'(1 test)' in result.stdout or b'(1 tests)' in result.stdout,result.stdout
    assert b'OK!' in result.stdout,result.stdout
    print('PASS:',method,'in a fresh process')
