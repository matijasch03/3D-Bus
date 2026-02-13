#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform float uX;
uniform float uY;
uniform float uS;

void main() {
    vec2 scaled = aPos * uS;
    vec2 positioned = scaled + vec2(uX, uY);
    gl_Position = vec4(positioned, 0.0, 1.0);
    TexCoord = aTexCoord;
}
