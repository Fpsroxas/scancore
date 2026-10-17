# Changelog

Downloads for the current version are listed on the [releases page](https://github.com/medium-gh/scancore/releases).

## Unreleased

- **Anime plays on Windows, macOS and Linux again.** Episodes are H.264 video, which the browser built into the desktop downloads could not decode, so the player only ever showed a spinner (Android was unaffected). Windows now plays through Microsoft Edge WebView2 and macOS through the system's WebKit. The Flatpak gains the player, using Flathub's Qt WebEngine, which has the codec, and the AppImage is built with a Qt WebEngine that has it too. Where a build still cannot play a video, the player says so and offers to open the episode in your browser.
- The player has a button to open the episode in your browser. On Windows and macOS the episode list sits beside the video, and in full screen the controls come back when the pointer reaches the top edge.
- **Sync is lighter.** While you watch or read, changes are sent to scancore.onl at most once a minute instead of every few seconds, and saves the website does not recognise are no longer offered again at every sync.
- Series you remove from your library are removed on scancore.onl too, instead of coming back at the next sync.
- A sync started from Settings can no longer stop halfway and leave "Syncing…" showing until a restart.
- Settings → Account no longer lists what syncs.
- Phones: page titles keep their full width (their buttons move below them), download progress shows its text again, and the anime schedule's day picker, search field and long titles fit the screen. Dialogs and reader messages fit narrow screens.
- Anime Browse shows a Load more button when the first results do not fill the page.
- The mouse wheel scrolls the page again with the pointer over a row of covers (Home, Anime, Novels and more); the row kept the wheel for itself, most noticeably on Windows. Shift with the wheel, or a sideways wheel, still scrolls the row.
- Comic pages from sites that protect their images (Webtoon among them) could stay black when a chapter opened, the first page in page mode most of all: the page was requested before the reader knew which site it came from. They load now, and a page that fails in page mode says so and offers Retry, as the vertical strip does.
- The Downloads page no longer measures the whole downloads folder on every page that downloads, which made it stutter during large downloads.

## 1.1.0 — 2026-10-17

- **Back works again.** Esc, the close arrow and Android Back now leave the comic reader, novel reader, anime player and browser checks; they did nothing before. Esc also goes back from details pages, and the back button on mice works.
- **Comic reader controls stay out of the way.** They no longer reappear on their own while you read or scroll; move the mouse (desktop) or tap (phone) to bring them back. On phones the readers now fill the screen behind the status bar, and the comic reader's top bar sits at the very top.
- **Anime progress syncs.** With a scancore.onl account, episodes you watch and where you stopped now sync between your devices and the website.
- **New Settings.** Settings is organised into categories with picture choices for the theme and reader layout, a live preview for novel text, and its own phone layout. Panel gap, progress rail, line spacing, column width and justified text are now adjustable there.
- **Faster to pick up where you left off.** Comics and novels you have opened before show their chapters immediately, and Continue starts reading without waiting for the site. A request that fails because the connection blinked is retried once automatically.
- Android: Back on Home now leaves the app (it stays running in the background), and a tap shows or hides the reader controls.
- Buttons and chips on the accent colour have readable text in the light theme and with coloured accents.
- Phones: source pickers scroll sideways instead of wrapping, the library filter no longer runs off screen, and reader titles show.
- Leaving the phone player in landscape no longer leaves the app in full screen (thanks to @Doomguy-BFG9000).
- Building from source now requires Qt 6.9.

## 1.0.0 — 2026-10-16

- First release in the reset v1 series. This release retains the current SCANCORE code, including the desktop and phone interface improvements.
- Read comics and novels, browse and watch anime, keep a local library and reading history, and download chapters for offline use.
- Linux packaging supports Flatpak-managed updates and can build without the optional downloadable Python novel-source pack.
- Android's numeric build code remains above previously shipped builds so existing Android installations can upgrade despite the public version reset.
