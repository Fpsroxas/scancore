pragma Singleton
import QtQuick
import ScanCore

// Colour, type and shape tokens. The default palette follows the ScanCore
// website: neutral greys with white (dark) or near-black (light) accents.
QtObject {
    id: theme

    readonly property string mode: App.settings.theme
    readonly property bool dark: mode === "dark" || mode === "black"
        || (mode === "system" && Qt.styleHints.colorScheme !== Qt.ColorScheme.Light)
    readonly property bool black: mode === "black"

    // Base colours per theme. Settings draws its theme previews from these.
    readonly property var palettes: ({
        light: { bg: "#f7f7f8", sidebar: "#efeff1", surface: "#ffffff", surface2: "#f1f1f3", surface3: "#e6e6e9",
                 border: "#e2e2e6", borderStrong: "#cfcfd5", text: "#141416", textMuted: "#5f5f68", textFaint: "#94949c" },
        dark: { bg: "#0c0c0d", sidebar: "#101012", surface: "#151517", surface2: "#1c1c1f", surface3: "#26262a",
                border: "#25252a", borderStrong: "#36363c", text: "#f2f2f3", textMuted: "#a3a3ab", textFaint: "#6c6c75" },
        black: { bg: "#000000", sidebar: "#050505", surface: "#0b0b0b", surface2: "#141414", surface3: "#1d1d1d",
                 border: "#25252a", borderStrong: "#36363c", text: "#f2f2f3", textMuted: "#a3a3ab", textFaint: "#6c6c75" }
    })
    readonly property var palette: palettes[black ? "black" : dark ? "dark" : "light"]

    readonly property color bg: palette.bg
    readonly property color sidebar: palette.sidebar
    readonly property color surface: palette.surface
    readonly property color surface2: palette.surface2
    readonly property color surface3: palette.surface3
    readonly property color border: palette.border
    readonly property color borderStrong: palette.borderStrong
    readonly property color text: palette.text
    readonly property color textMuted: palette.textMuted
    readonly property color textFaint: palette.textFaint
    readonly property color overlay: dark ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(0.1, 0.1, 0.12, 0.35)
    readonly property color danger: dark ? "#f87171" : "#dc2626"
    readonly property color success: dark ? "#4ade80" : "#16a34a"
    readonly property color warning: dark ? "#fbbf24" : "#d97706"

    // Accent options; "mono" matches the website.
    readonly property var accents: ({
        mono: { dark: "#f2f2f3", light: "#141416", on: { dark: "#0c0c0d", light: "#ffffff" } },
        violet: { dark: "#a78bfa", light: "#7c3aed", on: { dark: "#120a24", light: "#ffffff" } },
        ember: { dark: "#fb923c", light: "#ea580c", on: { dark: "#1f0d02", light: "#ffffff" } },
        sky: { dark: "#38bdf8", light: "#0284c7", on: { dark: "#02131d", light: "#ffffff" } },
        rose: { dark: "#fb7185", light: "#e11d48", on: { dark: "#22050b", light: "#ffffff" } }
    })
    readonly property var accentSet: accents[App.settings.accent] || accents.mono
    readonly property color accent: dark ? accentSet.dark : accentSet.light
    // Text and icons on the accent colour. (A name starting with "on" would be
    // read as a signal handler and silently never bound.)
    readonly property color accentInk: dark ? accentSet.on.dark : accentSet.on.light

    // Novel reader page colours; "app" follows the app theme.
    readonly property var readingThemes: ({
        app: { label: "App", bg: bg, text: text, muted: textMuted, chrome: surface },
        paper: { label: "Paper", bg: "#f7f4ec", text: "#2a2925", muted: "#79766d", chrome: "#efebe1" },
        sepia: { label: "Sepia", bg: "#efe3c8", text: "#4a3826", muted: "#8a7457", chrome: "#e6d8b9" },
        dark: { label: "Dark", bg: "#17171a", text: "#d9d9de", muted: "#8b8b93", chrome: "#1f1f23" },
        black: { label: "Black", bg: "#000000", text: "#c9c9ce", muted: "#77777e", chrome: "#0d0d0f" }
    })

    readonly property string fontSans: "DM Sans"
    readonly property string fontDisplay: "Space Grotesk"
    readonly property string fontSerif: "Literata"

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 20

    readonly property int textXs: 11
    readonly property int textSm: 13
    readonly property int textMd: 14
    readonly property int textLg: 16
    readonly property int textXl: 20
    readonly property int text2xl: compact ? 22 : 26
    readonly property int text3xl: compact ? 26 : 34

    // Set by Main.qml: true on phones and very narrow windows.
    property bool compact: false
    readonly property bool touch: App.platform === "android"
    readonly property int gutter: compact ? 16 : 32

    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    function mix(a, b, t) {
        return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1)
    }
}
