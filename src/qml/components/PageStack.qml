import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Per-section navigation stack. Pages slide and fade with a slight depth
// shift; the page underneath recedes rather than vanishing. In the Retro
// mode pages turn like a door about the screen's left edge instead
// (Windows Phone's turnstile): the old one swings away, the new one in.
StackView {
    id: stack
    property string initial

    Component.onCompleted: if (initial) push(Qt.resolvedUrl("../" + initial), {}, StackView.Immediate)

    // The turnstile's doors. A transition only animates its item's own
    // properties, so the pages get these as transforms while it runs.
    property Item leaving: null
    property Item showing: null
    onCurrentItemChanged: {
        leaving = showing
        showing = currentItem
    }
    Rotation { id: inTurn; origin.x: 0; origin.y: stack.height / 2; axis { x: 0; y: 1; z: 0 } }
    Rotation { id: outTurn; origin.x: 0; origin.y: stack.height / 2; axis { x: 0; y: 1; z: 0 } }
    function hinge() {
        if (stack.showing) stack.showing.transform = [inTurn]
        if (stack.leaving && stack.leaving !== stack.showing) stack.leaving.transform = [outTurn]
    }
    function unhinge() {
        for (const item of [stack.showing, stack.leaving])
            if (item) item.transform = []
        inTurn.angle = 0
        outTurn.angle = 0
    }

    // Forward: the old page swings away into the screen, the new one comes
    // in from the other side. Back: the other way round.
    Transition {
        id: turnForwardIn
        SequentialAnimation {
            ScriptAction { script: { stack.hinge(); inTurn.angle = -80 } }
            PropertyAction { property: "opacity"; value: 0 }
            PauseAnimation { duration: Motion.fast }
            PropertyAction { property: "opacity"; value: 1 }
            NumberAnimation { target: inTurn; property: "angle"; from: -80; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
            ScriptAction { script: stack.unhinge() }
        }
    }
    Transition {
        id: turnForwardOut
        SequentialAnimation {
            NumberAnimation { target: outTurn; property: "angle"; from: 0; to: 60; duration: Motion.fast; easing.type: Easing.InCubic }
            PropertyAction { property: "opacity"; value: 0 }
        }
    }
    Transition {
        id: turnBackIn
        SequentialAnimation {
            ScriptAction { script: { stack.hinge(); inTurn.angle = 60 } }
            PropertyAction { property: "opacity"; value: 0 }
            PauseAnimation { duration: Motion.fast }
            PropertyAction { property: "opacity"; value: 1 }
            NumberAnimation { target: inTurn; property: "angle"; from: 60; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
            ScriptAction { script: stack.unhinge() }
        }
    }
    Transition {
        id: turnBackOut
        SequentialAnimation {
            NumberAnimation { target: outTurn; property: "angle"; from: 0; to: -80; duration: Motion.fast; easing.type: Easing.InCubic }
            PropertyAction { property: "opacity"; value: 0 }
        }
    }

    Transition {
        id: slideIn
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 48; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }
    Transition {
        id: slideOut
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 0; to: -28; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }
    Transition {
        id: slideBackIn
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: -28; to: 0; duration: Motion.page; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }
    }
    Transition {
        id: slideBackOut
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "x"; from: 0; to: 48; duration: Motion.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.accelerate }
        }
    }

    readonly property bool turnstile: Theme.retro && !Motion.reduced
    pushEnter: turnstile ? turnForwardIn : slideIn
    pushExit: turnstile ? turnForwardOut : slideOut
    popEnter: turnstile ? turnBackIn : slideBackIn
    popExit: turnstile ? turnBackOut : slideBackOut
    replaceEnter: pushEnter
    replaceExit: pushExit
}
