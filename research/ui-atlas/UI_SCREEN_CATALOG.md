# SolidWorks UI screen catalog

Captured 19 October 2026. A screen surface means a workspace, panel, dialog, or transient UI state. This is a broad public-source inventory, not every screen in every edition.

Entry paths and usage sequences are documentation-based or analytical notes. They have not been executed in a live SolidWorks session. Image release labels are independent of documentation release labels.

## Start and files

### UI-001 — Welcome / Home

- Surface: Dialog; evidence: Visual reference.
- Placement: Centered startup dialog; tabs above action and thumbnail groups.
- Open/use: Welcome/Home command or startup.
- Controls: New Part/Assembly/Drawing, Open, recent thumbnails.
- Purpose/workflow: Choose a document or reuse recent work; image is a 2020 reference, not a 2026 pixel specification.
- [IMG-002: Welcome / Home with recent-document options](https://blogs.solidworks.com/solidworksblog/2019/09/whats-new-2020-performance-improvements.html) — 2020 article context; exact build not visible; inspected.
- Documentation: [Welcome Dialog Box](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_welcome_dialog_box.htm) (2026).
- Documentation: [Home Tab](https://help.solidworks.com/2025/English/SolidWorks/sldworks/c_home_tab.htm) (2025).

### UI-002 — Recent-document opening options

- Surface: Popover; evidence: Visual reference.
- Placement: Attached to a recent-document tile.
- Open/use: Open dropdown or options on a recent item.
- Controls: Mode and configuration fields in the captured example.
- Purpose/workflow: Choose how to load an existing document. Available modes depend on document type and release.
- [IMG-002: Welcome / Home with recent-document options](https://blogs.solidworks.com/solidworksblog/2019/09/whats-new-2020-performance-improvements.html) — 2020 article context; exact build not visible; inspected.
- Documentation: [Home Tab](https://help.solidworks.com/2025/English/SolidWorks/sldworks/c_home_tab.htm) (2025).

### UI-003 — New document — basic

- Surface: Dialog; evidence: Visual reference.
- Placement: Centered chooser with three document types; action buttons along bottom.
- Open/use: File > New / New icon.
- Controls: Part, Assembly, Drawing; Advanced; OK/Cancel/Help.
- Purpose/workflow: Select the required document category, then confirm creation.
- [IMG-059: New SOLIDWORKS Document — basic chooser](https://www.goengineer.com/fr-ca/blog/mastering-basic-part-modeling-solidworks) — Release not visible; English UI; inspected.
- Documentation: [Creating a New SOLIDWORKS Document](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_creating_new_sw_document.htm) (2026).

### UI-004 — New document — advanced

- Surface: Dialog; evidence: Visual reference.
- Placement: Template categories across top, list left and preview right.
- Open/use: New document > Advanced.
- Controls: Template tabs, template list, preview, OK/Cancel/Help.
- Purpose/workflow: Choose a configured template rather than assuming every new file uses identical defaults.
- [IMG-060: New SOLIDWORKS Document — advanced templates](https://www.goengineer.com/blog/solidworks-default-templates-error) — Release not visible; inspected.
- Documentation: [Creating a New SOLIDWORKS Document](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_creating_new_sw_document.htm) (2026).

## Editor shell

### UI-005 — No active document

- Surface: Workspace; evidence: Visual reference.
- Placement: Top application commands, mostly empty viewport, Task Pane on right.
- Open/use: Launch application without an open model.
- Controls: New/Open and resources.
- Purpose/workflow: The editor frame remains available before any part, assembly or drawing is active.
- [IMG-059: New SOLIDWORKS Document — basic chooser](https://www.goengineer.com/fr-ca/blog/mastering-basic-part-modeling-solidworks) — Release not visible; English UI; inspected.
- [IMG-070: 2026 French editor shell](https://www.elephorm.com/formation/maitriser-solidworks-2026-les-fondamentaux/decouvrir-les-outils-de-dessin-fondamentaux-dans-solidworks) — 2026 SP1.0 appears in status; low-resolution; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-006 — Part workspace

- Surface: Workspace; evidence: Visual reference.
- Placement: Horizontal command area above central model; manager dock on left.
- Open/use: Open or create a part.
- Controls: Features/Sketch/Evaluate tabs, history tree, graphics controls.
- Purpose/workflow: Select model entities or tree items, launch a feature, and inspect the result.
- [IMG-006: Part editor / Features CommandManager](https://cadcamlessons.com/hole-wizard-solidworks-tutorial/) — Release not visible; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-007 — Assembly workspace

- Surface: Workspace; evidence: Visual reference.
- Placement: Assembly tools above graphics; component hierarchy at left.
- Open/use: Open or create an assembly.
- Controls: Insert Components, Mate and component actions in the captured toolbar.
- Purpose/workflow: Manage occurrences and constraints; do not reuse a part toolbar indiscriminately.
- [IMG-008: Assembly workspace / Mate launch location](https://www.goengineer.com/blog/introduction-to-solidworks-mates) — Release not visible; inspected.
- [IMG-079: Mate PropertyManager in SOLIDWORKS Connected](https://www.swyftsol.com/blog/how-to-create-an-assembly-in-solidworks) — Connected Premium visible; article describes 2023; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-008 — Drawing workspace

- Surface: Workspace; evidence: Visual reference.
- Placement: Sheet central; sheet/view tree left; annotation tools top; optional palette right.
- Open/use: Open or create a drawing.
- Controls: Annotation commands, views, tables, sheet tabs.
- Purpose/workflow: Place and annotate model views within a drawing document.
- [IMG-012: Drawing editor with View Palette and BOM](https://blogs.solidworks.com/solidworksdeutschland/2019/08/technik-tipp-solidworks-stueckliste-erweitern.html) — 2019 SP2.0 visible; German UI; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-009 — CommandManager

- Surface: Toolbar; evidence: Visual reference.
- Placement: Usually below the menu/quick-access row in these references.
- Open/use: Select a command tab.
- Controls: Grouped icons, labels, flyouts and document-dependent tabs.
- Purpose/workflow: Expose task-specific commands while keeping the model visible. Docking and customization alter placement.
- [IMG-006: Part editor / Features CommandManager](https://cadcamlessons.com/hole-wizard-solidworks-tutorial/) — Release not visible; inspected.
- [IMG-008: Assembly workspace / Mate launch location](https://www.goengineer.com/blog/introduction-to-solidworks-mates) — Release not visible; inspected.
- [IMG-019: Sketch editing in Simplified Interface](https://bournetoinvent.com/index.php?catid=9&id=21&view=article) — Education edition; exact release not visible; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-010 — Heads-up View toolbar

- Surface: Toolbar; evidence: Visual reference.
- Placement: Near the upper edge of the graphics area.
- Open/use: Visible in an active document.
- Controls: View/navigation/display controls.
- Purpose/workflow: Operate the camera and display without switching command tabs. Exact icon set varies.
- [IMG-012: Drawing editor with View Palette and BOM](https://blogs.solidworks.com/solidworksdeutschland/2019/08/technik-tipp-solidworks-stueckliste-erweitern.html) — 2019 SP2.0 visible; German UI; inspected.
- [IMG-070: 2026 French editor shell](https://www.elephorm.com/formation/maitriser-solidworks-2026-les-fondamentaux/decouvrir-les-outils-de-dessin-fondamentaux-dans-solidworks) — 2026 SP1.0 appears in status; low-resolution; inspected.
- [IMG-079: Mate PropertyManager in SOLIDWORKS Connected](https://www.swyftsol.com/blog/how-to-create-an-assembly-in-solidworks) — Connected Premium visible; article describes 2023; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-011 — Command search

- Surface: Control; evidence: Visual reference.
- Placement: Upper-right application frame in the reviewed screenshots.
- Open/use: Click Search Commands.
- Controls: Search field and dropdown.
- Purpose/workflow: Find commands when their toolbar position is unknown; exact search modes need live validation.
- [IMG-059: New SOLIDWORKS Document — basic chooser](https://www.goengineer.com/fr-ca/blog/mastering-basic-part-modeling-solidworks) — Release not visible; English UI; inspected.
- [IMG-080: Measure dialog while editing a dimension](https://hawkridgesys.com/blog/whats-new-solidworks-2019-measure-tool-enhancements) — 2019 article context; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

### UI-012 — Document/study tabs and status

- Surface: Status area; evidence: Visual reference.
- Placement: Along the bottom of the document frame.
- Open/use: Visible while editing.
- Controls: Model/study tabs; edit context, definition state and unit indicators.
- Purpose/workflow: Keep active task and model state visible. Do not confuse sketch definition with cloud save status.
- [IMG-019: Sketch editing in Simplified Interface](https://bournetoinvent.com/index.php?catid=9&id=21&view=article) — Education edition; exact release not visible; inspected.
- [IMG-030: ConfigurationManager and Display States](https://www.tfordesign.com/designpurrs/category/solidworks-3d-modeling) — Release not visible; inspected.
- [IMG-079: Mate PropertyManager in SOLIDWORKS Connected](https://www.swyftsol.com/blog/how-to-create-an-assembly-in-solidworks) — Connected Premium visible; article describes 2023; inspected.
- Documentation: [User Interface Overview](https://help.solidworks.com/2025/english/SolidWorks/sldworks/c_user_interface_overview.htm) (2025).

## Managers and libraries

### UI-013 — FeatureManager design tree

- Surface: Dock; evidence: Visual reference.
- Placement: Left manager pane below its tabs and filter.
- Open/use: Select the FeatureManager tab.
- Controls: Expandable nodes, feature/component icons, selection and context menus.
- Purpose/workflow: Navigate design structure and select model references. Labels and visible folders are customizable.
- [IMG-006: Part editor / Features CommandManager](https://cadcamlessons.com/hole-wizard-solidworks-tutorial/) — Release not visible; inspected.
- [IMG-032: FeatureManager naming-density comparison](https://blogs.solidworks.com/products/solidworks/four-featuremanager-features-everyone-should-know/) — Release not visible; inspected.
- Documentation: [Manager Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_management_panel.htm) (2026).

### UI-014 — PropertyManager common structure

- Surface: Dock; evidence: Visual reference.
- Placement: Left manager pane while a command is active.
- Open/use: Start a supported command.
- Controls: Command title, confirm/cancel controls, input groups and selections.
- Purpose/workflow: Collect inputs without a large modal obscuring the model; shared layout is an analytical pattern.
- [IMG-024: Boss-Extrude with live preview](https://cadcamlessons.com/extruded-boss-solidworks-tutorial/) — Release not visible; inspected.
- [IMG-028: Fillet PropertyManager — mislabeled source context](https://www.cad100.jp/review/solidworks/review_solidworks_20190712.php) — Release not visible; Japanese UI; inspected.
- [IMG-065: Loft versus Boundary PropertyManagers](https://www.goengineer.com/blog/solidworks-loft-vs-boundary-key-differences) — Release not visible; inspected.
- Documentation: [Manager Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_management_panel.htm) (2026).

### UI-015 — ConfigurationManager

- Surface: Dock; evidence: Visual reference.
- Placement: Left pane; configuration list above a separate display-state section.
- Open/use: Select the configuration tab.
- Controls: Configuration rows and active-state indicator.
- Purpose/workflow: Choose a design variant; the reviewed GIF frame is a visual reference only.
- [IMG-030: ConfigurationManager and Display States](https://www.tfordesign.com/designpurrs/category/solidworks-3d-modeling) — Release not visible; inspected.
- Documentation: [Manager Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_management_panel.htm) (2026).

### UI-016 — Display States list

- Surface: Dock; evidence: Visual reference.
- Placement: Lower section of ConfigurationManager.
- Open/use: Open ConfigurationManager and choose a display state.
- Controls: State entries and contextual properties.
- Purpose/workflow: Switch presentation state; linking to configurations is a separate option.
- [IMG-030: ConfigurationManager and Display States](https://www.tfordesign.com/designpurrs/category/solidworks-3d-modeling) — Release not visible; inspected.
- Documentation: [Display State Mode](https://help.solidworks.com/2025/english/Solidworks/sldworks/c_Display_State_Mode.htm) (2025).
- Documentation: [Changing from One Display State to Another](https://help.solidworks.com/2026/english/SolidWorks/sldworks/t_Changing_from_One_Display_State_to_Another.htm) (2026).

### UI-017 — DisplayManager / Appearances

- Surface: Dock; evidence: Visual reference.
- Placement: Left manager pane with its own sub-tabs and sorting control.
- Open/use: Select the multicolor appearance-manager tab.
- Controls: Appearance hierarchy, sort order, scope groups.
- Purpose/workflow: Inspect visual overrides at component, face, feature, body and part levels.
- [IMG-031: DisplayManager appearance hierarchy](https://www.goengineer.com/blog/understanding-hierarchy-solidworks-appearances) — Release not visible; inspected.
- Documentation: [Manager Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_management_panel.htm) (2026).

### UI-018 — Task Pane / Resources

- Surface: Dock; evidence: Visual reference.
- Placement: Right-hand expandable pane next to a vertical tab strip.
- Open/use: Select Resources in the Task Pane.
- Controls: Resource groups and utility links.
- Purpose/workflow: Keep reusable content and supporting tools separate from command parameters.
- [IMG-059: New SOLIDWORKS Document — basic chooser](https://www.goengineer.com/fr-ca/blog/mastering-basic-part-modeling-solidworks) — Release not visible; English UI; inspected.
- Documentation: [Task Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_task_pane.htm) (2026).

### UI-019 — Design Library / Toolbox

- Surface: Dock; evidence: Documentation only.
- Placement: Right Task Pane. Exact panel crop is not captured.
- Open/use: Select Design Library.
- Controls: Library and reusable-content navigation.
- Purpose/workflow: Browse and drag suitable reusable design content into the document.
- Documentation: [Task Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_task_pane.htm) (2026).

### UI-020 — File Explorer task tab

- Surface: Dock; evidence: Documentation only.
- Placement: Right Task Pane. Distinct from the standalone PDM window.
- Open/use: Select the File Explorer task tab.
- Controls: Folder/document browsing.
- Purpose/workflow: Find files without leaving the CAD editor; integration can change its available tabs.
- Documentation: [Task Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_task_pane.htm) (2026).

### UI-021 — View Palette

- Surface: Dock; evidence: Visual reference.
- Placement: Right Task Pane alongside the drawing sheet.
- Open/use: Open View Palette in a drawing.
- Controls: Model chooser, import options and view thumbnails.
- Purpose/workflow: Drag a chosen model view onto a sheet; follow with projected views when appropriate.
- [IMG-012: Drawing editor with View Palette and BOM](https://blogs.solidworks.com/solidworksdeutschland/2019/08/technik-tipp-solidworks-stueckliste-erweitern.html) — 2019 SP2.0 visible; German UI; inspected.
- Documentation: [Task Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_task_pane.htm) (2026).

### UI-022 — DimXpertManager

- Surface: Dock; evidence: Documentation only.
- Placement: Left manager-tab family; exact content screenshot not captured.
- Open/use: Select DimXpertManager.
- Controls: Tolerance-feature list and supported dimension/tolerance commands.
- Purpose/workflow: Inspect manufacturing annotation structure. Availability is product-dependent.
- Documentation: [Manager Pane](https://help.solidworks.com/2026/english/SolidWorks/sldworks/c_management_panel.htm) (2026).

## Selection and overlays

### UI-023 — Selection breadcrumbs

- Surface: Overlay; evidence: Documentation only.
- Placement: Upper-left graphics area by default in the cited help; optionally at pointer.
- Open/use: Select an eligible entity or tree node.
- Controls: Hierarchical references and contextual actions.
- Purpose/workflow: Navigate selection context. The cited version excludes several selection/command states.
- Documentation: [Selection Breadcrumbs](https://help.solidworks.com/2024/english/solidworks/sldworks/c_selection_breadcrumbs.htm) (2024).

### UI-024 — Confirmation Corner

- Surface: Overlay; evidence: Visual reference.
- Placement: Upper-right graphics corner in the reviewed command views.
- Open/use: Start a command with confirmation controls.
- Controls: Accept/cancel or command-specific exit controls.
- Purpose/workflow: Finish or abandon the active operation without returning to the left panel.
- [IMG-066: Weldments — Structural Member](https://www.goengineer.com/blog/solidworks-weldment-profiles-weldment-tools-guide) — Release not visible; inspected.
- [IMG-079: Mate PropertyManager in SOLIDWORKS Connected](https://www.swyftsol.com/blog/how-to-create-an-assembly-in-solidworks) — Connected Premium visible; article describes 2023; inspected.
- Documentation: [Moving the Confirmation Corner Options to the Pointer](https://help.solidworks.com/2016/english/whatsnew/c_command_confirmation.htm) (2016; historical behavior reference).

### UI-025 — Feature/component context menu

- Surface: Overlay; evidence: Visual reference.
- Placement: Beside the clicked tree item or selected object.
- Open/use: Right-click eligible selections.
- Controls: Context actions and sometimes a small icon toolbar.
- Purpose/workflow: Expose actions appropriate to the selected entity and current mode.
- [IMG-009: Mate Controller context-menu entry](https://www.goengineer.com/blog/solidworks-mate-controller-explained) — Release not visible; inspected.
- [IMG-035: Simulation Mesh context menu](https://blogs.solidworks.com/teacher/2025/05/lesson-3-solidworks-optimization-analysis-using-a-topology-study.html) — 2025 lesson context; inspected.

### UI-026 — Selection filters

- Surface: Toolbar; evidence: Visual reference.
- Placement: Bottom filter toolbar in the reviewed comparison.
- Open/use: Show or use the selection-filter toolbar.
- Controls: Entity-type filter icons and filter-state controls.
- Purpose/workflow: Restrict selectable entity types; 2025/2026 images show different icon sets.
- [IMG-074: Selection filters — 2025 versus 2026](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — Both releases explicitly labeled; inspected.

### UI-027 — Customize — Mouse Gestures

- Surface: Dialog; evidence: Visual reference.
- Placement: Tabbed Customize dialog with command list and environment-specific mapping guide.
- Open/use: Tools > Customize > Mouse Gestures.
- Controls: Enable checkbox, gesture count, command search and guide wheels.
- Purpose/workflow: Assign gestures separately for part, sketch, assembly and drawing contexts.
- [IMG-053: Customize — Mouse Gestures with guide wheels](https://blogs.solidworks.com/solidworksdeutschland/2023/07/maus-wege-einsparen-in-solidworks.html) — 2023 article context; German UI; inspected.
- [IMG-055: Older Customize — Mouse Gestures grid](https://www.goengineer.com/blog/setting-up-solidworks-efficient-effective-design-team) — Older release; exact build not visible; inspected.

### UI-063 — Shortcut Bar

- Surface: Overlay; evidence: Documentation only.
- Placement: Near the pointer; exact visual crop not captured.
- Open/use: Press S by default; configurable.
- Controls: Mode-specific commands and command search.
- Purpose/workflow: Use a compact customizable command set for part, assembly, drawing or sketch mode.
- Documentation: [Shortcut Bars](https://help.solidworks.com/2026/english/solidworks/sldworks/c_shortcut_bars.htm) (2026).

### UI-064 — Orientation / View Selector

- Surface: Floating control; evidence: Documentation only.
- Placement: Over the graphics area; precise dimensions depend on release and layout.
- Open/use: Press Spacebar or use View Orientation.
- Controls: Standard orientations and custom views.
- Purpose/workflow: Change the view independently from model geometry.
- Documentation: [Orientation](https://help.solidworks.com/2023/english/SolidWorks/acadhelp/c_Orientation.htm) (2023).

## Sketching

### UI-028 — Sketch editing workspace

- Surface: Workspace; evidence: Visual reference.
- Placement: Sketch toolbar above planar geometry; tree left and definition state below.
- Open/use: Choose a plane/face and enter sketch editing.
- Controls: Sketch entities, dimensions, relations and Exit Sketch.
- Purpose/workflow: Create a profile and constrain intended motion before using it in a feature.
- [IMG-019: Sketch editing in Simplified Interface](https://bournetoinvent.com/index.php?catid=9&id=21&view=article) — Education edition; exact release not visible; inspected.
- [IMG-070: 2026 French editor shell](https://www.elephorm.com/formation/maitriser-solidworks-2026-les-fondamentaux/decouvrir-les-outils-de-dessin-fondamentaux-dans-solidworks) — 2026 SP1.0 appears in status; low-resolution; inspected.

### UI-029 — Line Properties

- Surface: Command panel; evidence: Visual reference.
- Placement: Left PropertyManager for a selected line.
- Open/use: Select a sketch line.
- Controls: Existing relations, construction option and parameters.
- Purpose/workflow: Inspect the line and its relations; direct parameters differ from dimensional design intent.
- [IMG-020: Sketch Line Properties with Smart Dimension launch](https://www.goengineer.com/blog/7-important-tips-for-new-solidworks-users) — Release not visible; inspected.
- Documentation: [Add Relations/Properties PropertyManager](https://help.solidworks.com/2024/English/SolidWorks/sldworks/HIDD_DVE_SKETCH_DEFAULT.htm) (2024).

### UI-030 — Smart Dimension launch and placement

- Surface: Command; evidence: Visual reference.
- Placement: Sketch command area plus in-graphics dimension placement.
- Open/use: Choose Smart Dimension and relevant entities.
- Controls: Entity selection and dimension placement.
- Purpose/workflow: Apply an appropriate dimension; panel and popup vary with the selected dimension.
- [IMG-019: Sketch editing in Simplified Interface](https://bournetoinvent.com/index.php?catid=9&id=21&view=article) — Education edition; exact release not visible; inspected.
- [IMG-020: Sketch Line Properties with Smart Dimension launch](https://www.goengineer.com/blog/7-important-tips-for-new-solidworks-users) — Release not visible; inspected.
- Documentation: [Modify Dimension](https://help.solidworks.com/2024/english/solidworks/sldworks/t_modify_dimension_dialog_box.htm) (2024).

### UI-031 — Modify Dimension popup

- Surface: Floating dialog; evidence: Visual reference.
- Placement: Near the edited dimension or remembered popup position.
- Open/use: Double-click an editable dimension.
- Controls: Value/name, check/cross, numeric increments and command controls.
- Purpose/workflow: Edit the value and confirm. A 2025/2026 comparison is included; exact positioning is not universal.
- [IMG-073: Modify dimension — 2025 versus 2026](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — Both releases explicitly labeled in comparison; inspected.
- [IMG-080: Measure dialog while editing a dimension](https://hawkridgesys.com/blog/whats-new-solidworks-2019-measure-tool-enhancements) — 2019 article context; inspected.
- Documentation: [Modify Dimension](https://help.solidworks.com/2024/english/solidworks/sldworks/t_modify_dimension_dialog_box.htm) (2024).

### UI-032 — Add Relations

- Surface: Command panel; evidence: Documentation only.
- Placement: Left PropertyManager; geometry selected in viewport.
- Open/use: Dimensions/Relations > Add Relations.
- Controls: Selected entities, existing relations and available new relations.
- Purpose/workflow: Select compatible entities and apply a relation appropriate to them.
- Documentation: [Add Relations/Properties PropertyManager](https://help.solidworks.com/2024/English/SolidWorks/sldworks/HIDD_DVE_SKETCH_DEFAULT.htm) (2024).

### UI-033 — Display/Delete Relations

- Surface: Command panel; evidence: Documentation only.
- Placement: PropertyManager linked to highlighted sketch geometry.
- Open/use: Dimensions/Relations > Display/Delete Relations.
- Controls: Relation filter, relation list and associated entities.
- Purpose/workflow: Inspect and remove unwanted, dangling or conflicting relations; exact panel appearance not captured.
- Documentation: [Display/Delete Relations PropertyManager](https://help.solidworks.com/2025/english/Solidworks/sldworks/HIDD_DVE_SK_EDIT_RELATIONS.htm) (2025).

## Features and surfaces

### UI-034 — Boss-Extrude

- Surface: Command panel; evidence: Visual reference.
- Placement: Left input groups with volumetric preview in the graphics area.
- Open/use: Features > Extruded Boss/Base on a suitable sketch.
- Controls: From, Direction 1, depth, optional second direction and thin-feature controls.
- Purpose/workflow: Set start/end conditions and depth, inspect preview, then accept or cancel.
- [IMG-023: Extrude PropertyManager crop](https://blogs.solidworks.com/solidworksdeutschland/2020/05/die-solidworks-benutzeroberflaeche-einfach-erklaert.html) — 2020 source article; German UI; inspected.
- [IMG-024: Boss-Extrude with live preview](https://cadcamlessons.com/extruded-boss-solidworks-tutorial/) — Release not visible; inspected.
- Documentation: [Extrude PropertyManager](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_extrude_propertymanager.htm) (2026).

### UI-035 — Extruded Cut

- Surface: Command panel; evidence: Documentation only.
- Placement: Uses the extrusion-command family; exact cut screenshot not captured.
- Open/use: Features > Extruded Cut.
- Controls: End conditions, depth and applicable scope options.
- Purpose/workflow: Use selected contours to remove material; do not present boss-only options as universal.
- Documentation: [Extrude PropertyManager](https://help.solidworks.com/2026/english/SolidWorks/sldworks/r_extrude_propertymanager.htm) (2026).

### UI-036 — Fillet

- Surface: Command panel; evidence: Visual reference.
- Placement: Left selection/radius controls; blend preview and callouts on geometry.
- Open/use: Features > Fillet, then choose suitable edges/faces.
- Controls: Selected items, radius, propagation and preview options.
- Purpose/workflow: Choose references, set size, inspect blend preview and commit.
- [IMG-028: Fillet PropertyManager — mislabeled source context](https://www.cad100.jp/review/solidworks/review_solidworks_20190712.php) — Release not visible; Japanese UI; inspected.

### UI-037 — Loft

- Surface: Command panel; evidence: Visual reference.
- Placement: Left PropertyManager with ordered selection groups.
- Open/use: Choose the appropriate Loft command.
- Controls: Profiles, start/end constraints, guide curves and preview.
- Purpose/workflow: Organize section inputs and inspect the transition; the reference is a cropped panel comparison.
- [IMG-065: Loft versus Boundary PropertyManagers](https://www.goengineer.com/blog/solidworks-loft-vs-boundary-key-differences) — Release not visible; inspected.

### UI-038 — Boundary

- Surface: Command panel; evidence: Visual reference.
- Placement: Left PropertyManager using directional selection groups.
- Open/use: Choose the appropriate Boundary command.
- Controls: Direction 1, Direction 2 and preview-related options.
- Purpose/workflow: Assign directional inputs; do not substitute a Loft panel with a changed title.
- [IMG-065: Loft versus Boundary PropertyManagers](https://www.goengineer.com/blog/solidworks-loft-vs-boundary-key-differences) — Release not visible; inspected.

### UI-039 — Hole Wizard — Type

- Surface: Command panel; evidence: Visual reference.
- Placement: Left PropertyManager, Type tab.
- Open/use: Features > Hole Wizard.
- Controls: Hole type icons, standard/type/size, fit and end condition.
- Purpose/workflow: Specify the hole definition, then switch to Positions. Options depend on selected hole type.
- [IMG-076: Hole Wizard Type tab](https://www.goengineer.com/blog/power-and-value-solidworks-hole-wizard) — Release not visible; inspected.
- Documentation: [Hole Wizard Type PropertyManager](https://help.solidworks.com/2026/English/SolidWorks/Sldworks/r_HIDD_DVE_HW_HOLE_SPEC.htm) (2026).

### UI-040 — Hole Wizard — Positions

- Surface: Command panel; evidence: Visual reference.
- Placement: Positions tab at left with placement interaction over a selected face.
- Open/use: Switch Hole Wizard to Positions.
- Controls: Face selection prompt, position points and supported sketch tools.
- Purpose/workflow: Choose support geometry, place hole centers and constrain their positions.
- [IMG-077: Hole Wizard Positions tab](https://www.goengineer.com/blog/power-and-value-solidworks-hole-wizard) — Release not visible; low resolution; inspected.
- [IMG-078: Hole-position selection in graphics area](https://www.goengineer.com/blog/power-and-value-solidworks-hole-wizard) — Release not visible; low resolution; inspected.
- Documentation: [Hole Wizard Positions PropertyManager](https://help.solidworks.com/2024/english/SolidWorks/sldworks/HIDD_DVE_HW_INSERT_HOLE.htm) (2024).

## Assemblies

### UI-041 — Mate

- Surface: Command panel; evidence: Visual reference.
- Placement: Left selection list and mate-type controls; selected faces in viewport.
- Open/use: Assembly > Mate.
- Controls: Standard/advanced/mechanical groups, selection list, alignment and mates list.
- Purpose/workflow: Select compatible references, choose relation/alignment, then confirm. Connected screenshot is explicitly labeled.
- [IMG-079: Mate PropertyManager in SOLIDWORKS Connected](https://www.swyftsol.com/blog/how-to-create-an-assembly-in-solidworks) — Connected Premium visible; article describes 2023; inspected.

### UI-042 — Mate Controller launch

- Surface: Context action; evidence: Visual reference.
- Placement: Tree context menu beside selected mates.
- Open/use: Right-click appropriate mate selection.
- Controls: Mate Controller menu item.
- Purpose/workflow: Launch reference only. The controller panel itself has not been captured or executed.
- [IMG-009: Mate Controller context-menu entry](https://www.goengineer.com/blog/solidworks-mate-controller-explained) — Release not visible; inspected.

## Drawings

### UI-043 — Drawing Model View

- Surface: Command panel; evidence: Documentation only.
- Placement: PropertyManager linked to placement on the central sheet.
- Open/use: Insert or select a drawing model view.
- Controls: Model choice, preview, orientation/scale and applicable import options.
- Purpose/workflow: Choose the referenced model and place a view. Exact open panel screenshot is not captured.
- Documentation: [Model/Predefined/Empty/Drawing View PropertyManager](https://help.solidworks.com/2025/english/SolidWorks/sldworks/HIDD_DVE_NAMED_VIEW.htm) (2025).

### UI-044 — Drawing view Properties

- Surface: Dialog; evidence: Documentation only.
- Placement: Separate dialog reached from a drawing view or its PropertyManager.
- Open/use: Right-click view > Properties or choose More Properties.
- Controls: View/model/configuration information and applicable properties.
- Purpose/workflow: Inspect the referenced model/configuration and update the view as required.
- Documentation: [Drawing View Properties](https://help.solidworks.com/2021/english/SolidWorks/sldworks/HIDD_DRVIEW_PROP.htm) (2021).

### UI-045 — BOM table on sheet

- Surface: Table; evidence: Visual reference.
- Placement: Placed within the drawing sheet; BOM node in left tree.
- Open/use: Insert or select a bill of materials.
- Controls: Rows/columns and model-linked property values.
- Purpose/workflow: Document assembly/component data. The reference proves table placement, not every BOM configuration option.
- [IMG-012: Drawing editor with View Palette and BOM](https://blogs.solidworks.com/solidworksdeutschland/2019/08/technik-tipp-solidworks-stueckliste-erweitern.html) — 2019 SP2.0 visible; German UI; inspected.

### UI-046 — Dimension PropertyManager

- Surface: Command panel; evidence: Visual reference.
- Placement: Left dock when relevant dimensions are selected.
- Open/use: Select one or more dimensions.
- Controls: Style, tolerance/precision, primary value, text and leader-related controls.
- Purpose/workflow: Edit annotation properties; only shared properties apply when selecting multiple dimensions.
- [IMG-080: Measure dialog while editing a dimension](https://hawkridgesys.com/blog/whats-new-solidworks-2019-measure-tool-enhancements) — 2019 article context; inspected.
- Documentation: [Dimension PropertyManager](https://help.solidworks.com/2023/english/SolidWorks/sldworks/HIDD_DVE_POWERASSIST_DIMCMD.htm) (2023).

## Manufacturing

### UI-047 — Weldments — Structural Member

- Surface: Command panel; evidence: Visual reference.
- Placement: Left parameters; path/profile preview in graphics area.
- Open/use: Weldments > Structural Member.
- Controls: Standard/type/size, groups, alignment and Locate Profile.
- Purpose/workflow: Select path segments and position a chosen profile before accepting.
- [IMG-066: Weldments — Structural Member](https://www.goengineer.com/blog/solidworks-weldment-profiles-weldment-tools-guide) — Release not visible; inspected.

### UI-048 — Sheet Metal — Edge Flange

- Surface: Command panel; evidence: Documentation only.
- Placement: PropertyManager paired with edge selection and a geometric preview.
- Open/use: Sheet Metal > Edge Flange in a sheet-metal part.
- Controls: Edges, radius, angle, length and bend-position options.
- Purpose/workflow: Select edges and define flange geometry. Actual panel screenshot remains a visual gap.
- Documentation: [Edge-Flange PropertyManager](https://help.solidworks.com/2025/english/SolidWorks/sldworks/HIDD_FEAT_SM_EDGE_FLANGE.htm) (2025).

## Evaluation

### UI-049 — Interference Detection

- Surface: Command panel; evidence: Visual reference.
- Placement: Left selected-components/results groups; overlap highlighted in graphics area.
- Open/use: Evaluate > Interference Detection.
- Controls: Components, Calculate, results and exclusion options.
- Purpose/workflow: Calculate, select an interference and inspect its highlighted geometry. Release/load-mode options vary.
- [IMG-063: Interference Detection](https://blogs.solidworks.com/solidworkslatamyesp/solidworks-blog/solidworks/solidworks-2019-vista-previa-los-avances-tecnologicos-parte-1/attachment/interference20checking20in20mb20parts-768x432-png/) — 2019 shown in status; low-resolution reference; inspected.
- Documentation: [Interference Detection PropertyManager](https://help.solidworks.com/2025/English/SolidWorks/sldworks/HIDD_DVE_INTERFERENCE.htm) (2025).

### UI-050 — Measure

- Surface: Floating dialog; evidence: Visual reference.
- Placement: Movable window over viewport with selected entities and in-graphics readouts.
- Open/use: Evaluate > Measure and select suitable entities.
- Controls: Selected entity list and distance/area or other applicable results.
- Purpose/workflow: Inspect measurements while geometry remains visible. Captured example also has a Modify popup open.
- [IMG-080: Measure dialog while editing a dimension](https://hawkridgesys.com/blog/whats-new-solidworks-2019-measure-tool-enhancements) — 2019 article context; inspected.

## Simulation

### UI-051 — Simulation study tree

- Surface: Dock; evidence: Visual reference.
- Placement: Study-specific left tree associated with an analysis tab.
- Open/use: Activate a Simulation study.
- Controls: Parts, Connections, Fixtures, External Loads, Mesh and results-related folders.
- Purpose/workflow: Organize model assumptions and analysis setup; this is an add-in workspace.
- [IMG-033: Simulation study tree / mixed mesh indicators](https://blogs.solidworks.com/products/solidworks/whats-new-in-solidworks-simulation-2020/) — Simulation 2020 article context; inspected.
- [IMG-038: Simulation setup-tree structure](https://www.goengineer.com/blog/6-steps-first-solidworks-simulation) — Release not visible; inspected.

### UI-052 — Mesh context actions

- Surface: Context menu; evidence: Visual reference.
- Placement: Popup beside the Mesh node in a Simulation study.
- Open/use: Right-click Mesh.
- Controls: Create Mesh, Mesh and Run, Apply Mesh Control and diagnostics.
- Purpose/workflow: Choose a meshing action; this image does not show the numeric mesh-settings panel.
- [IMG-035: Simulation Mesh context menu](https://blogs.solidworks.com/teacher/2025/05/lesson-3-solidworks-optimization-analysis-using-a-topology-study.html) — 2025 lesson context; inspected.

## Settings and notifications

### UI-053 — System Options — General

- Surface: Dialog; evidence: Visual reference.
- Placement: Category tree left, search upper right and settings in center.
- Open/use: Tools > Options > System Options.
- Controls: Behavior checkboxes, categories and confirmation buttons.
- Purpose/workflow: Configure application-wide preferences separately from document-specific defaults.
- [IMG-003: System Options — General](https://faq.mecad.co.za/knowledge/quick-and-easy-steps-to-maximize-solidworks-performance-with-your-current-nvidia-gpu) — Release not visible; inspected.

### UI-054 — Document Properties tab

- Surface: Settings tab; evidence: Visual reference.
- Placement: Alongside System Options in the captured Options dialog.
- Open/use: Tools > Options > Document Properties with appropriate context.
- Controls: Document-scoped properties; full content not captured here.
- Purpose/workflow: Keep document-specific settings distinct from application-wide settings. Only tab placement is visually confirmed.
- [IMG-072: Dismissed Messages options](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — 2026 article context; inspected.

### UI-055 — Dismissed Messages

- Surface: Settings page; evidence: Visual reference.
- Placement: Nested category under Messages/Errors/Warnings; message table in main area.
- Open/use: System Options > Messages/Errors/Warnings > Dismissed Messages.
- Controls: Dismissed-message rows and status-icon toggle.
- Purpose/workflow: Review auto-dismissed messages and restore relevant prompts; source screenshot is 2026 context.
- [IMG-072: Dismissed Messages options](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — 2026 article context; inspected.

### UI-056 — Customize Manager Pane Tabs

- Surface: Popup; evidence: Visual reference.
- Placement: Opened against the left manager-tab strip.
- Open/use: Customize manager tabs using the version-specific UI.
- Controls: Checkbox list for manager tabs; required tabs disabled in screenshot.
- Purpose/workflow: Control optional manager-tab visibility; 2026 reference, not proof of an identical older UI.
- [IMG-069: Customize Manager Pane Tabs](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — 2026 article context; inspected.

### UI-057 — Display scaling optimization

- Surface: Dialog; evidence: Visual reference.
- Placement: Prominent message dialog with three action choices.
- Open/use: Version-specific launch on a monitor with a different resolution.
- Controls: Move to main monitor; change settings; keep layout; dismissal checkbox.
- Purpose/workflow: Resolve a display-layout choice. Exact triggering logic has not been tested.
- [IMG-075: Display scaling optimization](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — 2026 article context; inspected.

### UI-058 — Import Complete notification

- Surface: Notification; evidence: Visual reference.
- Placement: Compact message crop; original screen position is not visible.
- Open/use: After import completes in the source example.
- Controls: Success indicator, file/path text, Open Model and Open Location.
- Purpose/workflow: Distinguish completion notice from the full import-progress/task-pane interface.
- [IMG-071: Import Complete message](https://www.goengineer.com/blog/solidworks-2026-ui-ux-performance-and-imports-whats-new) — 2026 article context; inspected.

## PDM — related application

### UI-059 — Vault File Explorer

- Surface: External workspace; evidence: Visual reference.
- Placement: Windows navigation left, vault files upper right, file-detail area below.
- Open/use: Open a configured PDM vault view.
- Controls: Vault commands, columns, folder navigation and detail tabs.
- Purpose/workflow: Browse managed files and inspect selected-file state. Windows Explorer UI is not a CAD document page.
- [IMG-045: PDM vault Explorer with Data Card](https://goengineer.com/blog/working-offline-solidworks-pdm-tips-and-tricks) — Exact PDM release not visible; inspected.
- Documentation: [PDM File Explorer Interface](https://help.solidworks.com/2024/english/EnterprisePDM/FileExplorer/c_Explorer_View.htm) (2024).

### UI-060 — Data Card

- Surface: Detail tab; evidence: Visual reference.
- Placement: Lower file-detail area in the captured Windows vault view.
- Open/use: Select a file and Data Card.
- Controls: Configurable metadata fields, configuration tabs and document status.
- Purpose/workflow: Inspect/edit permitted metadata. Field names are deployment-specific examples.
- [IMG-045: PDM vault Explorer with Data Card](https://goengineer.com/blog/working-offline-solidworks-pdm-tips-and-tricks) — Exact PDM release not visible; inspected.
- Documentation: [PDM File Explorer Interface](https://help.solidworks.com/2024/english/EnterprisePDM/FileExplorer/c_Explorer_View.htm) (2024).

### UI-061 — File Preview

- Surface: Detail tab; evidence: Visual reference.
- Placement: Below vault file list; shown behind the History dialog.
- Open/use: Select a file and Preview.
- Controls: Model preview and applicable viewing controls.
- Purpose/workflow: Review the chosen file without treating this as an editable CAD feature workspace.
- [IMG-049: PDM History dialog over vault view](https://www.goengineer.com/blog/feature-feud-solidworks-pdm-rollback-vs-checking-in-a-previous-version) — Exact PDM release not visible; inspected.
- Documentation: [PDM File Explorer Interface](https://help.solidworks.com/2024/english/EnterprisePDM/FileExplorer/c_Explorer_View.htm) (2024).

### UI-062 — History

- Surface: Dialog; evidence: Visual reference.
- Placement: Separate window above vault file list/preview.
- Open/use: Select a file and History.
- Controls: Event table, version/user/date and details.
- Purpose/workflow: Inspect stored iterations and transitions. No rollback or state mutation was executed.
- [IMG-049: PDM History dialog over vault view](https://www.goengineer.com/blog/feature-feud-solidworks-pdm-rollback-vs-checking-in-a-previous-version) — Exact PDM release not visible; inspected.
