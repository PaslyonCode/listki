# Listki — portable sticky notes

Listki is a lightweight portable sticky-note application for Windows 10/11 x64. It needs no installer, .NET, Python, or network connection. All data stays next to `Listki.exe`.

## Quick start

1. Extract the archive to a normal writable folder, for example `Documents\Listki`.
2. Run `Listki.exe`.
3. A light semi-transparent floating button appears near the middle of the right edge. Drag it wherever you want.

The `Listki-data` folder is created automatically. It stores notes, notebooks, settings, and backups and must be writable.

## Language

Right-click the floating button or the tray icon and choose **Language → Русский** or **Language → English**. The choice is stored in `Listki-data\language.ini`; the note/index format `LISTKI01` is unchanged.

## Main menu

Click the floating button to open four actions:

- **New** — create an empty note.
- **From clipboard** — create a note from clipboard text.
- **Recent** — open up to the ten most recent notes.
- **Notebook** — choose a notebook, browse its notes, or create/manage notebooks.

Click outside the menu to close it. Near screen corners the actions stack automatically and stay inside the work area.

## Notes

- Click the title in the top bar to edit it inline.
- The default title is **“New note”** in English and **«Новая записка»** in Russian.
- Press **Enter** or **Tab** to commit the title. Press **Esc** to restore the title from the beginning of the edit.
- Clearing the title restores the default placeholder.
- **F2** and the Rename command focus the same inline title field.
- Body text supports normal copy, paste, and RichEdit editing.
- Drag the top bar to move a note; drag its borders or corners to resize it.
- The pin keeps a note above other windows.
- The trash button next to the pin deletes a note after confirmation. Its RTF copy is kept in `Listki-data\Deleted`.
- The color button opens five paper colors.
- The three-dot button or right-click opens formatting, text color, notebook, hide, and delete commands.
- Selecting text opens a compact **B / I / U / size** toolbar. The selection is preserved when a toolbar command is clicked.

Text and title changes are saved automatically. **Esc** in the body hides the note; **Esc** in the title field first cancels the title edit.

## Notebooks and search

List windows search note titles and the beginning of note text. Notebook rows show their note count. Right-click a notebook to rename or delete it; deleting a notebook moves its notes to **No notebook** without deleting them.

## Tray and startup

Right-click the tray icon to open the same menu. It can:

- show or hide notes;
- reset the floating button to the right edge;
- change inactive-note opacity;
- enable Windows startup for the current user;
- open the data folder;
- switch the language;
- exit the application.

Startup is disabled by default. Use the tray menu's **Exit** command to close the application.

## Keyboard shortcuts

| Keys | Action |
|---|---|
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut / paste text |
| Ctrl+A | Select all |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+B / Ctrl+I / Ctrl+U | Bold / italic / underline |
| Ctrl+S | Save immediately |
| F2 | Edit the title |
| Enter / Tab in the title | Commit the title |
| Esc in the title | Cancel the title edit |
| Esc in the body | Hide the note |

## Updating and moving

Exit through the tray and replace only `Listki.exe`. Keep `Listki-data` in place: notes, notebooks, formatting, language, and window positions are preserved. Before moving the application, disable startup, move the EXE together with `Listki-data`, then enable startup again.

## Building from source

Install MinGW-w64 for x86-64.

```bash
cd source
bash build.sh
```

The result is `source/build/Listki.exe`. On Windows, run `source\build.bat`.

Model, UI-geometry, and language checks run with ordinary `g++`; the Windows integration test is compiled with MinGW-w64. Run the full Win32 GUI test on Windows 10/11.

## Data format and backups

`Listki-data\index.dat` uses the `LISTKI01` format and remains compatible with previous releases. Each note's text and formatting are stored in a separate RTF file; `.bak` files contain the previous saved version. The language preference is stored separately in `language.ini`.

Listki works locally and does not send data over the network.
