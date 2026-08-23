# osu!mania Inverse Converter

[![Language: C](https://img.shields.io/badge/Language-C99-blue.svg)]()

一个将 osu!mania 中的谱面转换为反键（Inverse）的小工具。  
转换时会为每个生成的长条预留一个可自定义长度的间隙。原有长条保持不变。

---

## ✨ 功能特性

- ✅ 可自定义间隙长度（默认 1/4 拍，可设为 1/8、1/2、1/16 等）
- ✅ 支持变速谱面（根据红线自动计算 BPM）
- ✅ 支持 1K ~ 18K 所有键位
- ✅ 支持批量处理多个 .osu 文件
- ✅ 跨平台：Windows 图形对话框 + 命令行模式；Linux/macOS 命令行模式
- ✅ 输出文件自动命名，并在难度名后添加转换标记

---

## 🔧 转换规则

对于每个Note（Rice），程序会查找**同一列**的下一个对象（可能是Rice或LN）：

- 若与下一个对象间隔 ≥ 1/N 拍且生成的 LN 长度 ≥ 10 ms，则将该音符转换为 LN。
- 若不满足条件或没有下一个对象，则保留为Rice。
- 原有LN完全保留。

---

## 📦 编译

### Windows (MinGW / MSYS2)

```bash
gcc -Wall -O2 -o osuInverseConverter.exe osuInverseConverter.c -lcomdlg32 -lm
```

### Linux / macOS

```bash
gcc -Wall -O2 -o osuInverseConverter osuInverseConverter.c -lm
```

> 注意：非 Windows 平台仅支持命令行参数模式，不支持文件选择对话框和消息框。

---

## 🚀 使用方法

### 1. 交互模式（仅 Windows）

打开 `osuInverseConverter.exe`，程序会先要求输入一个正整数（例如输入 `4` 表示 1/4 拍），然后弹出文件选择对话框，选择 `.osu` 文件后自动转换，完成后弹出提示框。

### 2. 拖拽模式（仅 Windows）

将一个或多个 `.osu` 文件拖拽到 `osuInverseConverter.exe` 图标上，程序会打开控制台窗口要求输入一个正整数，输入后自动批量转换所有文件，完成后弹窗显示处理数量。

### 3. 命令行批量处理

```bash
osuInverseConverter.exe [一个正整数] 文件1.osu [文件2.osu ...]
```

- 示例：
  ```bash
  # 使用 1/8 拍间隙批量转换
  osuInverseConverter.exe 8 song1.osu song2.osu

  # 使用默认 1/4 拍间隙
  osuInverseConverter.exe song1.osu song2.osu
  ```

---

## 📁 输出文件

转换后的文件保存在原谱面同目录下，文件名为：

```
原文件名 (inverse 1-N).osu
```

谱面内的难度名（`Version` 字段）会添加 ` (inverse 1/N)` 标记。

例如，输入 `song.osu`，使用 gap 除数 8，输出文件为 `song (inverse 1-8).osu`，难度名变为 `原来的难度名 (inverse 1/8)`。

---

## 🧪 示例

假设 BPM 为 150，则 1/4 拍 = 100 ms，间隔 300 ms，满足转换条件，生成 LN 结束时间 = 800 - 100 = 700 ms。

### 原谱面 HitObjects 片段

```
[HitObjects]
……
64,192,500,1,0,0:0:0:0:
64,192,800,1,0,0:0:0:0:
……
```

### 使用 1/4 拍间隙转换后

```
[HitObjects]
……
64,192,500,128,0,700:0:0:0:0:
64,192,800,1,0,0:0:0:0:
……
```

---

## ❓ 常见问题

**Q: 为什么有些Rice没有转换？**  
A: 可能原因：与下一个同列对象的间隔不足 gap；生成的 LN 长度小于 10 ms；该音符是所在列的最后一个对象；运算可能存在浮点数误差。

**Q: 如何处理变速？**  
A: 程序解析所有红线（uninherited timing points），根据音符所在时间点的 BPM 计算 timing，能正确适应变速谱面。

**Q: 如何指定 1/8 拍间隙？**  
A: 交互模式或拖拽时输入 `8`；命令行使用 `osuInverseConverter.exe 8 文件.osu`。

**Q: 程序会修改原文件吗？**  
A: 不会，原文件保持不变，输出为新文件。

**Q: 为什么命令行第一个参数是数字但被当作文件名？**  
A: 如果当前目录存在同名文件（例如 `8.osu` 且传入 `8`），程序会优先视为文件路径。建议使用完整文件名（含扩展名）或使用拖拽方式避免歧义。

---

## 📄 许可证

[LICENSE](LICENSE)
