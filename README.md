<p align="center">
  <img src="resources/branding/scancore.svg" width="128" alt="SCANCORE">
</p>

<h1 align="center">SCANCORE</h1>

<p align="center">
  <b>Comics, novels and anime in one fast, native app.</b><br>
  Read manhwa, manga, webtoons and web novels and watch anime from dozens of sites,<br>
  with your library, history and downloads kept on your own device.
</p>

<p align="center">
  <a href="https://github.com/medium-gh/scancore/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/medium-gh/scancore?label=release&labelColor=111113&color=f2f2f3&style=flat"></a>
  <a href="https://github.com/medium-gh/scancore/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/medium-gh/scancore/total?label=downloads&labelColor=111113&color=f2f2f3&style=flat"></a>
  <a href="https://github.com/medium-gh/scancore/actions/workflows/ci.yml"><img alt="CI" src="https://img.shields.io/github/actions/workflow/status/medium-gh/scancore/ci.yml?branch=main&label=build&labelColor=111113&style=flat"></a>
  <a href="LICENSE"><img alt="MIT licence" src="https://img.shields.io/badge/licence-MIT-f2f2f3?labelColor=111113&style=flat"></a>
  <img alt="Platforms" src="https://img.shields.io/badge/Linux%20·%20Windows%20·%20macOS%20·%20Android-111113?style=flat">
</p>

<p align="center">
  <a href="#download">Download</a> ·
  <a href="#features">Features</a> ·
  <a href="#screenshots">Screenshots</a> ·
  <a href="#community">Community</a> ·
  <a href="#build-from-source">Build from source</a> ·
  <a href="#sources">Sources</a> ·
  <a href="#faq">FAQ</a>
</p>

![SCANCORE home](docs/screenshots/home.webp)

> [!NOTE]
> SCANCORE is free and open source, built by [Medium Dev](https://mediumdev.com). The official downloads are the ones on this repository's [releases page](https://github.com/medium-gh/scancore/releases). The companion website is [scancore.onl](https://scancore.onl), and the community hangs out on [Discord](https://scancore.onl/discord).

## Why SCANCORE

- **Native and fast.** Written in C++20 with Qt Quick, so it renders on the GPU and starts quickly. There is no Electron and no embedded web UI.
- **Works the moment you open it.** The sources are built in. There are no extension repositories to find or add.
- **Three kinds of media, one library.** Comics, novels and anime share the same library, history, downloads and search.
- **Local first.** No account and no server needed. Everything lives in one SQLite file you can back up.
- **Smooth.** Covers fly into their details page, sections and pages glide, readers scroll with easing, and a "Reduce motion" switch turns all of it off.
- **Updates itself.** New versions download in the background and install when you restart, on every platform.

## Features

### Comics
- Search **Asura Scans, Webtoon, MangaDex, Flame Comics, HiveToons, Vortex Scans, Qi Manga, ThunderScans and MangaRead** at once, ranked by how well each title matches.
- **Vertical strip** for webtoons and manhwa, or **page by page** for manga, left-to-right or right-to-left.
- Adjustable strip width (Ctrl + scroll), panel gaps, a progress rail, and very tall strips that stay sharp.
- The next chapter is fetched ahead of time, and your exact place is remembered in every chapter.
- Latest-updates feed across sources. Paste a series link to open it directly.

### Novels
- **Royal Road** discovery shelves (top rated, rising stars, trending, new and completed) plus search across the NovelFull family: NovelFull, AllNovelFull, ReadNovelFull, FreeWebNovel, LibRead and more.
- An optional **extended pack** (desktop) adds about 130 more searchable novel sites through [lightnovel-crawler](https://github.com/dipu-bd/lightnovel-crawler).
- A reader you can tune: **serif, sans or grotesk** type, size, line spacing, column width, justification, and **app, paper, sepia, dark and black** themes.

### Anime
- **AniList**-powered trending, seasonal, popular and top lists, a rotating spotlight, search with genre, year, format and status filters, and a 7-day **airing schedule**.
- Details with episode thumbnails and titles, related entries and recommendations.
- Episodes play inside the app, with **resume**, **auto-next** and **AniSkip** intro, ending and recap skipping.

### Library and everything else
- **Library** with new-chapter checks in the background and desktop notifications.
- **History** grouped by day, **offline downloads** for comic and novel chapters, and a **Ctrl+K command palette**.
- **Optional scancore.onl account**: sign in or sign up from the welcome screen or Settings to bring your website library and comic reading history into the app and send saves back.
- **Browser checks**: if a site asks for one, open it inside SCANCORE once and the app reuses the clearance (desktop).
- Light, dark and black themes, five accent colours, backup and restore, tray icon and keyboard control throughout.
- **Phone layout** on Android: bottom tabs, stacked details pages and touch controls.

## Screenshots

| Welcome | Comic details |
| --- | --- |
| ![Welcome](docs/screenshots/welcome.webp) | ![Comic details](docs/screenshots/comic-details.webp) |
| **Comic reader** | **Novel reader** |
| ![Comic reader](docs/screenshots/comic-reader.webp) | ![Novel reader](docs/screenshots/novel-reader.webp) |
| **Anime** | **Anime details** |
| ![Anime](docs/screenshots/anime.webp) | ![Anime details](docs/screenshots/anime-details.webp) |
| **Novels** | **Account** |
| ![Novels](docs/screenshots/novels.webp) | ![Account](docs/screenshots/welcome-account.webp) |

<p align="center">
  <img src="docs/screenshots/phone-home.webp" width="240" alt="Phone home">
  <img src="docs/screenshots/phone-details.webp" width="240" alt="Phone details">
  <img src="docs/screenshots/phone-reader.webp" width="240" alt="Phone reader">
</p>

## Download

Get the latest version from the **[releases page](https://github.com/medium-gh/scancore/releases/latest)**.

| Platform | File | Notes |
| --- | --- | --- |
| **Linux** (x86_64) | `SCANCORE-<version>-linux-x86_64.AppImage` | Runs on any recent distribution. Updates itself in place. |
| **Linux** (Flatpak) | `SCANCORE-<version>-linux-x86_64.flatpak` | Install with Flatpak; updates are handled by Flatpak. Requires the Flathub runtime. |
| **Windows** 10/11 (x64) | `SCANCORE-<version>-windows-x64-setup.exe` | Installer with Start menu entry. Updates itself. |
| | `SCANCORE-<version>-windows-x64-portable.zip` | No install; unzip and run `scancore.exe`. |
| **macOS** 13+ (Apple Silicon and Intel) | `SCANCORE-<version>-macos-universal.dmg` | Drag to Applications. Updates itself. |
| **Android** 9+ (arm64) | `SCANCORE-<version>-android-arm64.apk` | Sideload; updates open the system installer. |

Community package definitions for [AUR, WinGet, Scoop and Homebrew](docs/package-managers.md) are maintained separately from GitHub release uploads.

<details>
<summary><b>Install notes for each platform</b></summary>

**Linux.** Make the AppImage executable and run it:

```bash
chmod +x SCANCORE-*-linux-x86_64.AppImage && ./SCANCORE-*-linux-x86_64.AppImage
```

To show it in your app launcher (GNOME, KDE, rofi, wofi and others), keep it somewhere permanent and add a desktop entry:

```bash
mkdir -p ~/Applications ~/.local/share/applications ~/.local/share/icons/hicolor/scalable/apps
mv SCANCORE-*-linux-x86_64.AppImage ~/Applications/SCANCORE.AppImage
curl -fsSL https://raw.githubusercontent.com/medium-gh/scancore/main/resources/branding/scancore.svg \
  -o ~/.local/share/icons/hicolor/scalable/apps/com.mediumdev.scancore.svg
printf '[Desktop Entry]\nType=Application\nName=SCANCORE\nExec=%s %%U\nIcon=com.mediumdev.scancore\nCategories=Graphics;Viewer;\n' \
  "$HOME/Applications/SCANCORE.AppImage" > ~/.local/share/applications/com.mediumdev.scancore.desktop
```

AppImages need FUSE 2 (`libfuse2` on Debian and Ubuntu, `fuse2` on Arch).

**Flatpak.** Install Flatpak and configure the Flathub remote for your distribution, then install the downloaded bundle:

```bash
flatpak install --user ./SCANCORE-*-linux-x86_64.flatpak
```

The bundle contains SCANCORE; Flatpak downloads the KDE runtime from Flathub. Future app updates are provided by Flatpak, not the in-app updater.

**Windows.** Run the installer. SmartScreen may warn about an unrecognised app the first time: choose *More info*, then *Run anyway*.

**macOS.** Open the disk image and drag SCANCORE to Applications. The app is not notarised yet, so the first time, right-click it and choose *Open*.

**Android.** Download the APK on your phone and open it. Allow installs from your browser or file manager when asked. Updates are offered inside the app.

</details>

## Sources

SCANCORE reads only what sites show publicly. It does not bypass paywalls, logins or locked chapters, and it does not host any content.

Site addresses, availability and template options live in [`sources/manifest.json`](sources/manifest.json). The app ships with a copy and checks this repository for a newer one, so when a site moves domain or breaks, it can be fixed for everyone **without a new release**. The manifest is data only and never contains code.

| Kind | Built in | Optional |
| --- | --- | --- |
| Comics | Asura Scans, Webtoon, MangaDex, Flame Comics, HiveToons, Vortex Scans, Qi Manga, ThunderScans, MangaRead | — |
| Novels | Royal Road, NovelFull, NovelFull.com, AllNovelFull, ReadNovelFull, FreeWebNovel, LibRead | About 130 more sites via the extended pack (desktop) |
| Anime | AniList metadata, ani.zip episode details, AniSkip, ReAnime player | — |

To check every built-in source against the live sites:

```bash
./build/debug/tests/scancore_probe comics "solo leveling"
./build/debug/tests/scancore_probe novels "lord of the mysteries"
```

## Build from source

### Desktop (Linux, Windows, macOS)

Requirements: CMake 3.28+, Ninja, a C++20 compiler (GCC 13+, Clang 17+ or MSVC 2022), and **Qt 6.8 or newer** with Qt Quick, Qt Quick Controls, Qt SVG, Qt Image Formats and **Qt WebEngine** (for the anime player; optional). [lexbor](https://github.com/lexbor/lexbor), the HTML parser, is downloaded automatically.

```bash
git clone https://github.com/medium-gh/scancore.git && cd scancore
cmake --preset debug          # or: cmake --preset release
cmake --build --preset debug
ctest --preset debug
./build/debug/bin/scancore
```

Installing the dependencies:

| System | Command |
| --- | --- |
| Arch Linux | `sudo pacman -S cmake ninja qt6-base qt6-declarative qt6-svg qt6-imageformats qt6-webengine` |
| Fedora | `sudo dnf install cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qtimageformats qt6-qtwebengine-devel` |
| Ubuntu 24.04+ | `sudo apt install cmake ninja-build qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-image-formats-plugins qt6-webengine-dev` |
| macOS | `brew install cmake ninja qt` |
| Windows | Install Qt 6.8+ for MSVC 2022 with the [Qt Online Installer](https://www.qt.io/download-qt-installer) (tick Qt WebEngine and Qt Image Formats), then build from a *Developer PowerShell for VS 2022* with `-DCMAKE_PREFIX_PATH=C:\Qt\6.x.y\msvc2022_64`. |

Build options: `-DSCANCORE_WITH_WEBENGINE=OFF` builds without the embedded browser. The anime player and browser checks are then unavailable, and everything else works.

### Android

Requirements: the desktop Qt above (as the host), **Qt for Android** (same version, `android_arm64_v8a`, with Qt WebView and Qt Image Formats), the Android SDK, **NDK r27c** (`27.2.12479018`) and JDK 17.

```bash
export ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/27.2.12479018
~/Qt/6.11.2/android_arm64_v8a/bin/qt-cmake -S . -B build/android -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH=/usr -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_NDK_ROOT=$ANDROID_NDK_ROOT
cmake --build build/android --target apk
adb install build/android/src/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
```

`QT_HOST_PATH` is your desktop Qt prefix: `/usr` on Arch, or something like `~/Qt/6.11.2/gcc_64` from the Qt installer. Use `-DCMAKE_BUILD_TYPE=Debug` for an APK that installs without signing. OpenSSL for Android is fetched automatically.

### Releases

Pushing a tag such as `v1.2.0` runs [the release workflow](.github/workflows/release.yml). It builds the Linux AppImage, the Windows installer and portable zip, the macOS universal disk image and a **signed** Android APK, then publishes them as a GitHub release. Installed copies pick the release up through their updater.

## How it works

```
src/core           HTTP (per-site limits, cancellation), HTML parsing, SQLite, image cache, background tasks
src/sources        comic and novel adapters, the lightnovel-crawler bridge, manifest loading
src/app            services used by the interface: Comics, Novels, Anime, Library, Downloads,
                   Account, Updater, Extensions and App
src/qml            the interface: theme and motion tokens, components, pages, phone layout
sources/           the source manifest
tests/             offline parser tests and the live source probe
packaging/         Linux desktop entry, Windows installer, macOS and Android icons and manifest
```

- Every network request runs on a worker pool with per-site and overall limits. Leaving a page cancels its requests.
- Chapter pages are decoded off the interface thread and drawn as GPU tiles, so long webtoon strips stay sharp.
- The library, progress, history, downloads, preferences and a metadata cache share one SQLite database (`~/.local/share/Medium Dev/ScanCore` on Linux).

## Community

Join the [SCANCORE Discord](https://scancore.onl/discord) for questions, feedback and community updates. Report bugs and request features in [GitHub Issues](https://github.com/medium-gh/scancore/issues); code contributions are welcome through pull requests.

## Contributing

Issues and pull requests are welcome. [CONTRIBUTING.md](CONTRIBUTING.md) has the build, test and source-adapter details. Questions and ideas go in [Discussions](https://github.com/medium-gh/scancore/discussions), bug reports in [Issues](https://github.com/medium-gh/scancore/issues), and chat on [Discord](https://scancore.onl/discord). Release notes are in [CHANGELOG.md](CHANGELOG.md).

- **A site moved or broke?** Most fixes are a one-line change to [`sources/manifest.json`](sources/manifest.json): the domain, `enabled` or a template option. Bump `version` so installed apps pick it up.
- **New site on a known template** (WordPress MangaReader, Madara, Iken, the NovelFull family)? Add a manifest entry with the matching `engine`.
- **New kind of site?** Add an engine in `src/sources/comics` or `src/sources/novels`, register it in `createSource`, and add an offline parser test in `tests/`.
- Run `ctest` and the live probe before opening a pull request.

## FAQ

<details>
<summary><b>Do I need an account?</b></summary>

No. Everything works without one. A [scancore.onl](https://scancore.onl) account only adds syncing with the website.
</details>

<details>
<summary><b>Where is my data?</b></summary>

On your device, in the app data folder (`~/.local/share/Medium Dev/ScanCore` on Linux, `%APPDATA%\Medium Dev\ScanCore` on Windows, `~/Library/Application Support/Medium Dev/ScanCore` on macOS). Settings → Backup exports it all to one file.
</details>

<details>
<summary><b>A source shows "asked for a browser check".</b></summary>

On desktop, use the **Open browser check** button (or the globe next to the source in Settings → Sources), complete the site's check and press **Done**. SCANCORE then reuses the clearance.
</details>

<details>
<summary><b>Why do some anime episodes not play?</b></summary>

Episodes come from the ReAnime player. Brand-new or obscure episodes may not be there yet. The player shows a message rather than a broken video.
</details>

<details>
<summary><b>How do I turn off automatic updates?</b></summary>

Settings → About → *Install updates automatically*. You can also turn off update checks entirely.
</details>

## Licence

SCANCORE is released under the [MIT licence](LICENSE).

Fonts: DM Sans, Space Grotesk and Literata (SIL Open Font Licence). Icons: [Lucide](https://lucide.dev) (ISC). HTML parsing: [lexbor](https://github.com/lexbor/lexbor) (Apache 2.0). Optional novel pack: [lightnovel-crawler](https://github.com/dipu-bd/lightnovel-crawler) (GPL-3.0, downloaded separately at your request).

## Disclaimer

SCANCORE does not host any content. It is a reader for websites that are publicly available, and its developers are not affiliated with them. Support the creators and official releases of the series you enjoy.
