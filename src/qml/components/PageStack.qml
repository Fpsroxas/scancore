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

    // The turnstile's doors: while a page turns it is drawn as one picture
    // (ShaderEffectSource) turned by a shader whose angle a UniformAnimator
    // drives on the render thread. The turn stays smooth while the page it
    // opens builds itself on the interface thread, and the page is one quad
    // in 3D instead of every node in it, which keeps the GPU's batching.
    property Item leaving: null
    property Item showing: null
    onCurrentItemChanged: {
        leaving = showing
        showing = currentItem
        // The doors take the pages at once: StackView shows the new page in
        // this very frame, and a transition's first step only runs at the
        // next tick, which flashed the page flat before it turned. Without
        // a transition (the first page, an immediate push) they let go.
        if (!turnstile || !leaving || leaving === showing) return
        hinge(inDoor, showing, 0, false)
        hinge(outDoor, leaving, 0, true)
        Qt.callLater(() => { if (!stack.busy) stack.unhinge() })
    }
    component Door: ShaderEffect {
        id: door
        property Item page: null
        property real angle: 0
        readonly property real hingeX: 0
        readonly property real hingeY: height / 2
        readonly property var source: ShaderEffectSource {
            sourceItem: door.page
            hideSource: true
            live: true
        }
        x: page ? page.x : 0
        y: page ? page.y : 0
        width: page ? page.width : 0
        height: page ? page.height : 0
        z: 1
        visible: false
        vertexShader: "qrc:/shaders/turnstile.vert.qsb"
        fragmentShader: "qrc:/shaders/turnstile.frag.qsb"
    }
    Door { id: inDoor }
    Door { id: outDoor }
    function hinge(door, page, angle, shown) {
        door.page = page
        door.angle = angle
        door.visible = shown && page !== null
    }
    function unhinge() {
        for (const door of [inDoor, outDoor]) {
            door.visible = false
            door.page = null
            door.angle = 0
        }
    }

    // Forward: the old page swings away into the screen, the new one comes
    // in from the other side. Back: the other way round.
    Transition {
        id: turnForwardIn
        SequentialAnimation {
            ScriptAction { script: stack.hinge(inDoor, stack.showing, -80, false) }
            PauseAnimation { duration: Motion.fast }
            ScriptAction { script: inDoor.visible = inDoor.page !== null }
            UniformAnimator { target: inDoor; uniform: "angle"; from: -80; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
            ScriptAction { script: stack.unhinge() }
        }
    }
    Transition {
        id: turnForwardOut
        SequentialAnimation {
            ScriptAction { script: stack.hinge(outDoor, stack.leaving !== stack.showing ? stack.leaving : null, 0, true) }
            UniformAnimator { target: outDoor; uniform: "angle"; from: 0; to: 60; duration: Motion.fast; easing.type: Easing.InCubic }
            ScriptAction { script: outDoor.visible = false }
        }
    }
    Transition {
        id: turnBackIn
        SequentialAnimation {
            ScriptAction { script: stack.hinge(inDoor, stack.showing, 60, false) }
            PauseAnimation { duration: Motion.fast }
            ScriptAction { script: inDoor.visible = inDoor.page !== null }
            UniformAnimator { target: inDoor; uniform: "angle"; from: 60; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
            ScriptAction { script: stack.unhinge() }
        }
    }
    Transition {
        id: turnBackOut
        SequentialAnimation {
            ScriptAction { script: stack.hinge(outDoor, stack.leaving !== stack.showing ? stack.leaving : null, 0, true) }
            UniformAnimator { target: outDoor; uniform: "angle"; from: 0; to: -80; duration: Motion.fast; easing.type: Easing.InCubic }
            ScriptAction { script: outDoor.visible = false }
        }
    }

    // NumberAnimations, not Animators: StackView applies the push
    // transition's starting values to the first page too (pushed with
    // StackView.Immediate), and an Animator never puts its end value back
    // when it does not run, so the page stayed at opacity 0 and every
    // section showed blank (2.12.1).
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
