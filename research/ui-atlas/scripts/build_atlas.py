#!/usr/bin/env python3
"""Build an offline, source-linked UI atlas. No network access is performed."""
from pathlib import Path
from collections import Counter
import csv
import hashlib
import html
import json
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def normalize_assets():
    path = ROOT / 'selected_images.json'
    rows = json.loads(path.read_text())
    current_source = 'https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new'
    hole_source = 'https://www.goengineer.com/blog/power-and-value-solidworks-hole-wizard'
    additions = [
        (76, 'modeling', 'Hole Wizard specification options', hole_source, 'https://cdn.goengineer.com/solidworks-hole-specification-options.png?format=webp'),
        (77, 'modeling', 'Hole Wizard Positions tab', hole_source, 'https://cdn.goengineer.com/solidworks-hole-wizard-positions-tab.png?format=webp'),
        (78, 'modeling', 'Hole position dimensions', hole_source, 'https://cdn.goengineer.com/solidworks-define-hole-position.png?format=webp'),
        (79, 'assembly', 'Mate PropertyManager in an assembly', 'https://www.swyftsol.com/blog/how-to-create-an-assembly-in-solidworks', 'https://images.squarespace-cdn.com/content/v1/5ed532ab341bc74ec50c77ff/8d9ad948-fb34-4252-a600-b8fe6928b181/Mating%2BComponents%2Bin%2BSOLIDWORKS%2B-%2B1.png?format=2500w'),
        (80, 'evaluate', 'Measure tool and Modify dialog', 'https://hawkridgesys.com/blog/whats-new-solidworks-2019-measure-tool-enhancements', 'https://embed-ssl.wistia.com/deliveries/74c93519fe0d2d3bd9281caf30e54c5d164c6a1a.webp?image_crop_resized=1916x1080'),
    ]
    for n, category, title, source, image in additions:
        if not any(r['id'] == f'IMG-{n:03}' for r in rows):
            rows.append(dict(id=f'IMG-{n:03}', category=category, title=title, source_url=source,
                             image_url=image, local_path=f'images/img-{n:03}.asset',
                             visual_review='pending', release='Not established', image_kind='Unclassified'))
    reviews = json.loads((ROOT / 'visual_reviews.json').read_text())
    for row in rows:
        file = ROOT / row['local_path']
        if file.is_file():
            try:
                with Image.open(file) as im:
                    im.verify()
                with Image.open(file) as im:
                    row.update(width=im.width, height=im.height, format=im.format)
                    extension = {'PNG': 'png', 'JPEG': 'jpg', 'WEBP': 'webp', 'GIF': 'gif', 'BMP': 'bmp'}.get(im.format, 'asset')
                target = file.with_suffix('.' + extension)
                if file != target:
                    file.rename(target)
                row.update(local_path=str(target.relative_to(ROOT)), download_status='downloaded',
                           sha256=hashlib.sha256(target.read_bytes()).hexdigest())
            except Exception as error:
                row['download_status'] = 'invalid image: ' + str(error)[:100]
        else:
            row['download_status'] = 'not downloaded'
            row['access_note'] = 'Source retained. Batch downloads encountered HTTP denials, missing assets and timeouts; exact status was not recorded per URL.'
        if row['id'] in reviews:
            row.update(reviews[row['id']])
            row['visual_review'] = 'inspected'
            row['reviewed_at'] = '2026-10-19'
        else:
            row.setdefault('label', row['title'])
            row.setdefault('kind', 'Reference awaiting visual review')
    path.write_text(json.dumps(rows, indent=2, ensure_ascii=False) + '\n')
    return rows


def build():
    assets = normalize_assets()
    screens = json.loads((ROOT / 'screen_catalog.json').read_text())
    sources = json.loads((ROOT / 'documentation_sources.json').read_text())
    image_by_id = {r['id']: r for r in assets}
    for screen in screens:
        screen['evidence'] = 'Visual reference' if any(image_by_id[i]['visual_review'] == 'inspected' for i in screen['images']) else 'Documentation only'
        screen['workflow_status'] = 'Documented or inferred sequence; not executed in a live SolidWorks session'
    (ROOT / 'screen_catalog.json').write_text(json.dumps(screens, indent=2, ensure_ascii=False) + '\n')
    stats = dict(screen_surfaces=len(screens), image_candidates=len(assets),
                 saved_images=sum(r['download_status'] == 'downloaded' for r in assets),
                 inspected_images=sum(r['visual_review'] == 'inspected' for r in assets),
                 visually_supported_surfaces=sum(r['evidence'] == 'Visual reference' for r in screens),
                 reference_sites=len(set(re.match(r'https?://([^/]+)', r['source_url'])[1] for r in assets)),
                 documentation_sources=len(sources), generated_at='2026-10-19',
                 live_solidworks_sessions=0)
    (ROOT / 'coverage.json').write_text(json.dumps(stats, indent=2) + '\n')
    with (ROOT / 'screen_catalog.csv').open('w', newline='') as stream:
        fields = ['id', 'category', 'name', 'surface', 'placement', 'entry', 'controls', 'usage', 'evidence', 'images', 'sources']
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        for screen in screens:
            writer.writerow({k: '; '.join(screen[k]) if isinstance(screen[k], list) else screen[k] for k in fields})
    lines = ['# SolidWorks UI screen catalog', '',
             'Captured 19 October 2026. A screen surface means a workspace, panel, dialog, or transient UI state. This is a broad public-source inventory, not every screen in every edition.', '',
             'Entry paths and usage sequences are documentation-based or analytical notes. They have not been executed in a live SolidWorks session. Image release labels are independent of documentation release labels.', '']
    for category in dict.fromkeys(s['category'] for s in screens):
        lines += ['## ' + category, '']
        for s in [x for x in screens if x['category'] == category]:
            lines += ['### ' + s['id'] + ' — ' + s['name'], '',
                      f"- Surface: {s['surface']}; evidence: {s['evidence']}.",
                      '- Placement: ' + s['placement'], '- Open/use: ' + s['entry'],
                      '- Controls: ' + s['controls'], '- Purpose/workflow: ' + s['usage']]
            for i in s['images']:
                a = image_by_id[i]
                lines += [f"- [{i}: {a['label']}]({a['source_url']}) — {a['release']}; {a['visual_review']}."]
            for key in s['sources']:
                source = sources[key]
                lines += [f"- Documentation: [{source['title']}]({source['url']}) ({source['release']})."]
            lines += ['']
    (ROOT / 'UI_SCREEN_CATALOG.md').write_text('\n'.join(lines))
    data = dict(assets=assets, screens=screens, sources=sources, stats=stats)
    template = (ROOT / 'atlas_template.html').read_text()
    payload = json.dumps(data, ensure_ascii=False).replace('</', '<\\/')
    (ROOT / 'index.html').write_text(template.replace('__ATLAS_DATA__', payload))
    print(json.dumps(stats, indent=2))


if __name__ == '__main__':
    build()
