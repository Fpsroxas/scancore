// Retro's page turn (components/PageStack.qml): a page drawn as one picture
// turned about a vertical hinge, with the depth Qt Quick's Rotation gives
// (a plane 1024 px away). Animated by a UniformAnimator, so it turns on the
// render thread while the page it opens is still loading.
// Rebuild turnstile.vert.qsb with resources/shaders/build.sh after changing it.
#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float angle;   // degrees
    float hingeX;  // the hinge and the eye's height, in the item's pixels
    float hingeY;
};

void main() {
    qt_TexCoord0 = qt_MultiTexCoord0;
    float a = radians(angle);
    float dx = qt_Vertex.x - hingeX;
    float dy = qt_Vertex.y - hingeY;
    // Qt's projected rotation: x' = x cos a over w = 1 - x sin a / 1024. Kept
    // as w for the GPU, so the picture is perspective-correct, and never
    // behind the eye (wide windows turned far).
    float w = max(0.1, 1.0 - dx * sin(a) / 1024.0);
    gl_Position = qt_Matrix * vec4(hingeX * w + dx * cos(a), hingeY * w + dy, 0.0, w);
}
