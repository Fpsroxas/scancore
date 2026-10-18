pragma Singleton
import QtQuick
import ScanCore

// Keyboard shortcuts that can be changed in Settings → Keyboard. Each action
// has default keys; the keybinds setting holds the ones changed, as
// {action: ["Ctrl+K", …]} in Qt's portable notation. Escape and the Left
// and Right arrows (page and chapter turns) are fixed and not listed.
QtObject {
    id: keymap

    // scope: "app" works everywhere; "reader" and "player" only while that
    // is open, so they may share keys with each other but not with "app".
    readonly property var actions: [
        { id: "palette", scope: "app", label: "Search and commands", keys: ["Ctrl+K", "Ctrl+P"] },
        { id: "shortcuts", scope: "app", label: "Show keyboard shortcuts", keys: ["?"] },
        { id: "back", scope: "app", label: "Back", keys: ["Alt+Left"] },
        { id: "home", scope: "app", label: "Go to Home", keys: ["Ctrl+1"] },
        { id: "comics", scope: "app", label: "Go to Comics", keys: ["Ctrl+2"] },
        { id: "novels", scope: "app", label: "Go to Novels", keys: ["Ctrl+3"] },
        { id: "anime", scope: "app", label: "Go to Anime", keys: ["Ctrl+4"] },
        { id: "library", scope: "app", label: "Go to Library", keys: ["Ctrl+5"] },
        { id: "history", scope: "app", label: "Go to History", keys: ["Ctrl+6"] },
        { id: "downloads", scope: "app", label: "Go to Downloads", keys: ["Ctrl+7"] },
        { id: "settings", scope: "app", label: "Go to Settings", keys: ["Ctrl+,"] },
        { id: "sidebar", scope: "app", label: "Collapse or expand the sidebar", keys: ["Ctrl+B"] },
        { id: "fullscreen", scope: "app", label: "Full screen", keys: ["F11"] },
        { id: "reader.next", scope: "reader", label: "Next chapter", keys: ["N"] },
        { id: "reader.prev", scope: "reader", label: "Previous chapter", keys: ["P"] },
        { id: "reader.mode", scope: "reader", label: "Switch between strip and pages", keys: ["M"] },
        { id: "reader.fullscreen", scope: "reader", label: "Full screen", keys: ["F"] },
        { id: "reader.wider", scope: "reader", label: "Wider strip, larger text", keys: ["+", "="] },
        { id: "reader.narrower", scope: "reader", label: "Narrower strip, smaller text", keys: ["-"] },
        { id: "reader.first", scope: "reader", label: "First page", keys: ["Home"] },
        { id: "reader.last", scope: "reader", label: "Last page", keys: ["End"] },
        { id: "reader.down", scope: "reader", label: "Scroll down", keys: ["Down", "J"] },
        { id: "reader.up", scope: "reader", label: "Scroll up", keys: ["Up", "K"] },
        { id: "reader.pageDown", scope: "reader", label: "Scroll down a screen", keys: ["Space", "PgDown"] },
        { id: "reader.pageUp", scope: "reader", label: "Scroll up a screen", keys: ["Shift+Space", "PgUp"] },
        { id: "player.next", scope: "player", label: "Next episode", keys: ["N"] },
        { id: "player.prev", scope: "player", label: "Previous episode", keys: ["P"] },
        { id: "player.fullscreen", scope: "player", label: "Full screen", keys: ["F"] }
    ]
    readonly property var scopes: [
        { scope: "app", title: "Anywhere" },
        { scope: "reader", title: "Comic and novel readers" },
        { scope: "player", title: "Anime player" }
    ]

    readonly property var changed: App.settings.keybinds || ({})

    // Lists stored in settings come back from C++ as list objects that are
    // not JavaScript arrays (Array.isArray says no); this makes them one.
    function listOf(value) {
        return value !== undefined && value !== null && typeof value.length === "number" && typeof value !== "string"
            ? Array.from(value) : null
    }

    function action(id) {
        for (const a of actions)
            if (a.id === id) return a
        return null
    }

    // The keys for an action, changed or default.
    function sequences(id) {
        const own = listOf(changed[id])
        if (own) return own
        const a = action(id)
        return a ? a.keys.slice() : []
    }

    // How they read on this system ("⌘K" on macOS).
    function labels(id) {
        return sequences(id).map(s => Customize.keyText(s))
    }

    function isDefault(id) { return listOf(changed[id]) === null }

    function set(id, keys) {
        const all = Object.assign({}, changed)
        all[id] = keys
        App.setSetting("keybinds", all)
    }

    function reset(id) {
        const all = Object.assign({}, changed)
        delete all[id]
        App.setSetting("keybinds", all)
    }

    function resetAll() { App.setSetting("keybinds", ({})) }

    // Another action that already answers to `key` where `id` would.
    function conflict(id, key) {
        const mine = action(id)
        if (!mine) return null
        for (const a of actions) {
            if (a.id === id) continue
            if (mine.scope !== "app" && a.scope !== "app" && a.scope !== mine.scope) continue
            for (const other of sequences(a.id))
                if (Customize.sameKeys(other, key)) return a
        }
        return null
    }

    // A key event (Keys.onPressed) matched against an action's keys, for
    // keys that only count while one item has focus.
    function matches(id, event) {
        const typed = Customize.keySequence(event.key, event.modifiers)
        return typed !== "" && sequences(id).some(s => Customize.sameKeys(s, typed))
    }

    // A key pressed inside a web page ({key, ctrl, alt, shift, meta} from the
    // DOM), matched against an action's keys.
    function matchesWeb(id, press) {
        const names = { Escape: "Esc", ArrowLeft: "Left", ArrowRight: "Right", ArrowUp: "Up", ArrowDown: "Down",
                        " ": "Space", Enter: "Return", PageUp: "PgUp", PageDown: "PgDown", Delete: "Del" }
        let key = names[press.key] || String(press.key)
        const symbol = key.length === 1 && !/[A-Za-z0-9]/.test(key)
        if (key.length === 1) key = key.toUpperCase()
        const parts = []
        if (press.ctrl) parts.push("Ctrl")
        if (press.alt) parts.push("Alt")
        if (press.shift && !symbol) parts.push("Shift")
        if (press.meta) parts.push("Meta")
        parts.push(key)
        const typed = parts.join("+")
        return sequences(id).some(s => Customize.sameKeys(s, typed))
    }
}
