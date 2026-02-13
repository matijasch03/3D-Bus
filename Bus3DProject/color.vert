#version 330 core
layout (location = 0) in vec2 aPos;

uniform vec2 uPosOffset;

void main() {
    gl_Position = vec4(aPos + uPosOffset, 0.0, 1.0);
}
