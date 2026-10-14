# EditHere: User guide

> Local whiteboard variant: launch to an empty workspace, then paste, drop, or open an image. System screenshots, OCR, scrolling capture, pinning, and global shortcuts are removed.


[简体中文](USER-GUIDE.md) · **English**

[Back to the product overview](../README.en.md) · [AI and the command line](AGENT-CLI.en.md) · [Changelog](../CHANGELOG.md) · [Development notes](DEVELOPMENT.en.md)

This page covers **0.11.1** and describes capture, video annotation, comments, component adjustments, project saving, feedback export and platform limits. The public download links below point to GitHub's current public release. Paths are relative to the project root; see the [licence](../LICENSING.md) for download and usage terms.

## Contents

- [Running and updating](#running-and-updating)
- [Annotation editing](#annotation-editing)
- [Video annotation](#video-annotation)
- [Explode](#explode)
- [Save and export](#save-and-export)
- [Feedback JSON](#feedback-json)
- [Settings](#settings)
- [Working with AI](#working-with-ai)
- [Recognition and platform boundaries](#recognition-and-platform-boundaries)

## Running and updating

Build this whiteboard variant from the current source. Launching opens an empty workspace; paste, drop or import an image. Screenshot features and screen permissions are removed. See [Development](DEVELOPMENT.en.md). Upstream packages contain the original product.

## Annotation editing

- The top tools are, in order, **Smart select**, **Point**, **Box**, **Adjust**; then **Explode**, undo, redo, **Fit image**, **Hide / show on-canvas annotations**, **Full screen**, and a button to collapse the annotation column. The default tool is chosen in **Settings → Toolbar → Default annotation tool**.
- The annotation column is permanently reserved from the start. Click an index or a move arrow and edit directly on the right; leaving the input box saves automatically, and the plus sign in the top-right adds a global note. Releasing a box selection immediately lets you type an annotation in the sidebar while keeping the manual region for component adjustments. When the window narrows, a button on the annotation column collapses the sidebar.
- Scrolling on empty space, or `Ctrl` + scrolling, zooms the whole image around the pointer; holding the middle mouse button pans freely, and it works even when the image is smaller than the window and can go past all four edges, in both ordinary annotation and Explode mode. `Ctrl + 0` fits to window. In an ordinary window, drag the title bar, or hold space / `Alt` and drag the image, to move the window.
- The Adjust tool moves or resizes annotation boxes; click an index to edit its text. `Ctrl + Z` undoes, `Ctrl + Y` redoes, and `Delete` removes the selected annotation.
- You can open, drop or paste PNG / JPEG / WebP / BMP, and reopen a `.edithere` project or feedback JSON.
- Closing the current capture prompts you to save unsaved changes, then releases the capture and slice caches; the app stays in the tray.
- The **eye** button at the top hides the marks on the image and keeps the annotation text on the right, to view the original or export a clean comparison image.

Mac editing shortcuts use Command and redo is Command + Shift + Z; global capture uses Option + Shift + 2, and new annotation uses Option + Shift + 1. Mac has not yet been verified on a real machine.

## Video annotation

Open a file or drop in a video to start viewing; click “Play” to play it and “Pause” to hold the frame. After dragging the timeline or using `−1s` / `+1s` to seek, the frame stops where you want: seeking while paused stays paused, and seeking while playing also pauses. An ordinary pause, the first frame and a seek still show the video; only the first time you actually add a point, box or global annotation is a screenshot saved and the annotation canvas entered. Selecting a mode, zooming and panning do not create a screenshot. Click “Play” again or press space to resume, and add the next set of annotations after finding another timestamp. Clicking the cyan downward-pointing tab on the timeline, or choosing from the “Annotated frames” list, restores an existing screenshot and its annotations directly; space toggles play and pause (it still types a space inside a text input box).

Saving an `.edithere` video project references the source video address and embeds the original images of the annotated frames and the editing state. Saving a project or exporting JSON does not automatically add a screenshot of a frame you merely paused to view. If the source video moves or is lost you can still view the saved screenshots; click “Locate the video again” to resume playback. Exported JSON uses `video-feedback-1`: each frame contains the actual screenshot timestamp, image feedback and an optional embedded screenshot, and the root object index attaches `timestampMs` and `frameId` to every shape annotation.

Timestamps are relative to the start of the source video, and coordinates are pixels in the corresponding frame's screenshot. The same coordinate at different timestamps is separate feedback, and an AI needs to read the time, the screenshot and the comment together. See the [video annotation guide](VIDEO-ANNOTATION.en.md) for the full structure, how to share the screenshot files, and the actual verification record. This round's video feature was acceptance-tested on Windows; other platforms follow their own build and acceptance results.

## Explode

Click the “Explode” button at the top; the button changes colour and an iridescent wave sweeps from top-right to bottom-left. Components are adjusted directly inside the **current window**.

Entering it for the first time splits the image into blocks along the detected boxes; when you click the button again to leave the mode, the image keeps its adjustments and the slice cache is kept. Reopening uses the same blocks and adjustments without re-splitting; the cache is cleared only after you close the current capture.

While Explode is on, the annotation column always stays on the right; the Point tool adds comments to the result, and indices and move arrows can be clicked to edit directly. The Smart tools select and adjust blocks, and Box adds manual regions; after selecting a component you can add an annotation on the right. The component adjustment area is fixed at the bottom of the sidebar, and the annotation list scrolls independently. Annotations attached to a component move and scale with it, and the editing result and annotations share the same canvas coordinates.

| Action | Behavior |
| --- | --- |
| Hover and scroll with nothing selected | Cycle through every candidate region under the cursor |
| Click a candidate region | Select a component or component group |
| Drag inside a selected region | Move the component |
| Drag an edge midpoint | Change the width or height |
| Drag a corner | Scale proportionally, without Shift |
| Scroll while pointing at a selected component | Scale the component up or down; pointing at empty space zooms the whole image |
| X, Y, width, height, scale on the right | Set the pixel position, size and ratio precisely |
| Arrow keys | Move 1 px |
| `Ctrl` + click | Multi-select; the selected blocks drag or transform together |
| Right-click, Esc or “Deselect” | Return to hover-based selection |
| Draw a box after “Box” at the bottom | Add an operable region, before or after exploding |
| `Ctrl` + scroll | Zoom the view |

When you select a large box, the components inside move and scale with it, keeping their relative layout; once a block is moved out of its parent group it leaves that group, and moving the parent no longer carries it. Undo and redo are supported. After a move, the original position is left empty and shown as a transparent chequerboard; no background is filled in and no covered content is restored. Adjustments are limited to a canvas the size of the original image.

A move becomes an annotation the moment it happens, listed alongside text annotations in the order it occurred, and its position is not affected by text you write afterwards; it can have no text and can be filled in later by opening it, and its index matches the arrow on the image. Once a block is moved back to its original position, move annotations with no text are removed along with it, while any that already have text are kept.

## Save and export

| Action | Default shortcut | Result |
| --- | --- | --- |
| Save project | `Ctrl + S` | An `.edithere` file that always embeds the original image and the slice cache, and can be reopened to keep editing |
| View JSON | `Ctrl + E` | Open the feedback preview window, where you can copy or save |
| Copy annotated image | `Ctrl + C` | Put the annotated image on the clipboard; the image area is a uniform 960 pixels wide, and indices, text and the sidebar keep a consistent scale |
| Copy JSON file / Copy JSON content / Save image / Hide marks / Global annotation | Not bound by default | Assign them yourself in Settings |

The bottom toolbar shows **Save project**, **Save image**, **View JSON**, **Copy JSON content**, **Copy JSON file**, **Copy annotated image**, **Retake screenshot** and **Fit image** by default; which buttons appear is chosen in **Settings → Toolbar**, and unchecked buttons do not appear in the bottom bar.

The **“Need help?”** entry on the toolbar and at the top of the annotation window explains what to do when an agent cannot take the feedback.

The **View JSON** window provides three options and actions:

- **Includes the source image; fully self-contained**: by default follows “Include the source image in exported JSON by default” in Settings. When unchecked, the JSON contains no image and reopening needs a PNG of the same name alongside it.
- **Compressed preview (same size, may lose slight detail)**: converts the embedded image to JPEG, keeping the size and reducing the file.
- **Copy JSON file**: writes the JSON to a temporary file and copies that file (not the text), for agents that can be sent files directly; the temporary folder is configured in **Settings → Default behavior → Feedback temporary folder**.
- **Copy JSON content**: copies the JSON text, for agents that can only paste text.
- **Save JSON and image**: saves the JSON and the accompanying image to a chosen folder.

Copying and exporting JSON use the same minimal structure; see [Feedback JSON](#feedback-json) for the fields.

## Feedback JSON

Image feedback uses an **object-oriented** structure: each object records a source region `source`, the position changes it undergoes `movements`, and the comments attached to that region `annotations`, so an AI can directly pair the comments and moves of the same object. Video feedback reuses this structure inside `frames[].feedback` and adds frame timestamps, source video information and a timestamped object index; see [Video annotation](#video-annotation) for details.

| Field | Meaning |
| --- | --- |
| `image` | Optional data URL of the image before adjustment (PNG, or JPEG when compressed); the size is unchanged and compression loses a little detail |
| `annotationSpace` | Fixed to `"result"`, meaning annotation coordinates refer to the current adjusted image |
| `objects[].source` | The object's region in the original image; `null` for a global note |
| `objects[].movements` | Records this region's moves or resizes in order, with `to` as the final coordinates |
| `objects[].annotations` | Text notes attached to this object |

Components that do not change in image feedback are not exported; no full slice list, component group references, hashes or timestamps are output. The Base64 source image increases the file size, which is the cost of putting the image in a single JSON and is independent of the number of components. The image format is defined by `schema/feedback-minimal.schema.json`; video feedback and projects are defined by `schema/video-feedback-v1.schema.json` and `schema/video-project-v1.schema.json` respectively.

Coordinates take the top-left of the canvas as the origin and are in image pixels, independent of the window position and view zoom. A box uses the two endpoints `x1/y1/x2/y2`; the bottom-right boundary excludes itself, and the width and height are `x2-x1` and `y2-y1`. Annotation coordinates are integers, while layout change coordinates may contain decimals.

The example below omits the optional `image` field and shows a point comment, a region comment, a moved object and a global comment:

```json
{
  "annotationSpace": "result",
  "objects": [
    {"source": {"x1": 240, "y1": 180, "x2": 240, "y2": 180},
     "movements": [], "annotations": ["Change this to blue"]},
    {"source": {"x1": 200, "y1": 140, "x2": 400, "y2": 240},
     "movements": [], "annotations": ["Add rounded corners"]},
    {"source": {"x1": 100, "y1": 100, "x2": 300, "y2": 200},
     "movements": [{"to": {"x1": 200, "y1": 140, "x2": 400, "y2": 240}}],
     "annotations": ["Move the whole block down and to the right a little"]},
    {"source": null, "movements": [], "annotations": ["Overall lighter and quieter, less decoration"]}
  ]
}
```

To reconstruct the result, first extract each object's `source` region from the original image and leave the original position empty, then draw them to `to` in the order of `movements`; finally interpret or draw the annotations on the result. Regions that were not moved stay as they are.

The parallel `annotations` and `changes` arrays used by earlier versions (the legacy format) can still be imported, and are merged into the object structure by source region on import:

| Format | Original image | Annotation coordinates | Layout information |
| --- | --- | --- | --- |
| v1/v1.1/v2 projects from 0.5 and earlier | `capture.pngBase64` can be embedded, or references an external PNG | Original image | v2 saves every slice, group and the original/target coordinates |
| 0.6 minimal feedback | External PNG with the same name | Original image | Only the changed `from/to` |
| 0.7 / 0.8 feedback | `image` can be embedded | Adjusted image | Only the changed `from/to` in `changes` |
| 0.9 onwards (current) | `image` can be embedded, included by default in the interface | Adjusted image | `source` and `movements` recorded per object in `objects` |
| 3.0.0 `.edithere` project | Always embeds the original image | Adjusted image | Full slice cache and current editing state |

The app no longer opens legacy projects or 0.6 minimal feedback, and old annotations are no longer converted to the position in the adjusted result. When you clear the option to include the source image, reopening needs an original PNG of the same name alongside it. An `.edithere` project saves the full slices and editing state (`schemaVersion: "3.0.0"`) and restores directly when reopened. Closing the whiteboard releases the in-memory cache. When you reopen feedback JSON, the layout is rebuilt from the original image and the changes it contains.

## Settings

Right-click the EditHere tray icon at the bottom-right of the taskbar and choose “Settings”. The window has six tabs: “Shortcuts”, “Appearance”, “Default behavior”, “Toolbar” “About and updates” and “Diagnostics”. Click “Save” to apply the settings; shortcuts and appearance take effect immediately, the default tool and zoom apply to the next image, and the startup behavior applies on the next launch. “Cancel” does not change the current configuration.

### Shortcuts

All configurable shortcuts apply inside the EditHere editor; system-wide shortcuts are removed. Click a field and press a new combination, or clear it to disable that shortcut. “Restore defaults” resets the current settings draft and still needs saving to take effect. Duplicate assignments, system conflicts or a failed save show the reason and keep the existing configuration.

The following are the Windows defaults; common Mac editing combinations use Command, redo is Command + Shift + Z, and real-machine verification is still pending.

| Scope | Action | Default shortcut |
| --- | --- | --- |
| In-app | Open image or project | Ctrl + O |
| In-app | Paste image | Ctrl + V |
| In-app | Save project | Ctrl + S |
| In-app | View JSON | Ctrl + E |
| In-app | Copy annotated image | Ctrl + C |
| In-app | Copy JSON / Save image / Hide or show annotation marks / Add global annotation | Not bound by default |
| In-app | Undo / Redo | Ctrl + Z / Ctrl + Y |
| In-app | Fit to window | Ctrl + 0 |
| In-app | Delete selected annotation | Delete |
| In-app | Cancel / Close whiteboard | Esc |
| In-app | Smart select / Point / Box / Adjust | B / P / R / V |
| In-app | Toggle Explode / Adjust components | E / M |


### Appearance

- **Appearance mode**: Follow system, Light or Dark, defaulting to Follow system. After switching, the editor, annotation column, component sidebar, menus and Settings window update together; the original pixels of the source image and exported images are unaffected by the theme.
- **Interface language**: Follow system, Simplified Chinese or English. Switching previews immediately, takes effect on save, and returns to the previous language on cancel. The system file dialogs, message boxes and colour picker follow it too. The JSON field values exported for AI do not change with the interface language.

### Default behavior

| Option | Description |
| --- | --- |
| Start EditHere at login | After you sign in it only stays in the tray and does not capture automatically; keep the folder the app is in, and after moving it save once to refresh the path |
| Fit to window when opening an image | When off, images open at 100%; you can still zoom or fit to window at any time |
| Include the source image in exported JSON by default | Sets the initial state of “Includes the source image” in “View JSON”; saving a project always includes the source image |
| Ask about saving when the annotation window closes | When off, closing no longer prompts and discards unsaved annotations directly; ticking “Do not ask again” in the annotation window also turns this off |
| Feedback temporary folder | Where temporary feedback JSON files are stored; leave empty to use `feedback` under the default cache directory |

Windows uses the current user's startup entry and needs no administrator privileges. If you enable it for the portable version, keep the folder the app is in; if Windows has disabled the startup entry, the app tells you to restore it manually in the system's Startup Apps settings and does not force the disabled state to change. Mac uses the system login items and warns you clearly if the system asks for approval; Mac still has not been acceptance-tested on a real machine.

### Toolbar

- **Default annotation tool**: Smart select, Point, Box or Adjust; sets the tool selected by default after opening an image.
- **Shown in the bottom toolbar**: Save project, Save image, View JSON, Copy JSON content, Copy JSON file, Copy annotated image, Retake screenshot and Fit image. Unchecking removes a button from the bottom bar; the feature is still available through a shortcut or the menu.

### Diagnostics

After a problem, open **Settings → Diagnostics**, click **Export diagnostics…**, and report the exported text file together with the steps to reproduce and when the problem occurred. **Open log folder** shows the automatically saved JSONL logs directly. The logs record the version, the system and Qt environment, capture sizes, operation states and errors, but not screenshots, videos or annotation text, and they are never uploaded automatically.

By default each log file is at most 1 MiB and the last 10 files are kept; the log of a running process is protected and the limit resumes after it exits. A single exported report is at most 4 MiB and includes recent logs and the runtime environment. Logs use the current user's data directory: on Windows `%LOCALAPPDATA%/EditHere/EditHere/logs`, and on macOS and Linux each platform's Qt `AppLocalDataLocation`. On launch the app records a previous session that did not end cleanly; this may come from a crash, a forced termination or a write failure, and you cannot determine the cause from that record alone. Logs filter out your home directory, URL query parameters and common secret fields, but you can still review them before sharing.

### About and updates

Shows the current version and licence information. You can tick **Check for updates on launch**, or click **Check for updates** to check manually; once a new version is found the button becomes **Update now** — see [Development](DEVELOPMENT.en.md). The **Open the guide** button closes Settings and plays the guide from the start, and unsaved settings are not saved; **Restore defaults** resets the current settings draft and still needs saving to take effect.

Settings are saved in the current user's Qt `AppConfigLocation/settings.ini` and are kept after the app closes. On Windows the default location is `%LOCALAPPDATA%/EditHere/EditHere/settings.ini`; on Mac it is decided by the system configuration directory.

## Guide and launch at login

- The first manual launch shows a six-step guide in the real editor window, and you can skip it at any time; it uses a sample when there is no capture and keeps your content when there is one. Both the **?** in the top-right and **Open the guide** at the bottom of Settings start it from the beginning. The guide pages forwards and backwards, and Esc only leaves the guide.
- The guide state is saved separately from your settings preferences, so restoring the default settings does not make the guide reappear repeatedly. Replaying the guide from Settings closes the Settings window and does not save changes you have not applied.
- After you sign in, the app only stays in the tray and does not capture automatically; A manual launch opens the whiteboard.

## Working with AI

You can copy the JSON manually, or let an AI connected to the CLI start an annotation session. In the latter, the AI opens the image, you add annotations or adjust components, and finally click **“Finish and return to AI” (完成并返回 AI)** at the top; only then does the CLI export this round's feedback and return success. Clicking “Cancel” or waiting past the timeout does not submit feedback, and the editor keeps the current document. A broken connection can leave the result unclear, so check whether this round has already completed rather than starting the session again.

Each round of feedback uses a new output file, and the AI reads only the result explicitly submitted in this round and must not treat an earlier file as new feedback. See [AI and the command line](AGENT-CLI.en.md) for the companion skill's installation, commands and examples.

## Recognition and platform boundaries

Smart selection analyzes colors, edges and grids in imported images locally. Use manual regions when detection misses a component. System capture, OCR and accessibility probing are removed; screen recording and accessibility permissions are not required.
