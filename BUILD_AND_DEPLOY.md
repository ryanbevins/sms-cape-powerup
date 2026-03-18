# Build & Deploy Guide

## Prerequisites

- **Python 3** with `pyisotools` installed (`python -m pip install pyisotools`)
- **CMake 3.8+** and **Ninja** on PATH
- **Clean NTSC-U (GMSE01) Super Mario Sunshine ISO**
- **BetterSunshineEngine v4.0.0 release** — download `BetterSunshineEngine_RELEASE.zip` from [GitHub releases](https://github.com/DotKuribo/BetterSunshineEngine/releases/tag/v4.0.0)

## First-Time Setup

### 1. Clone the repo with submodules

```bash
git clone --recursive https://github.com/ryanbevins/sms-cape-powerup.git
cd sms-cape-powerup
```

If you already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

### 2. Extract the ISO with pyisotools

```bash
python -m pyisotools "path/to/Super Mario Sunshine (USA).iso" E --dest game
```

This creates `game/root/` with `sys/` and `files/` inside.

### 3. Install BetterSunshineEngine

Extract the BSE release zip. From inside the extracted folder:

```bash
# Replace the DOL with BSE's Kuribo-patched version
cp main.dol game/root/sys/main.dol
cp boot.bin game/root/sys/boot.bin
```

Create the Kuribo directories and copy the kernel + BSE module:

```bash
mkdir -p "game/root/files/Kuribo!/System"
mkdir -p "game/root/files/Kuribo!/Mods"
cp Kuribo!/System/KuriboKernel.bin "game/root/files/Kuribo!/System/"
cp Kuribo!/Mods/BetterSunshineEngine.kxe "game/root/files/Kuribo!/Mods/"
```

> **Note:** The `!` in `Kuribo!` can cause issues in some shells. Use quotes or Python to handle it.

## Build the Module

### Configure (only needed once, or after CMake changes)

```bash
cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=targets/GCNKuriboClangRelease.cmake
```

### Build

```bash
ninja -C build
```

Output: `build/CapePowerup.kxe`

## Deploy & Package

### Copy the module into the extracted game

The `_` prefix ensures it loads after BetterSunshineEngine:

```bash
cp build/CapePowerup.kxe "game/root/files/Kuribo!/Mods/_CapePowerup.kxe"
```

On Windows, if the shell fights you on the `!`:
```python
python -c "
import shutil, pathlib
shutil.copy('build/CapePowerup.kxe', pathlib.Path(r'game\root\files\Kuribo!\Mods\_CapePowerup.kxe'))
"
```

### Rebuild the ISO

```bash
python -m pyisotools game/root B --dest sms-cape-modded.iso
```

### Open in Dolphin

Open `sms-cape-modded.iso` in Dolphin. No special settings required.

## Quick Rebuild Cycle

After editing source code:

```bash
ninja -C build
cp build/CapePowerup.kxe "game/root/files/Kuribo!/Mods/_CapePowerup.kxe"
python -m pyisotools game/root B --dest sms-cape-modded.iso
```

## Troubleshooting

### Black screen on boot
- Make sure `sys/main.dol` and `sys/boot.bin` are from the BSE release, not the original game
- Make sure `Kuribo!/System/KuriboKernel.bin` exists in `files/`
- Make sure the ISO is clean NTSC-U (GMSE01), not a patched/modded copy

### Module not loading
- Module filename must start with `_` to load after BSE (e.g. `_CapePowerup.kxe`)
- BSE submodule must be on commit `deca478` (matching the v4.0.0 release)

### Build errors
- Run `cmake` configure step again if you added/removed source files
- BSE submodule needs `--recursive` init for SunshineHeaderInterface
