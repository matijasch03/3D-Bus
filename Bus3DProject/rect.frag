#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uTex0;

void main() {
    FragColor = texture(uTex0, TexCoord);
}
