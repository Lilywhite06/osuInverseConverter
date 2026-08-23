# osu!mania Inverse Converter

[中文](README.zh-CN.md) | English

[![Language: C](https://img.shields.io/badge/Language-C99-blue.svg)]()

A small tool that converts osu!mania maps into **inverse**.  
It reserves a customizable gap for each generated long note. Original long notes remain unchanged.

---

## ✨ Features

- ✅ Customizable gap length (default 1/4 beat, can be set to 1/8, 1/2, 1/16, etc.)
- ✅ Supports variable BPM maps (based on uninherited timing points)
- ✅ Supports all key counts from 1K to 18K
- ✅ Batch processing of multiple .osu files
- ✅ Cross-platform: Windows GUI dialog + command line; Linux/macOS command line
- ✅ Automatic output file naming, with conversion marker added to difficulty name

---

## 🔧 Conversion Rules

For each rice note, the program looks for the next object in the **same column** (which may be a rice note or an LN):

- If the interval to the next object is ≥ 1/N beat and the resulting LN length is ≥ 10 ms, the note is converted to an LN.
- If the condition is not met or there is no next object, the note remains as a rice note.
- Original LNs are preserved completely.

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

Open `osuInverseConverter.exe`. The program will first ask you to enter a positive integer (for example, `4` means 1/4 beat), then it will open a file selection dialog. After selecting a `.osu` file, the conversion runs automatically and a message box appears when finished.

### 2. Drag-and-drop mode (Windows only)

Drag one or more `.osu` files onto the `osuInverseConverter.exe` icon. A console window will open and ask for a positive integer. After input, all files are converted automatically, and a message box shows the number of processed files.

### 3. Command-line batch processing

```bash
osuInverseConverter.exe [positive integer] file1.osu [file2.osu ...]
```

- Examples:
  ```bash
  # Use 1/8 beat gap for batch conversion
  osuInverseConverter.exe 8 song1.osu song2.osu

  # Use default 1/4 beat gap
  osuInverseConverter.exe song1.osu song2.osu
  ```

---

## 📁 Output Files

Converted files are saved in the same directory as the original maps, with the filename format:

```
original name (inverse 1-N).osu
```

The difficulty name (`Version`) inside the map will have ` (inverse 1/N)` appended.

For example, if the input is `song.osu` and the gap divisor is 8, the output file will be `song (inverse 1-8).osu`, and the difficulty name becomes `OriginalDifficultyName (inverse 1/8)`.

---

## 🧪 Example

Assume BPM is 150, so 1/4 beat = 100 ms. If the interval is 300 ms, the conversion condition is met, and the LN end time = 800 - 100 = 700 ms.

### Original HitObjects snippet

```
[HitObjects]
……
64,192,500,1,0,0:0:0:0:
64,192,800,1,0,0:0:0:0:
……
```

### After conversion with 1/4 beat gap

```
[HitObjects]
……
64,192,500,128,0,700:0:0:0:0:
64,192,800,1,0,0:0:0:0:
……
```

---

## ❓ FAQ

**Q: Why were some rice notes not converted?**  
A: Possible reasons: the interval to the next object in the same column is less than the gap; the resulting LN length would be less than 10 ms; the note is the last object in its column; or there is floating-point error.

**Q: How are variable BPM maps handled?**  
A: The program parses all uninherited timing points and calculates the timing based on the BPM at the note's position, correctly adapting to variable BPM maps.

**Q: How do I specify a 1/8 beat gap?**  
A: In interactive or drag-and-drop mode, enter `8`; on the command line use `osuInverseConverter.exe 8 file.osu`.

**Q: Will the program modify the original files?**  
A: No, original files remain unchanged. Output is written to new files.

**Q: Why might a number given as the first command-line argument be treated as a filename?**  
A: If a file with the same name exists in the current directory (for example, `8.osu` and you pass `8`), the program gives priority to treating it as a file path. To avoid ambiguity, use the full filename with extension, or use drag-and-drop mode.

---

## 📄 License

[LICENSE](LICENSE)