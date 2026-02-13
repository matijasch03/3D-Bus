#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include "Util.h"

#define M_PI 3.14159265358979323846

// --- POSTAVKE ---
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;
const unsigned int FBO_WIDTH = 800;
const unsigned int FBO_HEIGHT = 600;

// Kamera (Pozicija vozača)
glm::vec3 cameraPos = glm::vec3(0.0f, 1.5f, 0.0f); // Vozač sedi na visini 1.5
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// Miš/Pogled
bool firstMouse = true;
float yaw = -90.0f; // Gleda pravo napred ka šoferšajbni
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0;
float lastY = SCR_HEIGHT / 2.0;

// --- 2D BUS SIMULATION VARIABLES ---
unsigned busTexture;
unsigned stationTexture;
unsigned closedIconTexture;
unsigned openIconTexture;
unsigned controlIconTexture;
unsigned int colorShader2D;
unsigned int rectShader2D;

const float BUS_SCALE = 0.25f;
const float STATION_SCALE = 0.15f;
const float TRAVEL_TIME_SECONDS = 5.0f;
const float STATION_WAIT_SECONDS = 10.0f;

int currentStationIndex = 0;
float currentSegmentTime = 0.0f;
bool isWaiting = true;
float waitTimer = 0.0f;
int passengersNumber = 0;
int punishmentNumber = 0;
bool showControls = false;
double lastTime;

// Funkcije (Prototipi)
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// --- 2D SIMULATION HELPER FUNCTIONS ---
void preprocessTexture(unsigned& texture, const char* filepath) {
    texture = loadImageToTexture(filepath);
    if (texture != 0) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void formVAOTextured2D(float* vertices, size_t size, unsigned int& VAO) {
    unsigned int VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void formVAOPosition2D(std::vector<float> vertices, size_t size, unsigned int& VAO) {
    unsigned int VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, size, vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

float randomOffset(float range) {
    return (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
}

void drawPath(unsigned int shader, unsigned int VAO, int numPoints) {
    glUseProgram(shader);
    glUniform4f(glGetUniformLocation(shader, "uColor"), 1.0f, 0.0f, 0.0f, 1.0f);
    glUniform2f(glGetUniformLocation(shader, "uPosOffset"), 0.0f, 0.0f);
    glLineWidth(10.0f);
    glBindVertexArray(VAO);
    glDrawArrays(GL_LINE_LOOP, 0, numPoints);
    glBindVertexArray(0);
}

void drawStations2D(unsigned int shader, unsigned int VAO, float* positions, int num) {
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stationTexture);
    glBindVertexArray(VAO);
    for (int i = 0; i < num; ++i) {
        glUniform1f(glGetUniformLocation(shader, "uX"), positions[2 * i]);
        glUniform1f(glGetUniformLocation(shader, "uY"), positions[2 * i + 1]);
        glUniform1f(glGetUniformLocation(shader, "uS"), STATION_SCALE);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    glBindVertexArray(0);
}

void drawBus2D(unsigned int shader, unsigned int VAO, float x, float y) {
    glUseProgram(shader);
    glUniform1f(glGetUniformLocation(shader, "uX"), x);
    glUniform1f(glGetUniformLocation(shader, "uY"), y);
    glUniform1f(glGetUniformLocation(shader, "uS"), BUS_SCALE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, busTexture);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void drawIcon2D(unsigned int shader, unsigned int VAO, unsigned int tex, float x, float y, float scale) {
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1f(glGetUniformLocation(shader, "uX"), x);
    glUniform1f(glGetUniformLocation(shader, "uY"), y);
    glUniform1f(glGetUniformLocation(shader, "uS"), scale);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

int main() {
	// 1. Inicijalizacija GLFW
	if (!glfwInit()) {
		std::cerr << "Greška pri inicijalizaciji GLFW!" << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// 2. Kreiranje prozora
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Autobus Simulator 3D", NULL, NULL);
	if (!window) {
		std::cerr << "Greška pri kreiranju prozora!" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// 3. Callback funkcije i miš
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// 4. Inicijalizacija GLEW
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		std::cerr << "Greška pri inicijalizaciji GLEW!" << std::endl;
		return -1;
	}

	// 5. OpenGL Opcije
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	srand(time(NULL));
	lastTime = glfwGetTime();

	// === LOAD 3D SHADERS ===
	std::string vSourceStr = readFile("basic.vert");
	std::string fSourceStr = readFile("basic.frag");
	unsigned int shaderProgram = createShader(vSourceStr.c_str(), fSourceStr.c_str());

	std::string vTexSourceStr = readFile("texture.vert");
	std::string fTexSourceStr = readFile("texture.frag");
	unsigned int textureShader = createShader(vTexSourceStr.c_str(), fTexSourceStr.c_str());

	// === LOAD 2D SHADERS FOR FRAMEBUFFER ===
	std::string vRect = readFile("rect.vert");
	std::string fRect = readFile("rect.frag");
	rectShader2D = createShader(vRect.c_str(), fRect.c_str());
	glUseProgram(rectShader2D);
	glUniform1i(glGetUniformLocation(rectShader2D, "uTex0"), 0);

	std::string vColor = readFile("color.vert");
	std::string fColor = readFile("color.frag");
	colorShader2D = createShader(vColor.c_str(), fColor.c_str());

	// === LOAD TEXTURES FOR 2D SIMULATION ===
	preprocessTexture(busTexture, "res/avtobus.png");
	preprocessTexture(stationTexture, "res/busstation.jpeg");
	preprocessTexture(closedIconTexture, "res/zatvorena.png");
	preprocessTexture(openIconTexture, "res/otvorena.png");
	preprocessTexture(controlIconTexture, "res/kontrola.png");

	// === SETUP 2D SIMULATION DATA ===
	const int NUM_STATIONS = 10;
	float verticesBus2D[] = { -0.5f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f };
	float verticesStation2D[] = { -0.5f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f };

	float stationPositions[NUM_STATIONS * 2];
	float a = 0.8f;
	float b = 0.5f;
	for (int i = 0; i < NUM_STATIONS; ++i) {
		float angle = i * 2 * M_PI / NUM_STATIONS;
		stationPositions[2 * i] = cos(angle) * a;
		stationPositions[2 * i + 1] = sin(angle) * b;
	}

	std::vector<float> pathVertices;
	const int CURVE_POINTS_PER_SEGMENT = 5;
	const float WIGGLE_RANGE = 0.08f;
	for (int i = 0; i < NUM_STATIONS; ++i) {
		float x1 = stationPositions[2 * i];
		float y1 = stationPositions[2 * i + 1];
		float x2 = stationPositions[2 * ((i + 1) % NUM_STATIONS)];
		float y2 = stationPositions[2 * ((i + 1) % NUM_STATIONS) + 1];
		pathVertices.push_back(x1);
		pathVertices.push_back(y1);
		for (int j = 1; j < CURVE_POINTS_PER_SEGMENT; ++j) {
			float t = (float)j / CURVE_POINTS_PER_SEGMENT;
			float interX = x1 * (1.0f - t) + x2 * t;
			float interY = y1 * (1.0f - t) + y2 * t;
			float wiggleFactor = sin(t * M_PI);
			pathVertices.push_back(interX + randomOffset(WIGGLE_RANGE * wiggleFactor));
			pathVertices.push_back(interY + randomOffset(WIGGLE_RANGE * wiggleFactor));
		}
	}

	unsigned int VAObus2D, VAOstation2D, VAOpath2D;
	formVAOTextured2D(verticesBus2D, sizeof(verticesBus2D), VAObus2D);
	formVAOTextured2D(verticesStation2D, sizeof(verticesStation2D), VAOstation2D);
	formVAOPosition2D(pathVertices, pathVertices.size() * sizeof(float), VAOpath2D);
	int totalPathPoints = pathVertices.size() / 2;

	float busX = stationPositions[0];
	float busY = stationPositions[1];

	// === CREATE FRAMEBUFFER FOR 2D DISPLAY ===
	unsigned int framebuffer;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	unsigned int textureColorbuffer;
	glGenTextures(1, &textureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FBO_WIDTH, FBO_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// === SETUP 3D CABIN ===
	float vertices[] = {
		// 0-3: POD
		-2.0f, 0.0f,  2.0f,  2.0f, 0.0f,  2.0f,  2.0f, 0.0f, -5.0f, -2.0f, 0.0f, -5.0f,
		// 4-7: PLAFON
		-2.0f, 3.0f,  2.0f,  2.0f, 3.0f,  2.0f,  2.0f, 3.0f, -5.0f, -2.0f, 3.0f, -5.0f,
		// 8-11: LEVI ZID
		-2.0f, 0.0f,  2.0f, -2.0f, 0.0f, -5.0f, -2.0f, 3.0f, -5.0f, -2.0f, 3.0f,  2.0f,
		// 12-15: DESNI ZID
		 2.0f, 0.0f,  2.0f,  2.0f, 0.0f, -5.0f,  2.0f, 3.0f, -5.0f,  2.0f, 3.0f,  2.0f,
		// 16-19: ŠOFERŠAJBNA
		-2.0f, 0.0f, -5.0f,  2.0f, 0.0f, -5.0f,  2.0f, 3.0f, -5.0f, -2.0f, 3.0f, -5.0f,
		// 20-23: KONTROLNA TABLA (sa teksturnim koordinatama)
		-0.8f, 0.8f, -2.0f, 0.0f, 1.0f,  // Gornja leva
		 0.8f, 0.8f, -2.0f, 1.0f, 1.0f,  // Gornja desna
		 0.8f, 0.0f, -2.0f, 1.0f, 0.0f,  // Donja desna
		-0.8f, 0.0f, -2.0f, 0.0f, 0.0f   // Donja leva
	};

	unsigned int indices[] = {
		0, 1, 2, 2, 3, 0,          // Pod
		4, 5, 6, 6, 7, 4,          // Plafon
		8, 9, 10, 10, 11, 8,       // Levi zid
		12, 13, 14, 14, 15, 12,    // Desni zid
		16, 17, 18, 18, 19, 16     // Šoferšajbna
	};

	unsigned int controlIndices[] = {
		0, 1, 2, 2, 3, 0           // Kontrolna tabla
	};

	// VAO for cabin
	unsigned int VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// VAO for control panel (with texture coordinates)
	unsigned int VAOcontrol, VBOcontrol, EBOcontrol;
	glGenVertexArrays(1, &VAOcontrol);
	glGenBuffers(1, &VBOcontrol);
	glGenBuffers(1, &EBOcontrol);

	glBindVertexArray(VAOcontrol);
	glBindBuffer(GL_ARRAY_BUFFER, VBOcontrol);
	glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), &vertices[20 * 3], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOcontrol);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(controlIndices), controlIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	// --- RENDER PETLJA ---
	while (!glfwWindowShouldClose(window)) {
		processInput(window);

		// === UPDATE 2D SIMULATION LOGIC ===
		double currentTime = glfwGetTime();
		float deltaTime = (float)(currentTime - lastTime);
		lastTime = currentTime;

		if (isWaiting) {
			waitTimer += deltaTime;
			if (waitTimer >= STATION_WAIT_SECONDS) {
				isWaiting = false;
				currentSegmentTime = 0.0f;
				waitTimer = 0.0f;
				currentStationIndex = (currentStationIndex + 1) % NUM_STATIONS;
			}
		}
		else {
			currentSegmentTime += deltaTime;
			float t = currentSegmentTime / TRAVEL_TIME_SECONDS;
			if (t >= 1.0f) {
				if (showControls) {
					passengersNumber -= punishmentNumber + 1;
					std::cout << "Kazna: " << punishmentNumber << " | Preostali putnici: " << passengersNumber << std::endl;
					showControls = false;
					punishmentNumber = 0;
				}
				t = 1.0f;
				isWaiting = true;
			}
			int startIdx = ((currentStationIndex - 1 + NUM_STATIONS) % NUM_STATIONS) * 2;
			float xA = stationPositions[startIdx];
			float yA = stationPositions[startIdx + 1];
			int endIdx = currentStationIndex * 2;
			float xB = stationPositions[endIdx];
			float yB = stationPositions[endIdx + 1];
			busX = xA * (1.0f - t) + xB * t;
			busY = yA * (1.0f - t) + yB * t;
		}

		// === RENDER TO FRAMEBUFFER (2D SIMULATION) ===
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glViewport(0, 0, FBO_WIDTH, FBO_HEIGHT);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		drawPath(colorShader2D, VAOpath2D, totalPathPoints);
		drawStations2D(rectShader2D, VAOstation2D, stationPositions, NUM_STATIONS);
		drawBus2D(rectShader2D, VAObus2D, busX, busY);

		unsigned int statusTex = isWaiting ? openIconTexture : closedIconTexture;
		drawIcon2D(rectShader2D, VAObus2D, statusTex, 0.75f, 0.85f, 0.2f);

		if (showControls) {
			drawIcon2D(rectShader2D, VAObus2D, controlIconTexture, -0.75f, 0.85f, 0.3f);
		}

		// === RENDER TO SCREEN (3D CABIN) ===
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.5f, 0.8f, 0.9f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		glm::mat4 model = glm::mat4(1.0f);

		// Draw cabin with basic shader
		glUseProgram(shaderProgram);
		int colorLoc = glGetUniformLocation(shaderProgram, "color");
		int alphaLoc = glGetUniformLocation(shaderProgram, "alpha");
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

		glBindVertexArray(VAO);
		glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f);
		glUniform1f(alphaLoc, 1.0f);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f);
		glUniform1f(alphaLoc, 1.0f);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(unsigned int)));

		glUniform3f(colorLoc, 0.2f, 0.2f, 0.2f);
		glUniform1f(alphaLoc, 0.8f);
		glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, (void*)(12 * sizeof(unsigned int)));

		glUniform3f(colorLoc, 0.0f, 0.3f, 0.5f);
		glUniform1f(alphaLoc, 0.3f);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(24 * sizeof(unsigned int)));

		// Draw control panel with framebuffer texture
		glUseProgram(textureShader);
		glUniformMatrix4fv(glGetUniformLocation(textureShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(textureShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(textureShader, "model"), 1, GL_FALSE, glm::value_ptr(model));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
		glUniform1i(glGetUniformLocation(textureShader, "screenTexture"), 0);

		glBindVertexArray(VAOcontrol);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteFramebuffers(1, &framebuffer);
	glDeleteTextures(1, &textureColorbuffer);
	glDeleteVertexArrays(1, &VAO);
	glDeleteVertexArrays(1, &VAOcontrol);
	glDeleteVertexArrays(1, &VAObus2D);
	glDeleteVertexArrays(1, &VAOstation2D);
	glDeleteVertexArrays(1, &VAOpath2D);
	glfwTerminate();
	return 0;
}

// Obrada unosa sa tastature (ESC za izlaz)
void processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// Obrada tastature za kontrolu
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (isWaiting) {
		if (key == GLFW_KEY_K && action == GLFW_PRESS && !showControls) {
			showControls = true;
			if (passengersNumber != 0)
				punishmentNumber = rand() % passengersNumber;
			passengersNumber++;
			std::cout << "Broj putnika: " << passengersNumber << std::endl;
		}
	}
}

// Obrada miša za dodavanje/oduzimanje putnika
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (isWaiting) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && passengersNumber < 50) {
			passengersNumber++;
			std::cout << "Broj putnika: " << passengersNumber << std::endl;
		}

		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS && passengersNumber > 0) {
			passengersNumber--;
			std::cout << "Broj putnika: " << passengersNumber << std::endl;
		}
	}
}

// Obrada miša (Pogled vozača)
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // --- OGRANIČENJA SPECIFIKACIJE ---
    // 1. Gore/Dole (Y osa) - max 180 stepeni ukupno, ovde +-89 za stabilnost
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // 2. Levo/Desno - 180 stepeni (od -180 do 0, gde je -90 centar)
    if (yaw > 0.0f) yaw = 0.0f;
    if (yaw < -180.0f) yaw = -180.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Promena veličine prozora
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}