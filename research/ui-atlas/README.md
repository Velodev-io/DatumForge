# SolidWorks UI research atlas

Open `index.html` directly in a modern browser. Keep the adjacent `images/` folder intact. The viewer embeds its catalog and uses local images, so it does not require a server or external JavaScript libraries. Original-source links require internet access.

This snapshot, researched on 19 October 2026, contains:

- 64 cataloged UI surfaces across 13 categories.
- 80 selected image references from 19 source hostnames.
- 69 locally saved images; 38 were visually inspected.
- Reviewed visual support for 52 catalog surfaces; 12 are documentation-only.
- 25 version-labeled documentation references.

The atlas is a public-source study, not a live recording of SolidWorks and not a complete inventory of every release, add-in, or conditional dialog. Placement and workflow notes distinguish visual evidence from documented behavior and inference. Some references are older, localized, customized, or specific to SOLIDWORKS Connected or PDM.

## Files

| File | Purpose |
|---|---|
| `index.html` | Searchable viewer: workspace anatomy, screen map, image library, workflows, sources and gaps |
| `UI_DESIGN_NOTES.md` | Placement, panel grammar, interaction patterns, version differences and implementation implications |
| `UI_SCREEN_CATALOG.md` | Detailed source-linked surface inventory |
| `screen_catalog.csv` / `.json` | Structured screen catalog |
| `selected_images.json` | Image origin URLs, local paths, dimensions, hashes and review status |
| `visual_reviews.json` | Curated observations tied to specific image IDs |
| `documentation_sources.json` | Official documentation references |
| `coverage.json` | Coverage metrics |
| `validation.json` | Validation evidence and limits |
| `images/` | Original image files; pixels were not edited |
| `preview.jpg` | Screenshot of the atlas viewer |

## Rebuilding

The viewer needs no dependencies. Rebuilding from the maintained catalog inputs uses Python with Pillow:

```sh
python3 scripts/create_catalog.py
python3 scripts/build_atlas.py
```

`create_catalog.py` regenerates the screen/source records from the curated inventory. `build_atlas.py` checks and indexes already-downloaded images, adds visual-review notes, and generates the HTML/Markdown/CSV outputs. Neither script downloads anything. It preserves missing asset references rather than inventing replacement images.

Original screenshot copyrights remain with their publishers. Source attribution is attached to every record. This collection grants no additional rights to republish third-party assets.
