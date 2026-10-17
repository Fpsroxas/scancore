# Third-party package managers

These are draft v1.0.0 definitions for AUR, WinGet, Scoop and Homebrew. The release files and SHA-256 values are pinned to the existing [v1.0.0 GitHub release](https://github.com/medium-gh/scancore/releases/tag/v1.0.0). The manifests live in this repository so they can be reviewed and updated with each release; they have not been submitted to any package index.

## Arch Linux AUR

Draft files: [`packaging/aur/PKGBUILD`](../packaging/aur/PKGBUILD) and the matching [`.SRCINFO`](../packaging/aur/.SRCINFO).

Before you publish it:

1. Replace `YOUR NAME <your-email@example.com>` in the PKGBUILD maintainer comment with your real maintainer name and a working email address.
2. On Arch Linux, check and build the package from `packaging/aur`:

   ```bash
   cd packaging/aur
   makepkg --printsrcinfo > .SRCINFO
   makepkg -si
   ```

   `makepkg -si` compiles from the tagged source and installs it locally. The package turns off the optional novel-source pack installer, and Qt WebEngine is an optional dependency; without it, the embedded anime player and browser checks are unavailable.
3. If the local package works, create an empty package named `scancore` at [aur.archlinux.org](https://aur.archlinux.org/), clone its Git URL, copy in `PKGBUILD`, regenerate `.SRCINFO` with `makepkg --printsrcinfo > .SRCINFO`, then commit and push both files.
4. For future releases, update `pkgver`, both source checksums as applicable, regenerate `.SRCINFO`, build/test the package, and push the update to the AUR Git repository.

The AUR package builds from source and downloads Lexbor v2.7.0 as a separate, hash-pinned source archive. The CMake FetchContent override keeps the build from trying to clone Lexbor again.

## WinGet

The three-file v1.0.0 manifest draft is in [`packaging/winget/MediumDev.ScanCore/1.0.0`](../packaging/winget/MediumDev.ScanCore/1.0.0). It uses the x64 Inno Setup installer and its silent switches from [`packaging/windows/scancore.iss`](../packaging/windows/scancore.iss).

1. On Windows, install Microsoft's `wingetcreate` tool using the [WinGet Create instructions](https://learn.microsoft.com/windows/package-manager/package/manifest).
2. Validate the draft from PowerShell:

   ```powershell
   winget validate .\packaging\winget\MediumDev.ScanCore\1.0.0
   ```

3. Install it locally from that manifest directory to verify the installer, Start-menu shortcut and uninstall behavior:

   ```powershell
   winget install --manifest .\packaging\winget\MediumDev.ScanCore\1.0.0
   ```

4. Submit the three YAML files through `wingetcreate submit` or the Microsoft `winget-pkgs` repository's normal pull-request process. Check Microsoft's current submission instructions before submitting.
5. For each release, add a version directory with the new release URL and SHA-256, then submit the update. Do not reuse the v1.0.0 hash.

The identifier in the draft is `MediumDev.ScanCore`. Keep it stable after WinGet accepts the package. The draft has not yet been accepted by WinGet, and the package id may need adjustment if Microsoft reports a naming conflict.

## Scoop

The draft is [`bucket/scancore.json`](../bucket/scancore.json). Scoop buckets are Git repositories containing a `bucket` directory.

1. Create a public GitHub repository named `scoop-scancore` under the `medium-gh` account.
2. Put `scancore.json` in that repository's `bucket/` directory, commit it, and push it.
3. Install from the bucket:

   ```powershell
   scoop bucket add scancore https://github.com/medium-gh/scoop-scancore
   scoop install scancore
   ```

4. When SCANCORE releases a new version, update `version`, `url`, and `hash` in the manifest. Its `checkver` and `autoupdate` fields provide the release pattern, but review the downloaded artifact and checksum before committing an update.

This uses the portable ZIP, so it does not need administrator access. The manifest creates a Start-menu shortcut. The app's own updater is not Scoop-aware; update managed installs with `scoop update scancore` instead.

## Homebrew Cask

The draft is [`packaging/homebrew/Casks/scancore.rb`](../packaging/homebrew/Casks/scancore.rb).

1. Create a public repository named `homebrew-scancore` under `medium-gh`.
2. Copy the draft to `Casks/scancore.rb` in that repository, commit and push it.
3. On a Mac running macOS 13 or newer, install from the tap:

   ```bash
   brew tap medium-gh/scancore
   brew install --cask scancore
   ```

4. For future releases, update the cask `version` and matching DMG `sha256`, then commit and push the change. Verify the new DMG checksum before publishing.

The current macOS release is not notarized. Homebrew does not remove macOS Gatekeeper's first-open warning; users may need to approve the app in macOS settings. For a smoother public release, sign the app with a Developer ID and notarize it before broad Homebrew promotion. The app also has an in-app updater; Homebrew users should use `brew upgrade --cask scancore` to keep the package manager's record current.

## Release maintainer checklist

For every new version, publish the GitHub release assets first, then update each package definition with the exact release URL, version and verified checksum. Test the definition through its package manager before submitting it. A release on GitHub does not automatically update these external package indexes.
