# Changelog

Downloads for the current version are listed on the [releases page](https://github.com/medium-gh/scancore/releases).

## Unreleased

- **Fixed parts of SCANCORE working on separate copies of the library, account, downloads and updater**, a mix-up present since 1.0. Automatic update checks now show the update banner (only a check from Settings → About did before), saving or removing a series starts a sync and removals reach scancore.onl, signed-in users no longer sync twice at startup, and changes in Settings, such as how often to check for new chapters or app updates, apply without a restart.
- Opening SCANCORE while it is already running, for example in the tray, brings its window back instead of starting a second copy on the same library.
- Reading keys are yours to change as well: scrolling by a line or a screen (↓ ↑, J K, Space, Page Up/Down) is in Settings → Keyboard, and the novel reader now follows all your reader shortcuts; it ignored changed keys before.
- The novel reader goes back to where you stopped in a chapter that was loaded ahead (it started at the top), counts a chapter that fits on one screen as read, and its menus and messages fit narrow phones.
- The Ctrl+K palette groups its results under Search, Recent, Pages and Commands, and pointing at or clicking a result works again.
- The anime player remembers the server you picked and starts the next episode on it.
- Release pages list what changed and which file to download for each system.

## 1.2.0 — 2026-10-18

- **Your own theme.** Pick the window colour on a colour wheel, with a brightness slider that decides how dark it is, and the whole app follows it. Add a picture or animated GIF behind everything, fitted, dimmed and blurred to taste. Panels can be see-through, and so can the whole window, showing your desktop through it. Accents can be any colour too, and indigo, emerald and amber join the presets.
- **Share themes** as .sct files: one file with the colours, transparency and background picture inside it, which works on every system. Import from Settings → Theme, or drop the file on the window.
- **Settings shows one category at a time** on desktop, with a search box over the list of categories, instead of one long page to scroll through.
- **Navigation your way**: a full sidebar, a floating one, an icon rail or a dock, on the left or right (the dock on the top or bottom).
- **Change any shortcut** in Settings → Keyboard: click it and press the new keys. A key taken from another action moves over, and each one can go back to its default. The shortcuts sheet (**?**) shows yours.
- Settings → Interface adds text size, cover size (fit more covers on screen, or fewer and larger), animation speed (relaxed, default or snappy) and the page SCANCORE opens on.
- **Pick up where you left off.** Home leads with the last thing you read or watched, with its progress and a Continue button.
- **Library sorting**: new chapters first, recently read, recently added, or by title. History has a search box.
- **Recent searches** on the Comics and Novels pages, one click to search again.
- **Quick commands** in the Ctrl+K palette: switch theme, reduce motion, check for new chapters, sync, and a sheet of every keyboard shortcut (also on **?**).
- Comic reader: in page mode, swipe or drag sideways to turn the page.
- Anime: endings can be skipped automatically too, and the next-episode countdown can be 3, 8 or 15 seconds. With intros skipped automatically, the Skip button now still appears for endings, which were otherwise impossible to skip.
- Livelier details: notifications show an icon and the time left, pause while you point at them and can be swiped away; the anime spotlight shows when the next title comes, pauses on hover and can be swiped; shelves fade at the edge that scrolls; long pages have a back-to-top button; dialogs spring in.
- Lists no longer blink or replay their entrance when the library changes in the background, for example after a sync or while chapters download, and the Downloads list keeps its place during downloads.
- Signing out asks first.
- The sidebar scrolls instead of overlapping its Settings button in short windows.
- The logo on Settings → About is no longer stretched.
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
