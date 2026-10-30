# g-larpterm

A minimal GTK3 and VTE-based terminal emulator styled with a TempleOS aesthetic, featuring custom font and pixel-art elements.

## Dependencies

To build `g-larpterm`, you need a C compiler (`gcc` or `clang`), `make`, `pkg-config`, and development files for:
- GTK 3
- VTE 2.91
- Fontconfig

## Installation

### 1. Install dependencies

#### Debian / Ubuntu / Linux Mint
```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-3-dev libvte-2.91-dev libfontconfig-dev
```

#### Arch Linux / Manjaro
```bash
sudo pacman -S --needed base-devel pkgconf gtk3 vte3 fontconfig
```

#### Fedora / RHEL
```bash
sudo dnf install gcc make pkgconf-pkg-config gtk3-devel vte291-devel fontconfig-devel
```

#### openSUSE (Tumbleweed / Leap)
```bash
sudo zypper install gcc make pkg-config gtk3-devel vte-2.91-devel fontconfig-devel
```

#### Gentoo
```bash
sudo emerge --ask dev-build/make dev-util/pkgconf x11-libs/gtk+:3 x11-libs/vte:2.91 media-libs/fontconfig
```

#### Void Linux
```bash
sudo xbps-install -S base-devel pkg-config gtk+3-devel vte3-devel fontconfig-devel
```

#### Solus
```bash
sudo eopkg install -c system.devel gtk3-devel vte-devel fontconfig-devel
```

---

### 2. Build from source

Clone the repository and compile using `make`:

```bash
git clone https://github.com/dimmmbimm/G-larpterm.git
cd G-larpterm
make
```

The compiled binary will be generated inside the `build/` directory.

---

### 3. Running

To run `g-larpterm` directly after building:

```bash
./build/g-larpterm
```

---

### 4. System-wide Installation (Optional)

To install the binary, desktop file, and icons to your system:

```bash
sudo make install
```

To uninstall:

```bash
sudo make uninstall
```

---

## Configuration

Configuration options can be adjusted in `src/config.h` before compiling, or passed via supported runtime flags if defined in your build. Re-run `make` after modifying any headers.
