# osu!mania Inverse Converter

[中文](README.zh-CN.md) | English

[![Language: C](https://img.shields.io/badge/Language-C99-blue.svg)]()

A lightweight osu!mania map conversion tool that can convert regular notes (Rice) into long notes (LN), or delete, keep, extend LNs, or convert all LNs back to Rice.  
It reserves a customizable gap (as 1/N beat) for generated long notes to suit different inverse playstyles.

---

## ✨ Features

- ✅ Lightweight: single-file C program, no external dependencies, fast execution
- ✅ Customizable gap length (default 1/4 beat, can be set to 1/8 beat, 1/2 beat, 1/16 beat, etc.)
- ✅ Four LN processing modes: keep, extend, delete, convert all to Rice
- ✅ Supports variable BPM maps (based on uninherited timing points)
- ✅ Supports 1K to 18K key counts
- ✅ Batch processing of multiple .osu files
- ✅ Automatic output file naming, with conversion marker added to difficulty name

---

## 🔧 Conversion Rules

### Rice Notes

For each rice note, the program looks for the next object in the same column (which may be a rice note or an LN):

- If the interval to the next object is ≥ 1/N beat and the resulting LN length is ≥ 10 ms, the note is converted to an LN.
- If the condition is not met or there is no next object, the note remains as a rice note.

### Existing LN Modes

You can choose one of the following four modes:

#### Mode 0: Keep
Original LNs are left completely unchanged.

#### Mode 1: Extend
If the interval between the end of an LN and the next object in the same column is greater than 1/N beat, the LN's end time is extended so that the interval equals 1/N beat; otherwise, it remains unchanged.

#### Mode 2: Delete
Delete all original LNs and reconstruct:

- For each original LN, find the previous object in the same column.
  - If the previous object is an LN: extend its end time to the current LN's start time.
  - If the previous object is a rice note:
    - If the interval is ≥ 10 ms, convert the rice note to an LN, with end time set to the current LN's start time.
    - If the interval is < 10 ms, keep the rice note unchanged.
- Delete the current LN.
- Add a new object at the current LN's end point:
  - If the interval to the next object in the same column is ≥ 1/N beat and the new LN length is ≥ 10 ms, add a new LN (start = original LN end, end = next object time - 1/N beat).
  - Otherwise, add a rice note.
- After processing all original LNs, apply Mode 0 Rice→LN conversion to the remaining rice notes.

Effect: The player releases at the start and end of the original LN, and holds elsewhere.

#### Mode 3: Convert all LNs to Rice (rc)
Convert all LNs to rice notes, leaving other objects unchanged.

---

## 📦 Compilation

### Windows (MinGW / MSYS2)

```bash
gcc -Wall -O2 -o osuInverseConverter.exe osuInverseConverter.c -lcomdlg32 -lm
```

### Linux / macOS

```bash
gcc -Wall -O2 -o osuInverseConverter osuInverseConverter.c -lm
```

> Note: Non-Windows platforms only support command-line mode; file selection dialogs and message boxes are not available.

---

## 🚀 Usage

### 1. Interactive mode (Windows only)

Double-click `osuInverseConverter.exe`. The program will ask you to enter:

1. Gap denominator N (e.g., `4` for 1/4 beat, `8` for 1/8 beat), or enter `rc` to switch to Mode 3 (convert all LNs to rice).
2. LN processing mode (if not `rc`): enter `0`, `1`, or `2` for keep, extend, or delete.

Then a file selection dialog appears. After selecting a `.osu` file, the conversion runs automatically and a message box appears when finished.

### 2. Drag-and-drop mode (Windows only)

Drag one or more `.osu` files onto the `osuInverseConverter.exe` icon. A console window will open and immediately ask for the gap denominator and LN processing mode. After input, all files are converted automatically, and a message box shows the number of processed files.

### 3. Command-line batch processing

```bash
osuInverseConverter.exe [-gap <N>] [-mode <m>] file1.osu [file2.osu ...]
```

- `-gap <N>`: sets the gap denominator N (gap = 1/N beat). Can be omitted; default is `4`.
- `-mode <m>`: sets the LN processing mode (`0`, `1`, `2`, or `3`). Can be omitted; default is `0`.
- Shortcut: use `rc` as a parameter, equivalent to `-mode 3`.
- Examples:
  ```bash
  # Use 1/8 beat gap, mode 2 (delete LNs)
  osuInverseConverter.exe -gap 8 -mode 2 song1.osu song2.osu

  # Use default 1/4 beat gap, mode 1 (extend LNs)
  osuInverseConverter.exe -mode 1 song.osu

  # Convert all LNs to rice
  osuInverseConverter.exe rc song.osu
  ```

---

## 📁 Output Files

Converted files are saved in the same directory as the original maps, with filenames:

- Mode 0/1/2: `original name (inverse 1-N).osu`
- Mode 3: `original name (rc).osu`

The difficulty name (`Version` field) inside the map will have ` (inverse 1/N)` or ` (rc)` appended accordingly.

For example, if the input is `song.osu` and N = 8, the output file will be `song (inverse 1-8).osu`, and the difficulty name becomes `OriginalDifficultyName (inverse 1/8)`. If `rc` is used, the output is `song (rc).osu`, and the difficulty name becomes `OriginalDifficultyName (rc)`.

---

## 🧪 Examples

### Example 1: Rice to LN (common to modes 0/1/2)

Assume BPM is 150, so 1/4 beat = 100 ms.

Original snippet (same column):

```
[HitObjects]
64,192,500,1,0,0:0:0:0:
64,192,800,1,0,0:0:0:0:
```

After conversion with 1/4 beat gap:

```
[HitObjects]
64,192,500,128,0,700:0:0:0:0:
64,192,800,1,0,0:0:0:0:
```

### Example 2: Extend LN (mode 1)

Original snippet (same column, gap = 1/4 beat = 100 ms):

```
64,192,1000,128,0,1200:0:0:0:0:
64,192,1500,1,0,0:0:0:0:
```

After conversion:

```
64,192,1000,128,0,1400:0:0:0:0:
64,192,1500,1,0,0:0:0:0:
```

### Example 3: Delete LN (mode 2)

Original snippet (same column, gap = 1/4 beat = 100 ms):

```
64,192,1000,128,0,1200:0:0:0:0:
64,192,1500,1,0,0:0:0:0:
```

After mode 2, this LN is deleted. A new LN is added at 1200ms (interval to next object 300ms ≥ gap), ending at 1500-100=1400ms:

```
64,192,1200,128,0,1400:0:0:0:0:
64,192,1500,1,0,0:0:0:0:
```

### Example 4: Convert all LNs to Rice (mode 3 / rc)

Original snippet:

```
64,192,1000,128,0,1200:0:0:0:0:
192,192,1500,1,0,0:0:0:0:
```

After conversion (LN becomes rice note, others unchanged):

```
64,192,1000,1,0,0:0:0:0:
192,192,1500,1,0,0:0:0:0:
```

---

## ❓ FAQ

**Q: Why were some rice notes not converted?**  
A: Possible reasons: the interval to the next object in the same column is less than 1/N beat; the resulting LN length would be less than 10 ms; the note is the last object in its column; or there is floating-point error.

**Q: How are variable BPM maps handled?**  
A: The program parses all uninherited timing points and calculates the gap based on the BPM at the note's position, correctly adapting to variable BPM maps.

**Q: How do I specify a 1/8 beat gap?**  
A: In interactive or drag-and-drop mode, enter `8`; on the command line use `-gap 8`, for example `osuInverseConverter.exe -gap 8 file.osu`.

**Q: Will the program modify the original files?**  
A: No, original files remain unchanged. Output is written to new files.

**Q: What is the command-line argument format?**  
A: Explicit parameters are supported: `-gap <N>` sets the gap denominator N (gap = 1/N beat), `-mode <m>` sets the LN mode (0/1/2/3). The `rc` shortcut is equivalent to `-mode 3`. Parameters must appear before file names.  
Examples:  
`osuInverseConverter.exe -gap 8 -mode 2 song.osu`  
`osuInverseConverter.exe rc song.osu`

**Q: How do I choose the LN processing mode?**  
A: In interactive or drag-and-drop mode, the program will ask `Process existing LNs? (0 = keep, 1 = extend, 2 = delete)`. On the command line, use the `-mode` parameter, e.g., `-mode 2`.

**Q: Why does the editor always show "Object end is not snapped to timeline"?**  
A: When converting notes, the program calculates LN end times using the current BPM and. Because the calculations often involve decimals, floating-point errors may cause the computed end times to fall slightly off the editor's beat grid, usually within 1 ms. If you want the map to look clean in the editor, you can use the editor's built-in "Resnap all notes" function after conversion.

---

## 📄 License

[LICENSE](LICENSE)