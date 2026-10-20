#!/usr/bin/env python3
"""Deterministic semantic workload recipes; never claims a model was evaluated."""
import argparse
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def fixtures():
    common = {'version': 1, 'seed': 19002026, 'source': 'original project synthetic definitions',
              'license': 'CC0-1.0 for these synthetic fixture recipes only',
              'semantic_length_unit': 'm', 'kernel_length_unit': 'mm'}
    result = {}
    def put(key, payload):
        result[key] = dict(common, id=key, **payload)
    put('B0', {'recipe': {'documents': [], 'settle_seconds': 30}, 'expected': {'open_documents': 0},
               'evaluation': 'Not run: native empty production editor belongs to Module 1'})
    put('B1', {'recipe': {'width_mm': 80, 'height_mm': 50, 'depth_mm': 10,
                         'sketches': [{'type': 'rectangle', 'origin_mm': [0, 0], 'size_mm': [80, 50]},
                                      {'type': 'rectangle', 'origin_mm': [20, 20], 'size_mm': [20, 20]}],
                         'hole_radius_mm': 2, 'hole_centers_mm': [[10, 10], [60, 10], [10, 40], [60, 40]],
                         'fillet': {'role': 'outer vertical edge at x=0,y=0', 'radius_mm': 2}},
               'expected': {'solids': 1, 'base_volume_mm3': 40000, 'cut_volume_mm3': 36000,
                            'pattern_volume_mm3': 36000-160*math.pi, 'final_volume_mm3': 35960-150*math.pi,
                            'sketch_entities': 8, 'sketch_constraints': 16},
               'evaluation': 'Recipe frozen; see cad.geometry_topology for actual kernel spike evidence; production history unexecuted'})
    features = [{'type': 'extrude', 'profile_mm': [200, 100], 'depth_mm': 10}]
    features += [{'type': 'hole', 'center_mm': [10+18*(i%11), 10+10*(i//11)],
                  'radius_mm': 1, 'through_all': True} for i in range(99)]
    put('B2', {'recipe': {'features': features},
               'expected': {'features': 100, 'bodies': 1, 'volume_mm3': 200000-990*math.pi,
                            'exact_faces_max': 20000, 'visible_triangles_max': 500000},
               'actual_counts': None, 'evaluation': 'Not run: history evaluation Module 5; caps are not measured counts'})
    put('B3', {'recipe': {'definitions': [{'source': 'B1', 'width_mm': 80+i} for i in range(10)],
                         'occurrences': [{'id': f'occ-{i:03}', 'definition': i%10,
                                          'translation_mm': [100*(i%10), 80*(i//10), 0]} for i in range(100)],
                         'grounded': 'occ-000', 'mates': [{'type': 'distance', 'from': f'occ-{i-1:03}',
                           'to': f'occ-{i:03}', 'distance_mm': 100 if i%10 else math.hypot(900,80)} for i in range(1,100)]},
               'expected': {'occurrences': 100, 'unique_definitions': 10, 'mates': 99, 'visible_triangles_max': 1000000},
               'actual_counts': None, 'evaluation': 'Not run: assembly/history Module 7; underconstrained distance chain intentional'})
    put('B4', {'recipe': {'geometry': 'box', 'size_mm': [10,20,30], 'formats': ['STEP','IGES'],
                         'generator': 'tests/integration/cad_spike.cpp', 'generator_kernel': 'OCCT 7.9.2'},
               'expected': {'input_faces': 6, 'box_volume_mm3': 6000, 'source_units': 'mm',
                            'bytes_per_file_max': 25*1024*1024, 'faces_max': 50000},
               'byte_hash_policy': 'Translator header timestamps vary; retain each output hash in actual run evidence. Semantic recipe hash is deterministic.',
               'evaluation': 'Kernel-generated translator spike; curated damaged/colored/mixed-unit corpus remains Module 6'})
    put('B5', {'recipe': {'sheets': [{'size_mm': [297,210], 'source': 'B1', 'views': ['front','top','isometric']},
                                  {'size_mm': [297,210], 'source': 'B3', 'views': ['front','isometric'], 'bom': True}],
                         'scale': '1:2', 'projection': 'third-angle', 'annotations': [{'type': 'linear', 'value_mm': 80}]},
               'expected': {'sheets': 2, 'views': 5, 'annotations': 1, 'bom_quantity': 100, 'paper_dimension_mm': 40},
               'evaluation': 'Not run: associative drawing/PDF Module 8'})
    put('B6', {'recipe': {'corrupt_hex': {'truncated_sqlite': '53514c69746520666f726d6174203300',
                                       'oversize_frame': 'ffffffff', 'duplicate_map_key': 'a2617801617802',
                                       'nonfinite_cbor': 'a16178fb7ff0000000000000'},
                         'faults': ['before_commit','after_commit','during_snapshot','after_publication',
                                    'disk_full','permission_error','write_error','future_schema','missing_object',
                                    'old_epoch','old_generation','ambiguous_symmetric_faces','conflicting_distance']},
               'expected': {'prior_snapshot_preserved_before_publication': True, 'durable_command_count_after_retry': 1},
               'evaluation': 'See protocol/storage/CAD spike run logs; full application failure corpus Module 10'})
    return result

def encode(value):
    return (json.dumps(value, indent=2, sort_keys=True, allow_nan=False)+'\n').encode()

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
    manifest={}
    for name,value in fixtures().items():
        data=encode(value);path=ROOT/(name+'.json')
        manifest[name]={'file':path.name,'sha256':hashlib.sha256(data).hexdigest(),'version':1}
        if args.check:
            assert path.read_bytes()==data, f'Fixture drift: {name}'
        else:
            path.write_bytes(data)
    data=encode(manifest);path=ROOT/'manifest.json'
    if args.check:
        assert path.read_bytes()==data,'Manifest drift'
    else:
        path.write_bytes(data)
    print('Pass: B0–B6 recipes and hashes deterministic; unexecuted future-model counts explicitly pending')

if __name__=='__main__':main()
