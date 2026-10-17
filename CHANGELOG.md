# Changelog

Downloads for the current version are listed on the [releases page](https://github.com/medium-gh/scancore/releases).

## Unreleased

- **Back works again.** Esc, the close arrow and Android Back now leave the comic reader, novel reader, anime player and browser checks; they did nothing before. Esc also goes back from details pages, and the back button on mice works.
- **Comic reader controls stay out of the way.** They no longer reappear on their own while you read or scroll; move the mouse (desktop) or tap (phone) to bring them back. On phones the readers now fill the screen behind the status bar, and the comic reader's top bar sits at the very top.
- **Anime progress syncs.** With a scancore.onl account, episodes you watch and where you stopped now sync between your devices and the website.
- **New Settings.** Settings is organised into categories with picture choices for the theme and reader layout, a live preview for novel text, and its own phone layout. Panel gap, progress rail, line spacing, column width and justified text are now adjustable there.
- Buttons and chips on the accent colour have readable text in the light theme and with coloured accents.
- Phones: source pickers scroll sideways instead of wrapping, the library filter no longer runs off screen, and reader titles show.
- Leaving the phone player in landscape no longer leaves the app in full screen (thanks to @Doomguy-BFG9000).
- Building from source now requires Qt 6.9.

## 1.0.0 — 2026-10-16

- First release in the reset v1 series. This release retains the current SCANCORE code, including the desktop and phone interface improvements.
- Read comics and novels, browse and watch anime, keep a local library and reading history, and download chapters for offline use.
- Linux packaging supports Flatpak-managed updates and can build without the optional downloadable Python novel-source pack.
- Android's numeric build code remains above previously shipped builds so existing Android installations can upgrade despite the public version reset.
