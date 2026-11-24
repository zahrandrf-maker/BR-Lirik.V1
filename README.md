# BR Lirik v0.2.0 Prototype

Transparent live-lyrics FFGL source for Resolume Arena on Windows.

## Workflow

1. Add **BR Lirik** to two layers.
2. Set the first instance to `Output Role: PROGRAM`.
3. Set the second instance to `Output Role: PREVIEW - NEXT` and route that layer to the preview screen you use in Resolume.
4. Click **Add / Edit Lyrics**. Each input line is one lyric cue. Enter creates the next cue. A blank line creates a blank/clear cue.
5. Click **Update Lyrics**.
6. Use Previous, Next, Go To, or Reset from either instance. Both instances share the same lyric list and cue position.

The width of the text box controls word wrapping. One long input line can therefore appear as two or more visual lines without becoming multiple cues.

## Prototype features

- Resizable multiline Windows lyric editor
- Load and Save UTF-8 TXT
- Autosave of the last edited lyric list
- Program and next-cue preview roles
- Preview label and cue number
- Previous, Next, Go To, Reset, and preview visibility
- Cut, Fade, Roll Up + Fade, and Slide Left + Fade
- Fifteen transition choices including directional slides, zoom, pop, wipe, blur, and Stacked Next Preview
- One-click transitions continue to completion; optional navigation lock prevents accidental double Next
- Slow Floating and Rock Shake motion
- Installed Windows fonts plus font-name search
- Word wrap and automatic font fitting
- Two-colour font gradient with four directions
- Text fill, stroke, and stroke-only style
- Editable drop shadow and glow
- Off, fill, fill + stroke, and stroke-only rounded boxes
- Transparent premultiplied output
- Direct `.txt` / raw-text URL import through the lyric editor
- Automatic removal of full bracket-label lines such as `[CHORUS]`, `[VERSE 1]`, and `[BRIDGE]`

## GitHub Actions build

Upload the contents of this folder to the repository root, including the hidden `.github` folder. Open **Actions**, run **Build BR Lirik**, and download the artifact. Extract the GitHub ZIP and then the nested `.tar.gz` file.

Copy `BR Lirik.dll` to:

`Documents\Resolume Arena\Extra Effects`

Restart Resolume, then find **BR Lirik** under Sources.

## Prototype note

This is the first test version. Test cue synchronization, editor behavior, wrapping, transitions, and performance before using it in a live production.

Effect by Belajar Resolume.
