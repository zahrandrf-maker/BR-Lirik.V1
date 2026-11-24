# BR Lirik v0.3.1 — Text + PNG Lyrics

Transparent live-lyrics FFGL source for Resolume Arena on Windows.

## Workflow

1. Add **BR Lirik** to two layers.
2. Set the first instance to `Output Role: PROGRAM`.
3. Set the second instance to `Output Role: PREVIEW - NEXT` and route that layer to the preview screen you use in Resolume.
4. Choose `Input Mode: TEXT LYRICS` or `PNG LYRICS`.
5. For text, click **Add / Edit Text Lyrics**. Each input line is one lyric cue. Enter creates the next cue.
6. For PNG, click **Browse PNG Lyrics** and multi-select the PNG files in Windows File Explorer.
7. Use the same Previous, Next, Go To, Reset, Preview, and transition controls for either mode.

The width of the text box controls word wrapping. One long input line can therefore appear as two or more visual lines without becoming multiple cues.

## Prototype features

- Resizable multiline Windows lyric editor
- Separate TEXT LYRICS and PNG LYRICS input modes
- Multi-select transparent PNG cues from Windows File Explorer
- PNG Fit/Contain, Fill/Cover, Stretch, and Scale controls
- Text and PNG share the same navigation, preview, transitions, motion, shadow, glow, box, and screen-position controls
- Load and Save UTF-8 TXT
- Autosave of the last edited lyric list
- Program and next-cue preview roles
- Preview label and cue number
- Previous, Next, Go To, Reset, and preview visibility
- Optional **Blank First & End Lyrics** cues for a clean opening and ending
- Cut, Fade, Roll Up + Fade, and Slide Left + Fade
- Sixteen transition choices including directional slides, zoom, pop, wipe, blur, Stacked Next Preview, and Typewriter
- Typewriter transition for text lyrics; typing speed follows Transition Duration
- One-click transitions continue to completion; optional navigation lock prevents accidental double Next
- Slow Floating and Rock Shake motion
- Installed Windows fonts plus font-name search
- Word wrap and automatic font fitting
- Screen-position presets: Fit to Top, Fit to Middle, Fit to Bottom, plus Custom X/Y
- Two-colour font gradient with four directions
- Text fill, stroke, and stroke-only style
- Editable drop shadow and glow
- Off, fill, fill + stroke, and stroke-only rounded boxes
- Independent Box Opacity control
- Transparent premultiplied output
- Optional removal of full bracket-label lines such as `[CHORUS]`, `[VERSE 1]`, and `[BRIDGE]`; disabled by default

## GitHub Actions build

Upload the contents of this folder to the repository root, including the hidden `.github` folder. Open **Actions**, run **Build BR Lirik**, and download the artifact. Extract the GitHub ZIP and then the nested `.tar.gz` file.

Copy `BR Lirik.dll` to:

`Documents\Resolume Arena\Extra Effects`

Restart Resolume, then find **BR Lirik** under Sources.

## Prototype note

This is the first test version. Test cue synchronization, editor behavior, wrapping, transitions, and performance before using it in a live production.

Effect by Belajar Resolume.
