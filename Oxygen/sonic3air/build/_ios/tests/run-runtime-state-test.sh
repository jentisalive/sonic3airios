#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../../.." && pwd)
python3 - "$repo_root" <<'PY'
import concurrent.futures
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
root=Path(sys.argv[1])
sources=list((root/'librmx/source/rmxbase').rglob('*.cpp'))
sources += [p for p in (root/'Oxygen/lemonscript/source/lemon').rglob('*.cpp')
            if '/compiler/' not in str(p) and '/generator/' not in str(p)]
sources.append(root/'Oxygen/sonic3air/build/_ios/tests/runtime_state_test.cpp')
cxx=shlex.split(os.environ.get('CXX','c++'))
flags=['-std=c++17','-O0','-ffunction-sections','-fdata-sections',
       '-I'+str(root/'librmx/source'),'-I'+str(root/'Oxygen/lemonscript/source')]
# Optional ASan/UBSan run uses the same real VM, not a mirror of the parser.
flags += shlex.split(os.environ.get('RUNTIME_TEST_FLAGS',''))
with tempfile.TemporaryDirectory(prefix='s3air-runtime-') as build:
    objects=[Path(build)/f'{i}.o' for i in range(len(sources))]
    def compile_one(item):
        source,obj=item
        subprocess.run(cxx+flags+['-c',str(source),'-o',str(obj)],check=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(4,os.cpu_count() or 1)) as pool:
        list(pool.map(compile_one,zip(sources,objects)))
    binary=Path(build)/'runtime-state-test'
    subprocess.run(cxx+flags+[str(p) for p in objects]+['-lz','-Wl,--gc-sections','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
PY
