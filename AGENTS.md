# PepperSnap — notes for anyone working on this codebase

PepperSnap is a native Win32 screenshot, annotation and "pin to screen" app for
Windows, written in C++17 and built with Zig. It ships as **one self-contained
`.exe`** — no installer, no runtime DLLs, no framework. That single-file promise
is the product's main selling point, and most decisions below follow from it.

- Licence: Apache 2.0 (`finnuen`) — see `LICENSE`
- Platform: Windows 10/11, x64
- Version constant: `APP_VERSION` in `PepperSnapInternal.h`
- No third-party code anywhere: Win32 + GDI+ + WIC + a little COM

## Layout

| File | Lines | Owns |
|---|---:|---|
| `PepperSnap.h` | ~325 | Shared types: `HotkeyBinding`, `Color`, control IDs, window classes, message IDs, the `HK_*` hotkey ids |
| `PepperSnapInternal.h` | ~990 | `APP_VERSION`, the `PepperSnapDaemon` class declaration, app-wide constants |
| `PepperSnap.cpp` | ~390 | `WinMain`, single-instance mutex, admin re-launch, tray daemon window, shutdown, shared string/path helpers |
| `Settings.cpp` | ~3730 | `settings.ini` load/save, the Options window and its sub-windows (Customize keys, Naming pattern syntax), every default |
| `Registry.cpp` | ~240 | HKCU work only: launch-at-startup, and the Explorer image context-menu verbs |
| `InputHook.cpp` | ~440 | Low-level keyboard hook, global hotkey registration, shortcut-matching helpers |
| `Capture.cpp` | ~850 | Screen capture, clipboard, image encoding (PNG / JPEG / BMP), WebP file writing |
| `WebPEncode.cpp` | ~810 | The lossless WebP (VP8L) encoder — the only WebP writer there is |
| `Overlay.cpp` | ~9230 | The capture overlay: selection, magnifier, all annotation tools, OCR entry point, web image search |
| `Pinned.cpp` | ~4660 | Pinned floating windows and their toolbar, plus the in-app "Keyboard shortcuts and info" window |
| `Ocr.cpp` | ~360 | Text recognition via `Windows.Media.Ocr` over the WinRT COM ABI |
| `Tray.cpp` | ~970 | Tray icon, toasts, tray menu, update checker |

`Overlay.cpp` and `Pinned.cpp` are the two big ones — nearly every pixel-level
UI detail lives in them.

## Build

One script, `build.ps1`:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

It compiles `peppersnap.rc` with `zig rc`, then the ten translation units with
`zig c++ -target x86_64-windows-gnu`, and writes `build\PepperSnap_v<version>.exe`.

### Two rules that must not be broken

1. **Zig 0.13.0 exactly.** The flags are 0.13-era. `winget install zig.zig`
   currently gives 0.17.0, which will not work. `build.ps1` hard-fails on a
   version mismatch rather than producing something subtly wrong. Get 0.13.0
   from <https://ziglang.org/download/> and extract it; it is portable.
2. **Keep `-static`.** It is the entire reason the exe is ~1.2 MB with no
   dependencies. Drop it and the binary starts requiring `libstdc++-6.dll` and
   `libgcc_s_seh-1.dll`, silently destroying the headline feature. The script
   scans the finished exe's import table and fails the build if any of
   `libstdc++-6.dll`, `libgcc_s_seh-1.dll` or `libwinpthread-1.dll` appear.

### Versioning

One visible version, `MAJOR.MINOR`, lives in three places, and `build.ps1` is
the only thing that writes it: `FILEVERSION` / `PRODUCTVERSION`, their two
strings and `OriginalFilename` in `peppersnap.rc`, and `APP_VERSION` in
`PepperSnapInternal.h`. The build is named after the same number
(`build\PepperSnap_v4.23.exe`), so the build, the git tag and the version the
app reports cannot drift apart.

`FILEVERSION` needs four numbers, so it carries the version as `4,23,0,0`: the
visible `4.23` is always the first two fields and the trailing pair is always
`0`. The fourth field used to hold `git rev-list --count HEAD + 1`.

**MINOR is the build counter and increments on every build.** That is the point:
here each build is a release. So:

- `build.ps1` alone reads the recorded version and adds one to MINOR.
- `-NoVersionBump` builds the recorded version unchanged — what a repeat build
  of the same source uses, so the number does not creep.
- `-Version 4.23` sets it outright. It is how the three-digit scheme was folded
  into two, and how a new major series would start. Only the leading digit waits
  for the user to ask for it by name.

The scheme was folded at `4.2.2`: the next build is `4.23`, and `4.24` after
that. **The old tags stay exactly as published** — `4.2.1` and `4.2.2` are not
renamed to `4.21` and `4.22`, because renaming them would break existing release
links and the update check inside every build already out there. Nothing else
has to change for those builds to notice the update: `CompareVersionStrings`
pads both sides to four fields and compares numerically, so `4.23` reads as
`4.23.0.0` and sorts above `4.2.2`'s `4.2.2.0`.

The script rewrites both files in place and hard-fails if any field did not
land. The strings are easy to miss — the resource script writes them with a
trailing `\0` — and a half-applied rewrite is exactly how a file version drifts
away from the tag it was released under.

## Verifying a change

Verification here is **build-only**: compile it, confirm the build succeeds and
the import table is clean. The GUI is smoke-tested by hand — there is no test
harness and no test suite.

One exception, and use it: `WebPEncode.cpp` stands alone, so it can be compiled
into a throwaway program next to a decode step. Encode a known buffer, decode the
bytes back through WIC's registered "Microsoft Webp Decoder", and compare every
pixel. Nothing else in the codebase can be checked that cheaply, and a bitstream
bug is invisible until the file will not open at all.

Two traps when running it locally:

- **Quit a running PepperSnap first.** It holds the global hotkeys and the
  `Global\PepperSnap_TrayDaemon_Mutex`; a second instance just forwards its
  command line to the first and exits, so you will be testing the old build.
- **Rebuilding while it runs fails**, because the exe is locked:
  `lld-link: error: failed to write output ... Permission denied`.

## Things that will bite you

### Defaults live in three places and must agree

Every rebindable shortcut appears in **(1)** the customize-keys row table in
`Settings.cpp` (search for `defaultBinding`), **(2)** the "Restore everything to
default" block in `Settings.cpp`, and **(3)** the field initialisers in
`PepperSnapInternal.h`. Change one, change all three. Global hotkeys have a
fourth place — `ApplyGlobalHotkeys` in `InputHook.cpp` — and any hotkey
registered there must also be unregistered there. `HK_EDIT_CLIPBOARD` was once
registered but not unregistered, which left the old hotkey live after the user
cleared that shortcut.

Two more places hold the same numbers, and they are easy to forget because they
are not tables: the fallback arguments of the `readHk(...)` calls in
`LoadSettings` (what a key that is *absent* from `settings.ini` falls back to),
and the `HotkeyBinding` initialisers in `struct OptionsDlgState`. That is five
places in total for one default.

Option *toggles* add a sixth place, and it is the one that bites: that same
restore block sets each `st.<field>` value *and then* the matching checkbox, and
the two can disagree. `hKeepEditOpenChk` was set `BST_CHECKED` while
`keepEditOpen`, its `LoadSettings` fallback and the shipped default all said
false, so "Restore everything to default" followed by "Save options" turned the
option *on*. `IDOK` reads the checkbox, not the value — when the two disagree,
the checkbox is the bug.

Most of them ship unbound on purpose. The custom-area tools, OCR, pin-on-top,
the pinned zoom/outline/toolbar/smooth keys and both "Search on web" entries are
blank so that a fresh install cannot swallow a bare keystroke the user meant for
the app underneath; the maintainer's own `settings.ini` is the reference for
what "the default set" is.

### A text box gets its face, size and outline from one place

Calibri Light is the annotation face, the S / M / L / XL buttons set a text box's
*stroke width* and the type size is derived from it, and the glyph outline is
chosen from the fill colour rather than exposed as a setting — the earlier "Text
style" window that did expose it was pulled back out as inconvenient. The rules
live in five helpers at the top of `Overlay.cpp` (`AnnotationFontFace`,
`kAnnotationFontStyle`, `AnnotationFontSize`, `AnnotationOutlineColor`,
`AnnotationOutlineWidth`), and they are there because the size formula used to be
hand-copied into five separate call sites; a text box is drawn, measured, hit-
tested and exported by four different functions, and any of them drifting away
from the others shows up as a caret that sits off the glyphs.

Three things about it are not obvious:

- **The family is probed, never assumed.** Asking GDI+ for a font family that is
  not installed does not throw or fall back — it hands back a `Font` that draws
  nothing at all, so the text would silently vanish from the screenshot. The
  helper keeps Segoe UI as a fallback for the day Calibri Light is missing.
- **The outline is a stroked glyph path, not a second draw.** Each line is added
  to a `GraphicsPath`, stroked with a pen of *twice* the wanted border so that the
  fill covers the inner half, and filled over — that is what leaves a border of
  exactly the wanted width outside the letterforms. The path must use non-zero
  winding, or the counters in `o`, `e`, `a` and `4` close up.
- **Calibri Light has no bold face** (`calibril.ttf` is the only upright weight),
  so `FontStyleBold` makes Windows synthesise one. That is the weight the earlier
  Text style window shipped, so it is kept; `FontStyleRegular` is the switch for
  the true thin face, and it changes only `kAnnotationFontStyle`.

Outline width is at least 2 px and at most 5% of the type size; the floor wins
below 40 px type, where the two cannot both hold, because a thinner border reads
as anti-aliasing rather than as an outline.

### The annotation layer draws unclipped; the crop decides what is kept

`RenderOverlayWindow` used to put a `SetClip` on the selection rect before drawing
the highlighter and every vector annotation, and it is gone on purpose. With it,
anything reaching past the selection edge was cut exactly at the border — worst
of all a text box, which is typed blind: its letters simply stopped mid-glyph with
nothing to say they existed, and the box could be much wider than it looked.

So the editor now draws every annotation where it was put, over the dimmed
desktop outside the selection. What the capture keeps is decided downstream by
`RenderCroppedRegionBitmap`, which crops to the selection rectangle and masks an
ellipse or a quad via `ApplyEllipseMaskToBitmap` / `ApplyQuadPolygonMaskToBitmap`
after the annotations are drawn. That means removing the clip changed no saved
pixel at all: it is a display change only, and the same in every mode — region,
full-screen, window select, and an image opened for editing.

Two consequences worth knowing before "fixing" it back:

- **The selection frame, handles, HUD and toolbar are drawn after** the annotation
  pass, so they still sit on top of a stroke that runs under them.
- **Mosaic is the one tool that does not overflow.** Its pixelation is applied to
  the captured pixels inside the selection only (`applyMosaicClipped`), because
  there are no captured pixels outside it to pixelate; its dashed guide rectangle
  does overflow, so a mosaic box straddling the edge shows its outline beyond the
  border while the effect stops there.

Because the layer is unclipped, a click can land on an annotation *outside* the
selection, and `WM_LBUTTONDOWN` handles that before it falls through to the
"outside the box" branch — otherwise that click would start a whole new area.
Two modifiers need care:

- **Ctrl is two things** — the temporary select tool, and window-pick while no
  area exists. Only the second wins over an annotation click, and it can only
  happen with no selection at all, which is also the only time there are no
  annotations to hit. Testing "Ctrl is down" alone made Ctrl + click on an
  overflowing annotation start a new area while the toolbar already showed the
  select tool.
- **Alt stays excluded** — it is the ellipse-drag modifier, and an Alt drag that
  begins on an annotation has to stay a drag.

A new area does not start on top of an annotation, then: the drag has to begin a
pixel away. That is the trade for being able to grab a box that overflows.

### A new area keeps the annotations

Creating a new area — by dragging, by Ctrl + click window pick, or by the arrow
keys — replaces the *crop*, not the document. `annotations`, `undoStack`,
`redoStack` and `nextStepNum` are deliberately left alone in those four paths, so
the work drawn so far survives and the step numbers keep counting. The Clear
button (`DBTN_ACT_CLEAR`) and its shortcut are the one explicit way to throw the
annotations away, and that path does `PushUndo()` first, so even clearing is
undoable.

Anything left outside the new area is simply cropped out of the save — the
unclipped editor already shows it, which is what makes the arrangement legible.
Mosaic is safe to leave outside: `ApplyPixelateFilterRawBuffer` clamps its rect to
the bitmap and no-ops when the intersection is empty.

### The selection frame is a dashed guide drawn outside the capture

The selected area is outlined with the same 1 px dashed pen the aiming crosshair
uses — `Color(150, 239, 68, 68)`, `DashStyleDash`, which GDI+ measures as 3 px of
dash and a 1 px gap. Both pens come from `ConfigureGuidePen`, so the aim line and
the frame cannot drift apart; they are the same guide at two stages of one
gesture.

The outline is drawn **one pixel outside** the capture, not on its edge. A capture
is the pixels *inside* the area, so an outline drawn on the border hides part of
what is being saved: `DrawRectangle(x, y, w, h)` paints along `x .. x + w`, and
the old code passed the capture width, which put the left and top edges on the
first captured row and column. The frame now starts at `sx - 1` / `sy - 1` with
the width one larger, so every edge lands beyond the last captured pixel. A
deformed quad pushes each corner 1 px away from its centre. The crop arithmetic,
the W×H pill and the masks are untouched by this — only where the line is painted.

A circle area draws **both** its bounding box and its curve, from that same pen.
The curve is the shape that is kept and masked, but the grab zones are the box —
the curve is away from the corners and the middle of each side — so a curve with
no box would leave a resize band with nothing under the cursor.

Two consequences worth knowing:

- The outline now sits on the dimmed backdrop just outside the brightened area,
  not over the bright pixels, which is the point.
- At the very edge of the screen there is no pixel outside the capture, so
  `frameL` / `frameT` clamp to 0 and that one edge still covers the first
  captured row or column. Nothing else can be drawn there.

### The area draws no resize points, and a whole side resizes

No resize squares are drawn on the area, whatever its shape. The frame itself is
the affordance: `HitTestSelectionHandles` returns the resize mode for a press
within 7 px of an edge, anywhere along it — not merely around the invisible
midpoint — and `UpdateOverlayCursor` uses the same function to show a resize
arrow. Corners are tested first, so a press in the overlap at a corner still
pulls that corner. The test is distance to the real segment, so a deformed quad's
slanted sides work the same as an axis-aligned rectangle's. That band only exists
while the effective tool is `SelectMove`, so a drawing tool can still draw up to
the frame; Ctrl, being the temporary select tool, brings the band back with it.

A circle used to be the exception: its eight points were drawn because they sit
on the bounding box rather than on the curve. Drawing the box removed the reason,
and adding a point marker on top of a dashed frame only doubled it. Do not
"restore" squares to the area without asking.

One thing went with the rectangle points: the pale-yellow fill that used to mark a
corner snapped onto another corner during a Ctrl + corner pull. The snap still
happens; only that highlight is gone.

### Ctrl + corner deforms the area, and Shift snaps the pull to 15°

Ctrl + left-drag on a corner of the area pulls that corner free, turning the
rectangle into an irregular quad — or into a triangle, when the corner lands on
another corner. `isDraggingSingleCorner` marks the gesture, and it is also why
`hasRatioConstrain` is false there: Ctrl + corner is one corner, not a box resize,
so the aspect-ratio constraints have nothing to hold.

Holding Shift as well snaps the edge from the **next corner round** to the pulled
one to 15° steps, the same step the line tool uses. The pivot is therefore
`dragOrigQuad[(cornerIdx + 1) & 3]`: pull the top-right corner and the right edge
turns around the bottom-right corner, pull the bottom-right one and the bottom edge
turns around the bottom-left one. The angle is measured from that pivot and the
distance follows the cursor, so the corner rides a ray out of a neighbour it cannot
cross — which is the line that is actually being drawn. An earlier version pivoted
on where the corner itself started, and that is what made the snap feel unrelated to
the edge it was supposed to straighten. The 4 px corner-on-corner snap wins over
the angle, because landing exactly on a corner is what makes a triangle.

Shift is read live through `IsShiftModifierHeld`, the way the line tool reads it, so
it can be pressed and released mid-drag.

### A snap says what it latched onto

A snap that is only a few pixels of movement is invisible: the corner jumps and
nothing says which point or which line it took. `snapHintKind` therefore records
what the current drag snapped to — `None`, `Ray`, `Point`, `Outline` or `Align` —
and `DrawSnapHint` draws it over the annotations and the frame, from state that is
set during the move that snaps, cleared at the start of every drag branch and again
on `WM_LBUTTONUP`.

- **Ray** — the 15° guide: a thin dashed ray from `snapHintAnchor` (the pivot)
  through `snapHintTarget` and 28 px past it, a white dot at the pivot and a white
  ring on the corner. The dot is the point the edge turns around and the ring is
  where the corner sits now, so the two ends of one guide cannot be confused.
- **Point** — a white square with a cyan border on the point it landed on. A square
  because this is an exact landing, not a direction, and it is the shape a resize
  handle used to have.
- **Outline** — the matched side, stroke or circle curve drawn as a soft 3 px cyan
  line under the landing, so "on this line" is visible rather than inferred, plus
  the same square at the exact landing.
- **Align** — the one or two dashed lines a move or an edge resize lined up,
  spanning both shapes so the two can be seen to be flush. Both axes at once is
  what a corner-to-corner landing looks like.

The segments ride in `snapHintSegs[2]` (x1, y1, x2, y2 each) with
`snapHintSegCount`, which is why the guide has no per-kind drawing state to keep in
step.

Cyan is the annotation-selection colour, which is the one colour already meaning
"this is about the thing you are holding".

### A point snap pulls the pointer onto it

A snap moves the shape, not the pointer, so the two end up a few pixels apart and the
shape reads as having drifted off the cursor. `SetSnapHint` therefore warps the
pointer with `SetCursorPos` when the landing is a `Point` — the client coordinate plus
`vScreenX` / `vScreenY`, because the overlay covers the virtual screen, so client and
screen differ only by that origin.

Two things make it safe:

- **Only a *new* point is warped to.** `snapWarped` / `snapWarpX` / `snapWarpY` record
  the last one, and warping on every move would pin the pointer to the point so the
  drag could never be pulled off it. Once warped, the next move reports the same point
  at distance 0, so it stops there by itself.
- **Only `Point` is warped to.** An `Outline` landing tracks the cursor along a whole
  side or curve, so warping would drag the pointer along it; an `Align` landing is a
  coordinate, not a place the pointer belongs. Both just clear `snapWarped`.

`ClearSnapHint` resets the flag, so the next drag warps again on its first point.

### Annotations snap to each other

`FindSnapTarget` is the one search for "a place this can land", and it returns more
than points: a corner or an end (`Point`), a side, a stroke or a circle's curve
(`Outline`, with the geometry kept for the guide). That is what makes corner to
corner, corner to side, line to line and line to side one feature instead of four.

- **A corner pull** — area corners and edges, this shape's own other corners, and
  every other annotation. It passes its own id and corner index so the corner
  cannot snap to itself; the landing is what turns a quad into a triangle.
- **A plain corner resize** skips its own annotation entirely, because the opposite
  corner is where the drag began.
- **A line family end** — Line, Arrow and Numbering arrow — at mouse-down for the
  start and on every move for the end, and the same for dragging an existing end.
  That is what lets several strokes be joined into one irregular shape with no gap.
  The end refuses its own annotation (`skipOwnAnnotation`): its own stroke lies
  under the cursor at every position and would hold the line at zero length. A
  point within reach wins over Shift's 15° angle, because landing on a point is
  exact and is the reason to hold the two together.
- **A shape being drawn** — Rectangle, Ellipse, Mosaic square and Mosaic circle —
  at mouse-down for the first corner and on every move for the one under the
  cursor. Without it a new box could only be made flush by resizing it afterwards,
  which is the bug that put this in: the *resize* path had snapping and the
  *create* path did not. A point within reach wins over Shift's 1:1 constraint for
  the same reason it wins over the 15° angle. A landing that would leave the shape
  under 2 px on either axis is refused, and so is one on the draft's own start
  (which the draft snapped to, so the corner would otherwise be dragged straight
  back onto it). Pen and Highlighter are deliberately left out: a freehand stroke
  has no corner, and every sample being pulled onto a neighbour would deform the
  line being drawn.
- **A corner being dragged out also lines up by axis.** When nothing exact is
  within reach, `AlignCornerToEdges` runs `FindEdgeLine` on each coordinate and
  draws the same dashed guide an edge resize and a whole move use, so a new box can
  be made flush with an old one *while it is drawn* rather than only afterwards. The
  corner-resize path calls it too, after its own point/outline attempt. Two
  exclusions: **Shift is exempt**, because its 1:1 constraint is the shape of the box
  and moving one axis alone would break it, and a result that would flatten the box
  to under 2 px on either axis is dropped back to the raw cursor. This is the *axis*
  snap, not the nearest-point one — `FindSnapTarget` lands a corner on a side it is
  within 4 px of, while this catches a corner that is merely level with one.
- **An edge resize** snaps its whole coordinate, through `FindEdgeLine`, onto
  another shape's side or the area's edge — "side to side". It is only applied when
  the side stays outside its opposite, so a snap can never turn a shape inside out.
- **A whole move** lines up both of the primary annotation's axes with other
  shapes' sides (`FindEdgeLine` on each axis, applied to the group's delta). Both
  axes caught at once is corner to corner.

Four rules keep the set honest:

- **A point beats an outline** unless the outline is at least `kSnapPointBias`
  closer (1.5 px). Every corner lies on two of its own sides, so an unbiased
  comparison would make a corner almost unreachable near an edge; the bias is
  deliberately a fraction of the tolerance, or a tight net would make the tie-break
  wider than the net itself.
- **Only the line family lands on a curve.** `skipCircleTargets` takes every curve
  out of the search for a *shape* drag — a circle annotation's curve and a circle
  area's alike. A shape's corner landing on a curve leaves the drawn shape visibly
  off the curve it "snapped" to, which reads as the shape fighting the cursor; a line
  end landing on one is exact and looks like the two meeting, so lines keep it.
  Shapes still take corners, ends, sides, strokes and the area's straight edges.
- **A circle does not snap while it is created or resized.** Only its *move* is
  snapped. A corner drag controls the bounding box corner, which is nowhere near the
  curve, so every landing was either a size the drag did not ask for or a circle
  sitting visibly off the thing it latched onto — trying to fix that by scaling the
  box until the curve met the target (`FitEllipseThrough`, now removed) only turned a
  few pixels of snap into a size jump. The create path, the mouse-down first corner,
  the corner resize and the edge resize all skip `FindSnapTarget` / `FindEdgeLine`
  for `Ellipse` and `MosaicCircle`; `DragMode::MovingAnnotation` still aligns one
  through `FindEdgeLine` like any other annotation.
- **A circle custom area follows the same rule.** `SelectionEllipse` returns the
  area's centre and radii when it is an ellipse selection, and both searches take
  the curve and nothing else from it: `FindSnapTarget` offers `considerCurve` instead
  of the quad's corners and sides, and `FindEdgeLine` skips it entirely because a
  curve has no straight edge to line up against. Before that, the area's *bounding
  quad* was offered whatever its shape, so a circle area snapped as a square — its
  corners were reachable, its curve was not.
- **The custom area is always a target**, even with the setting off. Its corners are
  what a triangle is made from, and the crop has to be something annotations can be
  laid out against. A circle area is the one exception on *shape*: it contributes its
  curve, never its box.

`snapAnnotationsToEachOther` (Options → *Snap annotation to each other*, on by
default, `SnapAnnotationsToEachOther` in `settings.ini`) gates the *annotation*
targets only; the area's own corners keep working either way. Tolerance is
`kSnapTolerance` — 4 px — everywhere, including the area's own corner-on-corner
snap. It was 12 px, which took the corner away while it was still visibly clear of
the line and read as the shape fighting the cursor.

### A selected annotation is framed the way the area is

There are three selection visuals, and which one a tool gets follows its shape:

- **Rectangle, circle, mosaic square, mosaic circle** — `DrawShapeAnnotationFrame`,
  the area's language: one dashed outline that follows what was drawn and no
  resize points. It is pushed out past the stroke itself (half the stroke, plus
  1.5 px) so the dashes never sit on the border they mark, for the same reason the
  area's frame sits outside its capture; a mosaic's stroke is its block size rather
  than a drawn border, so it only takes a token gap. A circle gets its bounding box
  as well as its curve, paired exactly as the area's is.
- **Line, arrow, numbered arrow** — a pair of dashed rails that follow the stroke,
  one either side of it, plus the end points. A box around a diagonal line encloses
  mostly empty space and says nothing about what was picked. The rails stop where
  the arrowhead begins so they never cut across it, and a numbered arrow gets a
  dashed ring around its badge instead of a start dot that would sit on the number.
- **Pen, highlighter** — the box stays. A freehand stroke has no frame of its own
  to follow, so the box is the only honest outline for it.

Selecting a mosaic replaces its always-on white-and-red guide rather than adding to
it, otherwise the two outlines sit a pixel apart and the shape reads as doubled.

Because nothing marks the shape's frame, all of it has to be grabbable without a
marker to aim at. `HitTestAnnotation` returns `DraggingAnnotationCorner` within
7 px of a corner, `DraggingAnnotationEdge` within 7 px of a side (or `0..3`, the
same order the area uses, so the cursor can pick the right arrow), and
`MovingAnnotation` anywhere inside the frame — that last one matters, because
otherwise the whole outline being a resize band would leave a rectangle or a
circle with no body left to drag. An edge drag moves that side and leaves the
opposite one put; a deformed quad is scaled inside the new box so it follows the
drag instead of snapping back to a rectangle. Only Rectangle and MosaicSquare can
be pulled free: an ellipse has no corners to deform, so Alt on its corner stays an
ordinary resize.

### Only the select tool starts a new custom area

`WM_LBUTTONDOWN` used to send any press outside the selected area into the
"start a new area" branch, whatever tool was active. That made it impossible to
draw outside the area without destroying it. The branch is now entered when the
point is inside the area **or** the effective tool is not `SelectMove`:

```cpp
OverlayTool toolAtDown = GetEffectiveTool();
if (IsPointInSelection(pt) || toolAtDown != OverlayTool::SelectMove) { ... }
```

So a drawing tool draws where it lands, inside or outside, and only the select
tool turns an outside drag into a new area. Ctrl still means the temporary select
tool, so Ctrl + drag outside still makes a new area — that is the documented way
to start one without leaving the current tool.

Rectangle, ellipse and both mosaic corners **do** snap while being drawn, at both
ends of the drag: the first corner at mouse-down and the moving one on every move,
through the same `FindSnapTarget` everything else uses. It used to be left out — with
a drawing tool free to draw anywhere, a latch onto the window's corners was thought
to be in the way — and the result was worse: a new box could be made flush with an
old one only by resizing it afterwards, which is a bug the eye reads as snapping
being broken. The net is tight enough (`kSnapTolerance`, 4 px) that drawing clear of
a neighbour is not a fight, and a landing that would leave the shape under 2 px on
either axis is refused. The free-form corner pull still considers the window's
corners, alongside the shape's own and the other annotations'.

### A deformed rectangle lives in `points`, and mosaic pixelates by polygon

The custom area has always allowed Ctrl + corner to pull one corner free, turning
it into an uneven quad or a triangle. Rectangle and Mosaic-square annotations do
the same, but on **Alt**, not Ctrl — Ctrl stays the temporary select tool, so a
Ctrl + corner drag has to keep resizing. The shape is stored in the annotation's
`points` — four entries in TL, TR, BR, BL order, the same order the custom area's
`selQuad` uses.
`startPt` / `endPt` stay the bounding box (`SetAnnotationQuad` refreshes them), so
everything that only understands two points — marquee bounds, the export crop
arithmetic, the move code that shifts both ends — keeps working untouched.

`AnnotationIsQuad` is the single test for "this shape is deformed". It is
deliberately narrow: only `Rectangle` and `MosaicSquare` are ever treated this way,
because `points` already means a stroke path for Pen and Highlighter.

Two consequences worth knowing:

- **Mosaic pixelates through a polygon mask.** `ApplyPixelateFilterRawBuffer` grew
  an optional `poly`; with it, both the averaging and the writing are limited to
  pixels inside the shape. Both callers — the live overlay pass and
  `RenderRegionBitmap` — pass the corners relative to the annotation's own
  top-left, which is the space the filter works in after its `(rx, ry)` offset.
  A mosaic quad therefore has to be a real polygon fill test, not a bounding box.
- **Alt starts the deformation; after that, corners stay corners.** Ctrl is left
  alone: it is the temporary select tool, so Ctrl + corner stays an ordinary
  resize. A plain rectangle resizes axis-aligned when a corner is dragged
  (opposite corner fixed). Once the shape is a quad, dragging any corner keeps it
  a quad — no modifier is needed, and nothing flattens it back into a rectangle.
  The corner snaps to the shape's own corners, to every other annotation's corners,
  ends, sides, strokes and curves, and to the custom area's corners and edges, all
  within 4 px — `FindSnapTarget`, the same search everything else uses — which is
  also how a quad collapses into a triangle.
- **Alt + Shift snaps the edge to 15°.** The same snap the area's Ctrl + Shift
  corner pull uses: the pivot is the corner next round from the pulled one
  (`q[(idx + 1) & 3]`), so pulling the top-right corner turns the right edge around
  the bottom-right one. Shift on its own is already the 1:1-square constraint of the
  axis-aligned resize, and only Alt makes this a pull at all. A corner that has
  landed on another point keeps that exact place, because that landing is what makes
  a triangle.

`HitTestAnnotation` returns the corner index through an out parameter and the
`DraggingAnnotationCorner` mode; `draggingAnnCorner` and `draggingAnnCornerFree`
carry it into `WM_MOUSEMOVE`. Rectangle, circle and both mosaics now share one
frame hit test (corner, then side, then the body to move), and the line family
keeps the two-endpoint handles; a freehand stroke is moved rather than reshaped.
Corner and side are not drawn — the frame is the cue — so the cursor is what says
they are there: `UpdateOverlayCursor` maps the corner to a diagonal arrow and the
side to a straight one, the same arrows the custom area uses.

`DraggingAnnotationCorner` and the newer `DraggingAnnotationEdge` are separate
modes, and an edge index rides in the same out parameter as a corner index
(`0..3`, top/right/bottom/left, matching the area's own order). `draggingAnnEdge`
carries it; both are reset from `annDragMode` on mouse-down so a stale index can
never be read from the other mode.

### A resize keeps its fixed corner from the box the drag started with

Both resizes hold one side still and move the other: a corner drag keeps the
*opposite corner*, an edge drag keeps the *opposite side*. Reading that fixed part
back from the shape on every move is wrong, and it is what made a drag unable to
cross the opposite line: as soon as the dragged corner or side passes it, the
shape's own box flips (`AnnotationQuadPoints` normalises to min/max), so the
"opposite" corner or side is now the one under the cursor — the fixed point follows
the drag, and the resize collapses into a move that stops dead at the far edge.

`CaptureAnnotationResizeOrig` therefore records the shape's four corners at
mouse-down, and both branches read the fixed part from `annResizeOrig`. The edge
drag also normalises its new box instead of clamping it (`std::swap` when the sides
cross), which is what lets a side pass through its opposite and keep growing.

Two details that follow from it:

- **The quad rescale maps from the *current* box, not the captured one.** A deformed
  quad is rescaled by the ratio between the box it is in now and the new one, so the
  mapping composes over the drag; using the captured box as the source would apply
  the whole scale again on every move.
- **A crossed edge is not snapped.** `FindEdgeLine`'s "roomy" test compares against
  the side that stays put, so on the far side of the crossing it would yank the
  side back across; `crossed` skips the search instead.

The pulled corner snaps through `FindSnapTarget`, so it can land on its own other
corners, on **every other annotation's** corners, ends, sides, strokes and curves,
and on the area's corners and edges, all within the same 4 px — two rectangles can
be lined up exactly, and a line can be met end-on. A circle contributes only its
curve, never its bounding box.

**A caution learned the hard way:** `MovingAnnotation`, `DraggingAnnotationStart`
and `DraggingAnnotationEnd` share one `case` block. Inserting a new `case` label
between them silently steals the move gesture — the new block runs, finds no
corner index, and breaks, so nothing moves at all. New drag modes go *before*
`case DragMode::MovingAnnotation:`, never inside that group.

Arrow keys nudge the selected annotations by 1 px, or 10 px with Shift
(`NudgeSelectedAnnotations`). A burst of key auto-repeat is coalesced into one
undo step by a 600 ms window, so Ctrl + Z returns the annotation to where the
burst started rather than walking back one pixel at a time. Text-box editing
consumes the arrow keys before this point, so caret movement is unaffected.

### An undo step carries the selection, not only the annotations

`undoStack` / `redoStack` hold `OverlayUndoSnapshot`, not `std::vector<Annotation>`.
The area is undoable now, so a step has to restore it alongside the vectors —
otherwise stepping back would show the previous annotations against the current
crop.

Three rules keep it honest:

- **`PushUndo()` is still the only way an annotation change records a step**, and
  it captures the area too.
- **A selection gesture is one step, taken on mouse-down and committed on
  mouse-up** (`BeginSelectionUndoGesture` / `CommitSelectionUndoGesture`). A press
  that moved nothing compares equal and is dropped, so clicking an annotation or
  tapping a handle cannot fill the stack with no-ops.
- **The first area is deliberately not undoable.** With no previous selection the
  step is discarded, because stepping back to "no selection at all" would leave
  the overlay with no area to draw on. The step exists to put a *replaced* area
  back.

`CommitSelectionUndoGesture` runs at the end of the mouse-up block, after
`CanonicalizeSnappedQuad` / `SyncSelRectFromQuad`, so it records the area the drag
actually produced rather than an intermediate one.

### Capture filenames go through NextFreeCaptureStem, not FormatFilename

The default naming pattern is `PepperSnap_{N}`, and the counter is restored from
`settings.ini`, so a save pointed at a folder that already holds
`PepperSnap_1.png` would otherwise overwrite it. Every path that writes a
capture therefore calls `PepperSnapDaemon::NextFreeCaptureStem(ext)`, which
expands the pattern and walks the counter forward until the name is free, rather
than the old `FormatFilename(captureCounter++)`. If you add a save path, use the
helper. It also bails out as soon as the generated name stops changing, because
a purely timestamp-based pattern would otherwise loop forever.

### The overlay leaves the screen before a heavy button does its work

Copy, Pin, Quick save, Save as, Extract text, Translate on web and Search on web
cut their bitmap out of daemon state in `RenderCroppedRegionBitmap()` and then
close the overlay. That render alone is about 100 ms on a full-screen custom-area
selection (the crop clone plus the mosaic and highlighter passes), and the encode,
the clipboard handoff, the file write and OCR add a few hundred more. With the
window still up, all of that read as the toolbar freezing for a moment and *then*
doing the job — the most-complained-about thing in the app for five builds, and
two wrong theories (the 40 ms hover poll, then a starved message queue) were
chased before the log showed the work was simply sitting in front of
`DestroyWindow`.

The shape is therefore: `HideOverlayForPendingWork()`, then the render, then
close, then the work. The render reads daemon state and not the window, which is
exactly what makes hiding first safe. Do not "simplify" it back to
render-then-close, and give any new heavy button the same shape. The one exception
is a snip started on top of an edit session: closing there hands the window back
to the editor instead of taking it away, so the helper refuses to hide
(`savedEditSession.active`) and the window has to stay up.

The settings write is the other half, and it is bigger than it looks. The log for
one close reads `up-to-hide 2 ms`, then `close/record-selection 210 ms` and
`close/save-settings 95 ms`: `CloseRegionSnipOverlay()` runs `SaveSettings()` twice
— once directly and once through `RecordLastCustomSelection()` — and each call
makes about 130 `WritePrivateProfileStringW` calls, each of which rewrites the
whole file. That pair measured 300–420 ms. It is deliberately *after*
`DestroyWindow()`, where none of it can be seen; before that ordering existed it
was exactly the 300 ms the toolbar spent frozen.

`SaveSettings()` also used to rewrite every Explorer image context-menu verb on
every call — ten shell keys plus nine file extensions' worth of
`HKCU\Software\Classes`. Those verbs depend on three inputs only (the setting, the
web-search setting, the exe's own path), so `SyncImageContextMenuRegistration()`
now runs that work only when one of them moves; measured, it was 0.0 ms per save
afterwards. Anything expensive that either function grows must stay behind the
close, and anything static must be guarded the way the verbs are.

### The in-app guide is hand-written documentation and drifts

"Keyboard shortcuts and info" (`ShowShortcutsModal` in `Pinned.cpp`) is a
hand-written reference for every feature. It does not read from the code, so it
rots quietly — it once advertised an `images.google.com` flow and a `{count}`
naming token, neither of which existed any more. It builds shortcut labels
through `FormatLabelWithShortcut(..., hkXxx)` against the *live* binding, so key
names cannot drift; the prose can.

### A modal window runs its own message loop, so WM_QUIT has to be handed back

Options, Customize keys, the naming-syntax window, the in-app shortcuts window and
the clipboard-history prompt each pump their own `GetMessage` loop while they are
open. So when the tray's "Exit" is chosen while one of them is up, it is that loop
which receives the message, and `PostQuitMessage`'s WM_QUIT is consumed there. The
outer loop in `PepperSnap.cpp` then blocks for ever: the tray icon is already gone,
the window is still on screen, and the only way out is Task Manager.

Every nested loop therefore calls `PepperSnapDaemon::GetMessageKeepQuit` rather
than `GetMessageW`; it puts the quit back on the queue so each level unwinds in
turn and the outermost one still ends. Do not "simplify" one of them back, and use
the same helper for any new modal window.

### Window select has to match the frozen screenshot

`SnapshotDesktopWindows` (`Overlay.cpp`) builds the rects that Ctrl + hover
highlights, from a class-name skip list. Our own Options and shortcuts windows
must **not** be in that list: they sit behind the overlay but their pixels are in
the frozen bitmap, so skipping them made Ctrl + hover highlight whatever was drawn
behind them, while their sub-windows — Customize keys and the naming-syntax list —
were never skipped and highlighted correctly. The rule is "visible in the bitmap is
selectable"; only the overlay itself, the pinned bubble tooltip and the hidden tray
daemon window belong in the list.

### Elevated by default is deliberate, not an accident

`RunElevated` defaults to on. That is what makes shortcuts and capture work
while an app *running as administrator* has focus; without it, a shortcut
pressed over such a window silently does nothing — a silent no-op, which is
worse than a prompt.

Launching is plain self-elevation: `WinMain` re-launches itself through
`ShellExecuteExW`/`runas`, so Windows shows a UAC prompt.

**Do not try to make that prompt disappear.** A Task Scheduler logon task with
`RunLevel=HighestAvailable`, driven on demand through `schtasks /Run`, does give
a prompt-free launch. It was built and run here, and Microsoft Defender then
flagged the executable:

    Trojan:Win32/Bearfoos.A!ml    (Alert level: Severe)

The detection was behavioural, not merely string-based: Defender attached it to
the exe *and* to `C:\Windows\System32\Tasks\PepperSnap` and its `TaskCache`
registry keys — that is, to the act of registering an elevated task from an
unsigned portable binary. That is the pattern the family is defined by, so
renaming or restructuring does not make it acceptable; it would only be evasion,
and it would still be caught. A Windows service, an IFEO / silent-exit trick, or
any other "start elevated without asking" mechanism is the same story and worse.
Live with the prompt. The banner comment on `IsRunAtStartupEnabled` in
`Registry.cpp` records this too.

The one legitimate reduction is kept: `WinMain` looks for an already-running
daemon *before* elevating and forwards to it, so a second launch — a file to
pin, `--options`, a context-menu verb — costs no prompt at all, because it
elevates nothing. That reaches across the integrity boundary only because the
tray window calls `ChangeWindowMessageFilterEx(..., MSGFLT_ALLOW)` for
`WM_COPYDATA` and every `WM_TRIGGER_*` message; remove those and forwarding
breaks silently.

"Launch at Windows startup" is a plain `HKCU\...\Run` entry
(`IsRunAtStartupEnabled` / `ToggleRunAtStartup`). Run entries start with the
filtered token, so with `RunElevated` on, logging on shows the prompt as well.
The README says so rather than engineering it away. `RegisterImageContextMenu`
refreshes the stored path so the entry survives the exe being moved, and
`UnregisterImageContextMenu` deliberately leaves it alone — it used to delete
it, which silently switched autostart off along with the separate "right-click
integration" setting.

If the elevation prompt is declined the code simply carries on unelevated: there
is no `else` on the `ShellExecuteExW` call. Do **not** "fix" that by adding an
error path or an exit.

### "No dependencies" is about DLLs, not about WinRT

The exe imports only Windows system DLLs. It reaches OCR through the WinRT COM
ABI by loading `RoInitialize` and `RoGetActivationFactory` dynamically in
`Ocr.cpp` — which is why nothing links against the WinRT import libraries.

### Image encoding drops metadata on purpose

JPEG, PNG and BMP writing in `Capture.cpp` deliberately strip metadata
(`WriteMetadataFreeJPEG`, `WriteMetadataFreePNG`, `SaveBitmapAsMetadataFreeBMP`)
so saved screenshots carry no EXIF or ICC baggage. WebP goes through our own
encoder, which has no metadata to strip in the first place — see below.

### WebP is written by our own encoder, and it has to be

Windows can *read* WebP but has never been able to *write* it, and that is not a
matter of installing the right thing. Ask WIC to enumerate its encoders and you
get BMP, GIF, JPEG, PNG, TIFF, WMPhoto, DDS and HEIF — no WebP. The Store's "WebP
Image Extensions" package (present on most machines, including the one this was
written on) registers `System32\mswebp.dll` as a **decoder only**: asking that
codec object for `IWICBitmapEncoder` fails. The WinRT `BitmapEncoder` API has no
WebP encoder id at all. So the app carries its own encoder, in `WebPEncode.cpp`.

It implements the subtract-green and predictor transforms plus the full entropy
layer — length-limited prefix codes and LZ77 backward references. It leaves out
the colour transform, colour cache, colour indexing and meta prefix codes, which
are all optional refinements. The result beats PNG on screenshots (a 1920x1080
screen: 473 KB against PNG's 787 KB, pixel-identical after a round trip through
the WebP decoder Windows does have) and costs roughly half a second for a screen
that size, most of it in the match search.

`WebPEncode.cpp` deliberately knows nothing about the rest of the app: it takes a
BGRA buffer and returns a file image. That is what makes it testable on its own,
which matters more here than anywhere else in the codebase, because a corrupt
bitstream is not something you can eyeball.

Three traps, all of which produced a file that Windows refused to decode:

- **A one-symbol prefix code costs zero bits.** RFC 9649 describes such a tree
  with a length of 1 and then says no bits are consumed when it is used, and the
  decoder means it. Emitting that bit shifts every following bit and corrupts the
  whole image.
- **The predictor's border rules are not the obvious ones.** The first pixel of
  the image predicts solid black (`0xFF000000`), the top row always predicts from
  the left, the left column always from above, and on the rightmost column the
  top-right pixel is taken from the *start of the same row*.
- **A prefix code has to be a complete tree.** Huffman's usual output can exceed
  the 15-bit limit the format allows, and clamping the deep leaves leaves a code
  that is not full, which the decoder rejects. The builder raises the rarest
  symbols' weights and rebuilds instead, so every tree it emits is a real one.


### Don't commit build output

`.gitignore` covers `build/`, `*.exe`, `*.lib`, `*.res`, `*.obj`, `*.pdb`.
`PepperSnap.lib` was once committed by mistake — don't resurrect it.

### Binary assets are permanent

`assets/demo.gif` is ~5.8 MB and lives in git history forever. Every additional
megabyte of media is a megabyte on every future clone.

## Conventions

- Text files are UTF-8 **without a BOM**, LF line endings; git normalises line
  endings on commit.
- Comments explain *why*, especially for non-obvious Win32 behaviour. Match the
  surrounding style, which uses `// ---` banner comments to separate sections.
- No third-party libraries, ever. If a feature needs a dependency, it needs a
  different design.

## Manual smoke list

1. `Ctrl + Print Screen` → region capture; the overlay opens.
2. Draw with each tool; undo/redo; copy; save in each format.
3. Pin an image (`Alt + Print Screen`); move, resize and scale it.
4. OCR: copy text out of a screenshot.
5. Options → rebind a shortcut; confirm the new key works *and* the old one is gone.
6. Tray → "Launch at Windows startup" on; confirm
   `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` has a `PepperSnap`
   value. Turn it off; confirm the value is gone. Quit PepperSnap, then start it
   from a shortcut: the UAC prompt should appear, and declining it should still
   start the app unelevated. With PepperSnap already running, a second launch
   should show **no** prompt and should snip.
7. Options → "Restore everything to default"; confirm "Run as administrator" is
   still checked afterwards.
8. Type into a text box at each S / M / L / XL size, over both a light and a dark
   part of the screen: the outline should hug the glyphs, stay open in the
   counters of `o`, `e`, `a` and `4`, follow the text colour (black behind the
   bright swatches, white behind black), and look the same in the saved file as
   it did in the overlay.
9. In a custom-area session, click Copy, Pin, Quick save, Extract text and Close:
   the overlay should leave on the click, with the toast, pin or file following a
   moment later — no frozen toolbar in between.

## Release

Releases are built from `main` with `build.ps1` and published with the exe it
produced, `build\PepperSnap_v<version>.exe`, attached **under its unversioned
name**, `PepperSnap.exe`. A standalone program is updated by replacing the file,
and a versioned name is a different program as far as Windows is concerned: the
"Open with" entry, the default-app choice, shortcuts and the pinned icon are all
tied to the file's name and path, so they would have to be set again after every
release. The version still has to be somewhere, and it is: the tag, the file
properties, and the app's own window titles (Options, Customize keys, naming
syntax, Keyboard shortcuts and info) all carry it.

The tag is the same number with no `v` prefix (`4.23`), because the update check
compares the release tag against `APP_VERSION`. The **title** always carries the
`v` and is exactly `PepperSnap v<tag>` — `PepperSnap v4.23`, and
`PepperSnap v3.2.0.6` for the four-field tags of the 3.0–3.3 era, whose build
counter stays in the name. So: tag bare, title with `v`, attached file plain. All
40 releases to date read that way in their titles; sixteen had drifted (no `v`, a
lowercase `s`, a bare `4.2.0`, an empty title, `v4` for `4.0.0`) and were
normalised in October 2026. Their attached files still carry the version in the
name — the unversioned asset starts with the first release after `4.2.2`, which
is `4.23`. GitHub's own generated source archives are the source download —
there is no separate source zip any more.

The build is **not byte-reproducible**: the PE header carries the link time, so
two builds of identical source differ in that field and therefore in SHA-256.
A hash quoted for a release is only ever valid for the exact file that gets
attached, and rebuilding — even to fix a comment — invalidates it. Take the
hash from the finished asset and do not build again between writing it down and
publishing; if a rebuild does happen, re-take the hash.

The version number is part of the same decision. A plain build bumps the second
digit, which is the ordinary case and needs no thought: every build here carries
a change, so every build is a release. The leading digit moves only when the user
asks for it by name, never by inference. `-Version` sets the number outright —
that is how the scheme was folded to `4.23` — and is otherwise only for starting
a new major. That call belongs to whoever prepares the release — here the
assistant, acting for the maintainer — rather than being a question to ask each
time: the number only means something because someone weighed it, and the
reasoning goes into the message that accompanies the release, where a veto is
still cheap. One mechanic constrains it: the number is fixed by the build that
gets published, since the exe embeds it and a rebuild changes the hash, so a wrong
call is corrected by a later release, never by renumbering.
