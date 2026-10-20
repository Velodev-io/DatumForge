# SolidWorks UI design and interaction notes

Research date: 19 October 2026. The companion `index.html` is an offline reference atlas; `UI_SCREEN_CATALOG.md`, JSON and CSV contain the screen inventory. Original screenshots and their source links are preserved. No screenshot is presented as our own live SolidWorks capture.

## Scope and evidence

This pass covers the main CAD frame, startup, sketches, selected feature commands, assemblies, drawings, managers, customization, evaluation, Simulation, and the separate PDM client. SolidWorks has many conditional dialogs, add-ins, editions, and release-specific states. This is a broad inventory, not every page of every product.

The default image library shows only screenshots visually inspected during this pass. Other downloaded candidates can be revealed explicitly. A source title, image-search description, and screenshot contents can disagree; visible content takes precedence. Workflow notes are drawn from documentation and analysis, not from executing those operations in a running installation.

## The editor's spatial organization

Across the reviewed editor screenshots, stable responsibilities recur:

| Region | Typical contents | Design purpose | Reference |
|---|---|---|---|
| Top application row | Menus, document actions, search, application/window controls | Global navigation and file operations | IMG-059, IMG-079 |
| CommandManager below | Labeled command groups and mode-specific tabs | Task-oriented command discovery | IMG-006, IMG-008 |
| Left Manager Pane | Feature tree or command parameters, switched through tabs | Structure, selection and active operation inputs | IMG-024, IMG-030 |
| Upper graphics edge | Heads-up display/navigation tools | View control close to geometry | IMG-012, IMG-079 |
| Central graphics region | Model or drawing sheet, selection, previews and manipulators | Direct engineering interaction | IMG-024, IMG-066 |
| Right Task Pane | Resources, libraries, view thumbnails and integrations | Supporting content outside the active command | IMG-012, IMG-059 |
| Bottom region | Document/study tabs, edit state, units and status | Context and feedback | IMG-019, IMG-079 |

This is a qualitative placement map. Screenshots are cropped, resized, localized, customized and captured at different display scales. They do not establish universal pixel coordinates or fixed panel widths. See the official [UI overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm).

## Workspace changes are state changes

A part, assembly and drawing share a frame but differ in tool availability, tree semantics, graphics content, and supporting panels. A sketch edit can change available commands without creating a new application window. A feature command commonly occupies the existing left dock. A modal dialog is used for other tasks, including document creation and options.

The atlas therefore records a *surface*: workspace, dock, command panel, dialog, notification, toolbar or overlay. Counting every screenshot as a separate navigable page would lose this distinction.

## The left dock and right pane serve different jobs

The Manager Pane at left is central to structure and operation state. The FeatureManager presents history/components. The PropertyManager collects command inputs. ConfigurationManager and DisplayManager expose other model views. The dock can be split or customized; a single fixed left column is an incomplete implementation model. [Manager Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_management_panel.htm)

The right Task Pane holds supporting resources and reusable content. The captured drawing palette offers view thumbnails; the empty-editor screenshot offers resources. Connected users can see additional platform tabs. A right-side icon rail must therefore be contextual rather than a hard-coded universal list. [Task Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_task_pane.htm)

## Shared PropertyManager grammar

This is an analytical pattern derived from multiple actual panels, not a vendor promise that every command follows it:

1. Command name and help affordance at the top.
2. Accept/cancel controls near the command heading.
3. A selection prompt or message strip when inputs are incomplete.
4. Collapsible sections for selection roles and parameters.
5. Lists with colored selection accents and model-linked highlighting.
6. Numeric spin boxes, unit-bearing inputs, dropdowns and checkboxes.
7. A preview in the viewport, with applicable handles or callouts.

IMG-024 shows extrusion start/depth controls; IMG-028 shows fillet selections/radius; IMG-065 contrasts Loft input roles with Boundary directions; IMG-066 shows structural-profile and path groups. Reusing a visual scaffold is sensible, but replacing titles on an identical form would not reproduce the commands.

## Selection is a first-class interaction

Selection appears in several coordinated forms: tree highlighting, command input lists, model face/edge colors, preview geometry, and contextual controls. Author-added red arrows and yellow highlights must not be mistaken for SolidWorks controls. Only some screenshots show the actual selected entity state clearly.

Breadcrumbs add hierarchical selection navigation. Their default upper-left placement can move to the pointer, and visibility depends on context. Selection filters constrain entity types. Both systems require state and cannot be implemented as purely decorative toolbar icons. [Breadcrumb behavior](https://help.solidworks.com/2024/english/solidworks/sldworks/c_selection_breadcrumbs.htm)

## Confirmation and transient controls

Reviewed panels commonly have a green check and red cross near their heading. Several full editor images also show the Confirmation Corner in the upper-right graphics area. The cited historical documentation supports moving those controls to the pointer with the default D shortcut. Custom shortcuts and release differences matter. [Confirmation controls](https://help.solidworks.com/2016/english/whatsnew/c_command_confirmation.htm)

The Modify Dimension popup is a different surface: it appears alongside dimension editing rather than replacing every field in the left dock. IMG-073 contains an explicitly labeled 2025/2026 placement comparison. The image demonstrates the example difference, not the full positioning algorithm.

The Measure screenshot, IMG-080, is important because a Measure window, Dimension panel, Modify popup and model callouts coexist. A design that makes every dialog blocking would miss this demonstrated interaction.

## Visual language

The public references primarily use neutral gray chrome and large light graphics regions. Compact icons are frequently paired with short labels in command areas, while tab strips use icons to preserve dock space. Trees emphasize hierarchy through indentation, disclosure triangles and entity-specific icons. Divider lines and collapsible group headings organize dense forms.

Colors carry multiple roles: selected inputs, geometry highlighting, previews, constraints and notifications. In the inspected extrusion, yellow translucent geometry is a preview; in other images yellow is an author's annotation or a different model appearance. Do not infer one universal meaning from one color. Accessibility and user color settings require separate verification.

Typography, spacing, border styles and icon sizes vary visibly between older and newer references. The atlas does not claim exact font families, design tokens, DPI behavior or theme values for the whole product.

## Workflows recorded for implementation planning

These are documented/analytical sequences. None was executed in a live session during this pass.

| Workflow | UI progression | What must remain synchronized |
|---|---|---|
| New part and extrusion | New chooser → part workspace → plane/sketch → dimension → Boss-Extrude → preview → confirm | Active document, sketch support, selected contour and feature parameters |
| Standard hole | Hole Wizard Type → Positions → face/points → positional constraints → confirm | Specification and locations across two tabs |
| Assembly mate | Assembly → select references → Mate panel → relation/alignment → preview → confirm | Occurrence context, selection roles, constraint result |
| Drawing view | Drawing → model selection/View Palette → sheet placement → projected/annotation work | Referenced model/configuration, sheet scale and view state |
| Appearance change | Select model scope → appearance manager/library → inspect override | Component/part/body/feature/face scope and visual result |
| Evaluation | Choose entities/components → Measure or Interference → inspect readouts/results | Current geometry and the highlighted result |
| Simulation setup | Study tree → materials/connections → fixtures/loads → mesh action → analysis/results | Study context distinct from CAD feature history |
| PDM review | Vault file selection → preview/data card/history | Selected file, version, checkout state and metadata |

The [Hole Wizard documentation](https://help.solidworks.com/2026/English/SolidWorks/Sldworks/r_HIDD_DVE_HW_HOLE_SPEC.htm) supports separate Type and Positions responsibilities. The [drawing-view documentation](https://help.solidworks.com/2025/english/SolidWorks/sldworks/HIDD_DVE_NAMED_VIEW.htm) describes model/view placement. These references do not establish animation timing, mouse capture, focus restoration, or private implementation.

## Important version and product distinctions

- IMG-012 visibly identifies 2019 SP2.0 and German labels; it is useful for drawing placement, not a current-release specification.
- IMG-019 uses an Education/Simplified Interface configuration. Its Basic Modeling Tools tab is not proof of everyone's default tabs.
- IMG-053 and IMG-055 show different generations of mouse-gesture customization: guide wheels versus a grid.
- IMG-069 provides a 2026-context manager-tab customization example with some disabled choices.
- IMG-073 and IMG-074 are editorial comparisons explicitly labeled 2025 and 2026.
- IMG-079 says SOLIDWORKS Connected Premium; its platform-specific title and tabs must remain distinguished.
- IMG-045 and IMG-049 show a separate PDM Explorer-based client. They are not pages inside the ordinary part editor.
- IMG-071 is a compact Import Complete message. Its crop does not prove where a full import-progress panel sits.

## Corrections made after looking at the images

| Search/source implication | What the image actually establishes |
|---|---|
| Mirror article | IMG-028 visibly shows a Fillet panel, so it is cataloged as Fillet |
| Mate Controller article | IMG-009 shows the context-menu launch entry, not the controller panel |
| Smart Dimension screenshot | IMG-020 has Line Properties open and points toward the Smart Dimension command |
| FeatureManager menu description | IMG-032 is a side-by-side tree naming-density comparison |
| 2026 sketch tutorial | IMG-070 shows an empty part with sketch tools exposed; active sketch editing is not established |
| New filter descriptions | IMG-074 shows toolbar changes, but unlabeled icon meanings are not guessed |

## Coverage gaps and next reference targets

The following need dedicated visual capture or stronger release-specific references before implementing their exact layouts:

- Complete Open, Save As, Pack and Go, and dependency-reference dialogs.
- Primitive sketch-tool panels, SketchXpert, and 3D-sketch-specific interactions.
- Revolve, Sweep, Shell, Draft, Pattern, Mirror, and reference-geometry panels.
- Full Mate Controller, Exploded View, Move Component, and component-replacement dialogs.
- Drawing Sheet Properties, Sheet Format editing, section/detail creation, full BOM setup and GD&T editors.
- Base/Edge Flange previews, Flat Pattern settings, weldment trimming and cut-list property editors.
- Simulation load/fixture PropertyManagers, full mesh settings, solver options and result plots.
- Complete CAM, Electrical, Routing, Plastics, Flow Simulation, Visualize, and browser-native product interfaces.
- Live hover/focus behavior, keyboard traversal, drag/drop acceptance, cancel recovery, errors and timing.

Several assets were blocked or unavailable from their origin. Their URLs remain in `selected_images.json`, and the atlas marks them as not downloaded. No placeholder is passed off as a screenshot.

## Applying these observations to the clone

Start with the shared frame and explicit interaction states. Separate the model tree, active command editor, content Task Pane and transient overlays. Make command forms reflect distinct input roles. Keep document type, edit mode, selection, configuration, preview and notification state explicit. Show unavailable functionality as a deliberate product decision rather than a clickable icon with no meaningful result.

Use this atlas as a behavioral and spatial reference. Screenshots remain owned by their original publishers; the atlas preserves provenance and does not grant redistribution rights or supply proprietary implementation code.
