#define _CRT_SECURE_NO_WARNINGS
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h" // para cargar los archivos .glb, .gltf

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h> // Crear la ventana, contexto OpenGL y lee teclado/mouse

#include <glm/glm.hpp> // matemáticos
#include <glm/gtc/matrix_transform.hpp> // transformaciones ( translate, rotate...)
#include <glm/gtc/type_ptr.hpp> // convierte matrices/ vectores a punteros

// Dimensiones de ventana
int ventanaAncho = 1000;
int ventanaAlto = 800;

// Cámara (WASD + Mouse)
glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 20.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 orbitTarget = glm::vec3(0.0f, 1.5f, 0.0f); //orbita vertical

//mouse
bool firstMouse = true;
float yaw = -90.0f;
float pitch = -5.0f;
float lastX = ventanaAncho / 2.0f;
float lastY = ventanaAlto / 2.0f;
float mouseSensitivity = 0.1f;

float deltaTime = 0.0f; // deltaTime permite que la velocidad sea igual sin importar los FPS
float lastFrame = 0.0f;
float orbitAngleV = 0.25f; // Ángulo vertical en radianes
float orbitSpeedV = 1.5f;  // Velocidad de órbita vertical (rad/s)

// Vertex shader con posición, UV y normal transformada
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos; //posición local del vértice
layout (location = 1) in vec2 aTexCoords; //cordenadas UV de textura
layout (location = 2) in vec3 aNormal; //normal local del vértice

out vec2 TexCoord;
out vec3 FragPos; //posicion del fragmento vertice en coordenadas del mundo
out vec3 Normal;  //normal transformada a coordenadas del mundo

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoord = aTexCoords;
    FragPos = vec3(model * vec4(aPos, 1.0f));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0f);
}
)";
//
// Fragment shader con textura
// iluminación (Blinn-Phong) especular + ambiental +difusa
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture1;
uniform bool tieneTextura;

uniform vec3 viewPos; //pos camara
uniform vec3 lightPos; //pos luz dentro del mundo
uniform vec3 lightColor; //color luz

void main()
{
// color base basado en UVs
    vec3 baseColor = tieneTextura
        ? texture(texture1, TexCoord).rgb
        : vec3(TexCoord.x, TexCoord.y, 0.65f);

    // Ambiente: evita sombras negras
    float ambientStrength = 0.22f;
    vec3 ambient = ambientStrength * lightColor;

    // Difusa (depende del ángulo entre la normal y la dirección de la luz)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0f);
    vec3 diffuse = diff * lightColor;

    // Especular (Blinn-Phong)
    float specularStrength = 0.35f;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0f), 32.0f);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 resultado = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(resultado, 1.0f);
}
)";

// Shaders para dibujar la curva de Bézier
const char* lineVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0f);
}
)";

const char* lineFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 lineColor;

void main()
{
    FragColor = vec4(lineColor, 1.0f);
}
)";

// mates curva de bezier
glm::vec3 cubicBezier(const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2, const glm::vec3& p3, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return uuu * p0
        + 3.0f * uu * t * p1
        + 3.0f * u * tt * p2
        + ttt * p3;
}

// Compila vertex shader y fragment shader y los enlaza en un shader program
unsigned int compilarProgramaShader(const char* vsSrc, const char* fsSrc) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// Ajusta el viewport al redimensiona la ventana
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    ventanaAncho = width;
    ventanaAlto = height;
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    // Actualiza variables según el desplazamiento del mouse
    float xoffset = (xpos - lastX) * mouseSensitivity;
    float yoffset = (lastY - ypos) * mouseSensitivity;
    lastX = xpos;
    lastY = ypos;

    yaw += xoffset;
    pitch += yoffset;

    // Limita pitch para evitar que la cámara se voltee al pasar de los 90°
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Orbita verticalmente la cámara manteniendo la distancia
void orbitarVertical(float deltaAngulo) {
    // Radio actual desde el objetivo hasta la cámara
    glm::vec3 offset = cameraPos - orbitTarget;
    float radius = glm::length(offset);
    if (radius < 0.001f) radius = 1.0f;

    // Ángulo vertical actual (evitando polos exactos +/- PI/2 para no perder el vector up)
    float currentElev = asin(glm::clamp(offset.y / radius, -0.98f, 0.98f));
    float newElev = glm::clamp(currentElev + deltaAngulo, -1.45f, 1.45f);

    // Proyección horizontal en XZ manteniendo el radio
    float horizontalRadius = radius * cos(newElev);
    float horizLen = sqrt(offset.x * offset.x + offset.z * offset.z);

    glm::vec3 dirXZ = (horizLen > 0.0001f)
        ? glm::vec3(offset.x / horizLen, 0.0f, offset.z / horizLen)
        : glm::vec3(0.0f, 0.0f, 1.0f);

    cameraPos.x = orbitTarget.x + dirXZ.x * horizontalRadius;
    cameraPos.z = orbitTarget.z + dirXZ.z * horizontalRadius;
    cameraPos.y = orbitTarget.y + radius * sin(newElev);

    // Apuntar la cámara hacia el objetivo y sincronizar yaw/pitch para que el mouse siga fluido
    glm::vec3 dirToTarget = glm::normalize(orbitTarget - cameraPos);
    cameraFront = dirToTarget;
    pitch = glm::degrees(asin(glm::clamp(cameraFront.y, -1.0f, 1.0f)));
    yaw = glm::degrees(atan2(cameraFront.z, cameraFront.x));
}

// Lee teclas de movimiento (WASD, Q/E) y órbita vertical (flechas ARRIBA/ABAJO)
//
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float velocidad = 10.0f * deltaTime;
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += velocidad * flatFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= velocidad * flatFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= right * velocidad;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += right * velocidad;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos += cameraUp * velocidad;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos -= cameraUp * velocidad;

    float anguloPaso = orbitSpeedV * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        orbitarVertical(anguloPaso);   // Sube y rodea el modelo desde arriba
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        orbitarVertical(-anguloPaso);  // Baja y rodea el modelo desde abajo
}

//crea VAO/VBO/EBO
struct SubMalla {
    unsigned int VAO, VBO, EBO;
    size_t indexCount;
    int texturaID; // -1 si no tiene textura asignada
};

//
// Carga un archivo .glb, extrae texturas incrustadas, geometría 
bool cargarModeloGLBConMateriales(const std::string& rutaArchivoGLB, std::vector<SubMalla>& submallasSalida, std::vector<unsigned int>& texturasSalida) {
    tinygltf::Model modelo;
    tinygltf::TinyGLTF cargador;
    std::string err, warn;

    std::cout << "Cargando modelo .glb con soporte de materiales: " << rutaArchivoGLB << std::endl;
    bool exito = cargador.LoadBinaryFromFile(&modelo, &err, &warn, rutaArchivoGLB);

    if (!exito) {
        std::cerr << "Error al cargar .glb: " << err << std::endl;
        return false;
    }


    //imágenes empaquetadas en el.glb a texturas 2D
    for (const auto& imagen : modelo.images) {
        if (imagen.width > 0 && imagen.height > 0 && !imagen.image.empty()) {
            unsigned int texID = 0;
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);

            GLenum formato = GL_RGBA;
            if (imagen.component == 3) formato = GL_RGB;
            else if (imagen.component == 1) formato = GL_RED;

            glTexImage2D(GL_TEXTURE_2D, 0, formato, imagen.width, imagen.height, 0, formato, GL_UNSIGNED_BYTE, imagen.image.data());
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            texturasSalida.push_back(texID);
            std::cout << "Textura incrustada cargada con exito. ID GPU: " << texID << std::endl;
        }
        else {
            texturasSalida.push_back(0);
        }
    }

    //Recorrer mallas y primitivas para construir cada SubMalla
    for (const auto& mesh : modelo.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) continue;

            int texturaAsignada = -1;
            if (primitive.material >= 0 && primitive.material < modelo.materials.size()) {
                const auto& material = modelo.materials[primitive.material];
                int texIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                if (texIndex >= 0 && texIndex < modelo.textures.size()) {
                    int imageIndex = modelo.textures[texIndex].source;
                    if (imageIndex >= 0 && imageIndex < texturasSalida.size()) {
                        texturaAsignada = texturasSalida[imageIndex];
                    }
                }
            }

            const auto& posAccessor = modelo.accessors[primitive.attributes.find("POSITION")->second];
            const auto& posBufferView = modelo.bufferViews[posAccessor.bufferView];
            const auto& posBuffer = modelo.buffers[posBufferView.buffer];
            const float* posiciones = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);

            const float* normales = nullptr;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const auto& normAccessor = modelo.accessors[primitive.attributes.find("NORMAL")->second];
                const auto& normBufferView = modelo.bufferViews[normAccessor.bufferView];
                const auto& normBuffer = modelo.buffers[normBufferView.buffer];
                normales = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
            }

            const float* uvs = nullptr;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const auto& uvAccessor = modelo.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                const auto& uvBufferView = modelo.bufferViews[uvAccessor.bufferView];
                const auto& uvBuffer = modelo.buffers[uvBufferView.buffer];
                uvs = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset + uvAccessor.byteOffset]);
            }

            std::vector<float> verticesLocales;
            for (size_t i = 0; i < posAccessor.count; ++i) {
                verticesLocales.push_back(posiciones[i * 3 + 0]);
                verticesLocales.push_back(posiciones[i * 3 + 1]);
                verticesLocales.push_back(posiciones[i * 3 + 2]);

                if (uvs) {
                    verticesLocales.push_back(uvs[i * 2 + 0]);
                    verticesLocales.push_back(uvs[i * 2 + 1]);
                }
                else {
                    verticesLocales.push_back(0.0f);
                    verticesLocales.push_back(0.0f);
                }

                if (normales) {
                    verticesLocales.push_back(normales[i * 3 + 0]);
                    verticesLocales.push_back(normales[i * 3 + 1]);
                    verticesLocales.push_back(normales[i * 3 + 2]);
                }
                else {
                    verticesLocales.push_back(0.0f);
                    verticesLocales.push_back(0.0f);
                    verticesLocales.push_back(1.0f);
                }
            }


            // Leer índices para dibujar con glDrawElements
            std::vector<unsigned int> indicesLocales;
            if (primitive.indices >= 0) {
                const auto& indexAccessor = modelo.accessors[primitive.indices];
                const auto& indexBufferView = modelo.bufferViews[indexAccessor.bufferView];
                const auto& indexBuffer = modelo.buffers[indexBufferView.buffer];
                const void* datosIndices = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

                for (size_t i = 0; i < indexAccessor.count; ++i) {
                    unsigned int indice = 0;
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        indice = reinterpret_cast<const unsigned short*>(datosIndices)[i];
                    }
                    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        indice = reinterpret_cast<const unsigned int*>(datosIndices)[i];
                    }
                    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
                        indice = reinterpret_cast<const unsigned char*>(datosIndices)[i];
                    }
                    indicesLocales.push_back(indice);
                }
            }

            SubMalla submalla;
            submalla.indexCount = indicesLocales.size();
            submalla.texturaID = texturaAsignada;

            glGenVertexArrays(1, &submalla.VAO);
            glGenBuffers(1, &submalla.VBO);
            glGenBuffers(1, &submalla.EBO);

            glBindVertexArray(submalla.VAO);

            glBindBuffer(GL_ARRAY_BUFFER, submalla.VBO);
            glBufferData(GL_ARRAY_BUFFER, verticesLocales.size() * sizeof(float), verticesLocales.data(), GL_STATIC_DRAW);

            if (!indicesLocales.empty()) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submalla.EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesLocales.size() * sizeof(unsigned int), indicesLocales.data(), GL_STATIC_DRAW);
            }

            GLsizei stride = 8 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);
            submallasSalida.push_back(submalla);
        }
    }
    return true;
}

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(ventanaAncho, ventanaAlto, "Proyecto OpenGL - Mystery Shack", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glEnable(GL_DEPTH_TEST);

    std::string rutaGLB = "C:\\Users\\57312\\Desktop\\tercer corte\\mystery_shack.glb";
    std::vector<SubMalla> submallas;
    std::vector<unsigned int> texturasGlobales;

    if (!cargarModeloGLBConMateriales(rutaGLB, submallas, texturasGlobales)) {
        std::cerr << "Error crítico al cargar submallas del .glb" << std::endl;
        glfwTerminate();
        return -1;
    }

    unsigned int shaderProgram = compilarProgramaShader(vertexShaderSource, fragmentShaderSource);
    unsigned int lineShader = compilarProgramaShader(lineVertexShaderSource, lineFragmentShaderSource);

    // Puntos de control de la curva de Bézier cúbica alrededor de la escena
    glm::vec3 p0(-12.0f, 2.0f, 8.0f);
    glm::vec3 p1(-6.0f, 10.0f, 12.0f);
    glm::vec3 p2(6.0f, 10.0f, -10.0f);
    glm::vec3 p3(12.0f, 3.0f, 6.0f);

    //curva en 120 segmentos
    std::vector<glm::vec3> puntosCurva;
    const int segmentos = 120;
    for (int i = 0; i <= segmentos; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segmentos);
        puntosCurva.push_back(cubicBezier(p0, p1, p2, p3, t));
    }

    unsigned int curveVAO, curveVBO;
    glGenVertexArrays(1, &curveVAO);
    glGenBuffers(1, &curveVBO);
    glBindVertexArray(curveVAO);
    glBindBuffer(GL_ARRAY_BUFFER, curveVBO);
    glBufferData(GL_ARRAY_BUFFER, puntosCurva.size() * sizeof(glm::vec3), puntosCurva.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    //renderloop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Matrices de proyección (perspectiva) y vista (cámara)
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)ventanaAncho / (float)ventanaAlto, 0.1f, 1000.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // Posición dinámica sobre la curva de Bézier (usada aquí para mover la luz)
        float t = 0.5f * (1.0f + sin(currentFrame * 0.7f));
        glm::vec3 lightPos = cubicBezier(p0, p1, p2, p3, t);

        // modelo GLB con iluminación y texturas
        glUseProgram(shaderProgram);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(0.3f));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 0.96f, 0.88f);

        for (const auto& sm : submallas) {
            if (sm.texturaID != -1) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sm.texturaID);
                glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
                glUniform1i(glGetUniformLocation(shaderProgram, "tieneTextura"), GL_TRUE);
            }
            else {
                glUniform1i(glGetUniformLocation(shaderProgram, "tieneTextura"), GL_FALSE);
            }

            glBindVertexArray(sm.VAO);
            if (sm.indexCount > 0) {
                glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sm.indexCount), GL_UNSIGNED_INT, 0);
            }
            glBindVertexArray(0);
        }

        //dibujar la curva de Bézier
        glUseProgram(lineShader);
        glm::mat4 lineModel = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(lineShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(lineShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(lineShader, "model"), 1, GL_FALSE, glm::value_ptr(lineModel));
        glUniform3f(glGetUniformLocation(lineShader, "lineColor"), 1.0f, 0.85f, 0.2f);

        glBindVertexArray(curveVAO);
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(puntosCurva.size()));
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &curveVAO);
    glDeleteBuffers(1, &curveVBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(lineShader);

    glfwTerminate();
    return 0;
}