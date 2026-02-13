#include "Util.h"
#include <iostream>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Moraš imati ovaj fajl u folderu projekta!

std::string readFile(const char* filePath) {
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int createShader(const char* vsSource, const char* fsSource) {
    unsigned int program = glCreateProgram();
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vs, 1, &vsSource, NULL);
    glCompileShader(vs);

    glShaderSource(fs, 1, &fsSource, NULL);
    glCompileShader(fs);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

unsigned int loadImageToTexture(const char* filePath) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Podešavanja teksture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath, &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Neuspešno učitavanje teksture: " << filePath << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

GLFWcursor* loadImageToCursor(const char* filePath) {
    int width, height, channels;
    unsigned char* pixels = stbi_load(filePath, &width, &height, &channels, 4);
    if (!pixels) return NULL;

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;

    GLFWcursor* cursor = glfwCreateCursor(&image, 0, 0);
    stbi_image_free(pixels);
    return cursor;
}