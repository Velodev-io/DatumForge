#!/usr/bin/env python3
"""Summarize measured native reference-scene samples without asserting SRS budgets."""
import json
import math
from pathlib import Path
import statistics
ROOT = Path(__file__).resolve().parents[1]
folder = ROOT/'docs/testing/artifacts/module-1'
rows = []
def stats(values):
    return {'n': len(values), 'median_ms': statistics.median(values),
            'p95_nearest_rank_ms': sorted(values)[math.ceil(.95*len(values))-1]}
for dpr in ('1', '1.25', '1.5', '1.75', '2'):
    raw = json.loads((folder/f'native-{dpr}.json').read_text())
    rows.append({'effective_dpr': float(dpr), 'graphics': raw['graphics'],
                 'viewport_logical': raw['viewport_logical'], 'fixture': raw['fixture'],
                 'camera_command_to_frameSwapped': stats(raw['input_to_swap_ms']),
                 'cpu_submission_including_initial_frames': stats(raw['cpu_submission_ms']),
                 'process_memory_point_samples': raw.get('process_memory', [])})
(folder/'measurements.json').write_text(json.dumps({
    'scope': 'Debug Qt Test harness with 100 copies of one 12-triangle reference box; not B3 or NFR performance acceptance',
    'limitations': 'Camera-command presentation proxy and point memory snapshots; background/swap uncontrolled; no GPU/shared/steady/peak/cold-launch accounting',
    'rows': rows}, indent=2)+'\n')
print('Pass: native raw samples summarized without product budget claims')
