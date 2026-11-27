## ADDED Requirements

### Requirement: Standalone local tool
The tool SHALL run independently inside the TCS Tools directory without requiring Unreal Engine or altering combat-system modules.
#### Scenario: Start production build
- **WHEN** dependencies are installed and the documented build/start commands are run
- **THEN** a loopback HTTP service serves the workbench and reports its address.

### Requirement: Four-region real 3D workbench
The workbench SHALL contain layers, a WebGL2 viewport, timeline, and properties/AI panels. Orbit SHALL use a genuine 3D camera with selectable left/right mouse binding, wheel dolly, and conflict-free pan.
#### Scenario: Inspect a spatial effect
- **WHEN** a user selects Impact and orbits, changes binding, zooms and resets the camera
- **THEN** spatial relations and perspective change and the controls remain operable.
#### Scenario: Unsupported graphics
- **WHEN** WebGL2 creation fails
- **THEN** an actionable error is displayed rather than a simulated 2D viewport.

### Requirement: Three editable templates
The system SHALL create independent projects from versioned magic-circle, slash and Impact templates and allow layer addition, duplication, removal, visibility and Solo plus basic transform, spawn, motion and appearance edits for applicable layer kinds.
#### Scenario: Edit and undo
- **WHEN** a user changes layer parameters or duplicates/removes a layer
- **THEN** the viewport reflects the change and undo/redo restores the corresponding definitions.
#### Scenario: Empty project
- **WHEN** all layers are removed
- **THEN** the UI shows a valid empty state and allows adding a new layer.

### Requirement: Deterministic time evaluation
The foundation SHALL evaluate supported layers from project time and seed, support playback/pause, seek and single-frame steps, and keep camera inspection independent from evaluation state.
#### Scenario: Revisit a frame
- **WHEN** a user seeks away from and back to a frame without editing the definition
- **THEN** generated geometry and particle state match the previous evaluation.
#### Scenario: Frame bounds
- **WHEN** a 0.8-second, 60-fps project is opened
- **THEN** authored sample indices are 0 through 47 and frame stepping does not exceed the last sample.

### Requirement: Local project persistence
The system SHALL save, list and reopen validated projects with schema/template versions and stable IDs, expose unsaved/saving/saved/error states, and prevent stale save responses from overwriting newer editing state.
#### Scenario: Save and reopen
- **WHEN** a user renames and edits a project, saves it and reopens it
- **THEN** the same validated project data is restored.
#### Scenario: Invalid project or failed request
- **WHEN** input is invalid or storage is unavailable
- **THEN** a readable error is shown and the current edit is preserved.

### Requirement: Restricted local service
The server SHALL bind only to loopback, reject unauthorized mutation requests and invalid IDs, and validate documents without implicit type coercion, defaults or unknown-field removal.
#### Scenario: Invalid mutation
- **WHEN** a request has an untrusted origin, wrong token, invalid ID, unknown document fields or out-of-range parameters
- **THEN** it is rejected without changing stored projects.

### Requirement: Honest phase boundary
The interface and documentation SHALL identify AI generation, advanced curves/trajectory import and baked export as unavailable in this phase; they SHALL NOT return simulated success for these operations.
#### Scenario: AI panel
- **WHEN** the AI panel is opened
- **THEN** it states that no provider is configured and does not claim to generate assets.
