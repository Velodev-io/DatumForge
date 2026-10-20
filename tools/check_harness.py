#!/usr/bin/env python3
"""Prove both executable and CTest report a deliberate failed assertion."""
from pathlib import Path
import subprocess
import sys
import tempfile
exe=Path(sys.argv[1]).resolve()
direct=subprocess.run([str(exe),'--fail'],capture_output=True,text=True)
assert direct.returncode==1 and 'FAIL:' in direct.stderr
with tempfile.TemporaryDirectory(prefix='cad-step-zero-harness-') as root:
    escaped=str(exe).replace('\\','/').replace('"','\\"')
    (Path(root)/'CTestTestfile.cmake').write_text(f'add_test(deliberate_failure "{escaped}" "--fail")\n')
    result=subprocess.run(['ctest','--test-dir',root,'--output-on-failure'],capture_output=True,text=True)
    assert result.returncode!=0 and '0% tests passed' in result.stdout and 'Failed' in result.stdout,result
    print(result.stdout)
    print(f'Pass: executable exit={direct.returncode}; CTest exit={result.returncode} for deliberate failure')
