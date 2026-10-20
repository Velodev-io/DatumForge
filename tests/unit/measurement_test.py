import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('sample',Path(__file__).parents[2]/'tools/sample_memory.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
assert module.percentile(list(range(1,31)))==29
assert module.percentile([10])==10
for values in [[],[float('nan')],[float('inf')]]:
    try:module.percentile(values)
    except ValueError:pass
    else:raise AssertionError('Invalid samples accepted')
print('Pass: nearest-rank p95 and rejected missing/nonfinite samples')
