#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <random>
#include <unordered_map>

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include "shader.h"
#include "Sphere.h"

int main();
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int modsdouble);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

/*
    TODO:
    Spheres instead of circles
    Sphere UV-s and get shadows on each circle/sphere
*/

glm::vec3 camPos = glm::vec3(10.0f, 10.0f, 10.0f);
glm::vec3 camFront = glm::vec3(-1.0f, -1.0f, -1.0f);
glm::vec3 camUp = glm::vec3(0.0f, 0.0f, 1.0f);
float sensitivity = 5.0f;
bool focused = false;

bool firstMouse = true;
float yaw = 90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 45.0f;
// settings
const unsigned int SCR_WIDTH = 800 * 2;
const unsigned int SCR_HEIGHT = 600 * 2;

const float PI = 3.14159265359f;
float deltaTimeFrame = .0f;
float lastFrame = .0f;


void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float camSpeed = static_cast<float>(sensitivity * deltaTimeFrame);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camPos += camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camPos -= camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camPos -= camSpeed * glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camPos += camSpeed * glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camPos += camSpeed * camUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camPos -= camSpeed * camUp;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}


struct Vertex {
    glm::vec3 pos;
    glm::vec3 col;
    glm::vec3 normal;
    //glm::vec2 texCoord;

    bool operator==(const Vertex& other) const {
        return pos == other.pos;
    }
};

struct Object {
    int index;
    std::string model_path;
    unsigned int vao, vbo, ebo;
    unsigned int wvao, wvbo, webo;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> windices;
    glm::vec3 pos;
    glm::vec3 scale;
    float zAngle;
    int mode;
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos)) >> 1);
        }
    };
}

bool loadModel(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::string model_path) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "./"; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(model_path, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        return false;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            std::unordered_map<Vertex, uint32_t> uniqueVertices{};
            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                Vertex vert{};
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                vert.pos.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vert.pos.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vert.pos.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Check if `normal_index` is zero or positive. negative = no normal data

                if (idx.normal_index >= 0) {
                    vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                }

                /*
                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    vert.texCoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    vert.texCoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                }
                */
                // Optional: vertex colors
                vert.col.r = attrib.colors[3 * size_t(idx.vertex_index) + 0];
                vert.col.g = attrib.colors[3 * size_t(idx.vertex_index) + 1];
                vert.col.b = attrib.colors[3 * size_t(idx.vertex_index) + 2];
                uniqueVertices[vert] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vert);
                //printf("vert pos: %f, %f, %f, \t index:%i \t col: %f, %f, %f\n", vert.pos.x, vert.pos.y, vert.pos.z, idx.vertex_index, vert.col.r, vert.col.g, vert.col.b );
                //vertices.push_back(vert);
                indices.push_back(uniqueVertices[vert]);
            }
            index_offset += fv;
        }
    }
    return true;
}


void generateGridData(std::vector<glm::vec3>& gridVertices, int a) {
    gridVertices.clear();
    glm::vec3 orig = glm::vec3(-a / 2.0f, -a / 2.0f, -2.0f);
    int sq = a * a;
    for (int i = 0; i < sq; i++) {
        glm::vec3 p1(i % a, i / a, 0.0f);
        glm::vec3 p2(i % a + 1, i / a, 0.0f);
        glm::vec3 p3(i % a, i / a + 1, 0.0f);
        glm::vec3 p4(i % a + 1, i / a + 1, 0.0f);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p2);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p3);

        gridVertices.push_back(orig + p2);
        gridVertices.push_back(orig + p4);

        gridVertices.push_back(orig + p3);
        gridVertices.push_back(orig + p4);
    }
}

void generatePlane(float* data, float size) {
    data[0] = size / 2.0f;
    data[1] = size / 2.0f;
    data[2] = 0.0f;
    data[3] = 1.0f;
    data[4] = 1.0f;

    data[5] = size / 2.0f;
    data[6] = -size / 2.0f;
    data[7] = 0.0f;
    data[8] = 1.0f;
    data[9] = 0.0f;

    data[10] = -size / 2.0f;
    data[11] = -size / 2.0f;
    data[12] = 0.0f;
    data[13] = 0.0f;
    data[14] = 0.0f;

    data[15] = -size / 2.0f;
    data[16] = size / 2.0f;
    data[17] = 0.0f;
    data[18] = 0.0f;
    data[19] = 1.0f;
    float vertices[] = {
        // positions          // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f  // top left 
    };
}

void generateCircle(float* data, unsigned int* indices, float rad) {

    for (int i = 0; i < 32; i++) {
        data[3 * i] = rad * cos(i * PI / 16);
        data[3 * i + 1] = rad * sin(i * PI / 16);
        data[3 * i + 2] = 0.0f;
        indices[3 * i] = i;
        indices[3 * i + 1] = (i + 1) % 32;
        indices[3 * i + 2] = 32;
    }
    data[96] = 0.0f;
    data[97] = 0.0f;
    data[98] = 0.0f;
}

void updateBufferData(unsigned int& bufIndex, std::vector<glm::vec3>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, bufIndex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * data.size(), &data[0], GL_DYNAMIC_DRAW);
}

struct Sphere {
    glm::vec3 pos;
    float r;
    float alpha;
    unsigned char* tex;
};

struct Light {
    glm::vec3 pos;
    float size;

};

struct Ground {
    float size;
    int w, h;
    unsigned char* tex;
};


void calculateShadow(Ground& ground, std::vector<Sphere>& s, Light& l) {
    delete[] ground.tex;
    ground.tex = new unsigned char[ground.w * ground.h * 3];
    float resx = ground.size / ground.w;
    float resy = ground.size / ground.h;
    float r1 = l.size;
    float distToLight = l.pos.z;
    float rmax = 0;
    for (int j = 0; j < s.size(); j++) {
        float distToSphere = s[j].pos.z;
        float ratio = distToLight / distToSphere;
        float r = s[j].r * ratio;
        if (rmax < r)
            rmax = r;
    }
    float a_max = pow(r1, 2);
#pragma omp parallel
    {

        for (int i = 0; i < ground.w * ground.h; i++) {
            unsigned char br = 255;
            double a = 0;
            double b = 0;
            int x = i % ground.w;
            int y = i / ground.w;

            glm::vec3 p = glm::vec3(x * resx, y * resy, 0.0f) - glm::vec3(ground.size / 2.0f, ground.size / 2.0f, 0.0f);
            int ca = 0;

            for (int j = 0; j < s.size(); j++) {


                float distToSphere = s[j].pos.z;
                float ratio = distToLight / distToSphere;
                float r2 = s[j].r * ratio;

                glm::vec3 dir = p - s[j].pos;
                dir *= ratio;
                glm::vec3 projectCenter = p - dir;
                float d = glm::length(projectCenter - l.pos);
                int ca = 0;
                if (d > r2 + r1) {
                    b = 0;
                    ca = 0;
                }
                else if (d <= (r1 - r2) && r1 >= r2) {
                    b = pow(r2, 2) / a_max;
                    ca = 1;
                }
                else if (d <= (r2 - r1) && r2 >= r1) {
                    b = pow(r1, 2) / a_max;
                    ca = 2;
                }
                else {
                    float alpha = acos((r1 * r1 + d * d - r2 * r2) / (2 * r1 * d)) * 2;
                    float beta = acos((r2 * r2 + d * d - r1 * r1) / (2 * r2 * d)) * 2;
                    float a1 = 0.5 * beta * r2 * r2 - 0.5f * r2 * r2 * sin(beta);
                    float a2 = 0.5 * alpha * r1 * r1 - 0.5f * r1 * r1 * sin(alpha);
                    b = (a1 + a2) / (a_max * PI);
                    ca = 3;
                }
                a = a + b - a * b;
                //if(ca != 0)
                    //printf("%i, %i: %f, case: %i\n", i % ground.w, i / ground.h, a, ca);
            }

            //if (a > 1.0f)
              //  a = 1.0f;
            br = 255 * (1.0f - a);
            ground.tex[3 * i] = br;
            ground.tex[3 * i + 1] = br;
            ground.tex[3 * i + 2] = br;
        }
    }
}

int main() {
    //set glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Noise Free Shadows", NULL, NULL);
    if (window == NULL) {
        printf("Failed to create GLFW Window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to init GLAD");
        return -1;
    }
    /*
    std::vector<glm::vec3> v;
    std::vector<glm::vec3> c;
    std::vector<glm::vec3> n;

    unsigned int vao, vbo_pos, vbo_col, vbo_norm;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_pos);
    glGenBuffers(1, &vbo_col);
    glGenBuffers(1, &vbo_norm);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * v.size(), &v[0], GL_DYNAMIC_DRAW);
    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_col);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * c.size(), &c[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_norm);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * n.size(), &n[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    */

    // texture stuff

    float circVert[99];
    unsigned int circIndex[96];
    generateCircle(circVert, circIndex, 1.0f);
    for (int i = 0; i < 32; i++)
        printf("%i, %i, %i\n", circIndex[3 * i], circIndex[3 * i + 1], circIndex[3 * i + 2]);
    unsigned int VBO_c, VAO_c, EBO_c;
    glGenVertexArrays(1, &VAO_c);
    glGenBuffers(1, &VBO_c);
    glGenBuffers(1, &EBO_c);

    glBindVertexArray(VAO_c);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_c);
    glBufferData(GL_ARRAY_BUFFER, sizeof(circVert), circVert, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_c);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(circIndex), circIndex, GL_DYNAMIC_DRAW);
    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    Ground g;
    g.size = 20.0f;
    float vertices[] = {
        // positions          // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f  // top left 
    };
    generatePlane(vertices, g.size);

    unsigned int indices[] = {
        0,1,3,
        1,2,3
    };



    //generateCircle(vertices, indices, 1.0f);
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    // load and create a texture 
    // -------------------------
    unsigned int texture1;
    // texture 1
    // ---------
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    g.w = 32;
    g.h = 32;
    g.tex = new unsigned char[g.w * g.h * 3];

    std::vector<Sphere> S;
    Sphere s{};
    s.pos = glm::vec3(1.0f, 0.0f, 2.1f);
    s.r = 3.f;
    S.push_back(s);
    s.pos = glm::vec3(-1.0f, 0.0f, 1.9f);
    s.r = 2.f;
    //S.push_back(s);
    Light l{};
    l.pos = glm::vec3(0.0f, 0.0f, 10.f);
    l.size = 5.0f;

    calculateShadow(g, S, l);

    if (g.tex)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g.w, g.h, 0, GL_RGB, GL_UNSIGNED_BYTE, g.tex);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }


    unsigned int VAO_plane;
    glGenVertexArrays(1, &VAO_plane);

    float axisVertices[] = {
        0.0f,0.0f,0.0f,
        0.0f,0.0f,1.0f,
        0.0f,0.0f,1.0f,
        0.0f,0.0f,1.0f,

        0.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,

        0.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f
    };

    unsigned int VBO_pos_p;
    glGenBuffers(1, &VBO_pos_p);

    glBindVertexArray(VAO_plane);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_pos_p);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVertices), &axisVertices[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    std::vector<glm::vec3> gridVertices;
    generateGridData(gridVertices, 8);

    unsigned int VAO_grid;
    glGenVertexArrays(1, &VAO_grid);

    unsigned int VBO_grid;
    glGenBuffers(1, &VBO_grid);

    glBindVertexArray(VAO_grid);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_grid);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * 3 * sizeof(float), &gridVertices[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    int width, height;
    Shader shader = Shader("C:\\Src\\shaders\\vertFastShadow.glsl", "C:\\Src\\shaders\\fragFastShadow.glsl");
    Shader lightShader = Shader("C:\\Src\\shaders\\vertEmit.glsl", "C:\\Src\\shaders\\fragEmit.glsl");
    Shader lineshader = Shader("C:\\Src\\shaders\\vertParticle.glsl", "C:\\Src\\shaders\\fragParticle.glsl");

    shader.setInt("texture1", 0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glLineWidth(4.0f);

    float orthoScale = 10.0f;
    glm::vec3 lightPos = { 10.0f,10.f,10.f };
    bool ax = true, grid = true;
    int gridSize = 8;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    //ImGui::SetNextWindowPos(viewport->Pos);
    //ImGui::SetNextWindowSize(viewport->Size);
    //ImGui::SetNextWindowViewport(viewport->ID);
    bool ortho = false;
    bool proj = false;
    bool change = false, bilin = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    while (!glfwWindowShouldClose(window))
    {

        // input
        // -----
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTimeFrame = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);

        shader.use();
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        shader.setMat4("view", view);

        glfwGetWindowSize(window, &width, &height);

        float ratio = (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.01f, 1000.0f);
        if (ortho)
            projection = glm::ortho(-orthoScale * ratio, orthoScale * ratio, -orthoScale, orthoScale, -1000.0f, 1000.0f);

        shader.setMat4("projection", projection);

        glm::mat4 model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        shader.setVec3("lightPos", lightPos);
        //glBindVertexArray(vao);
        //glDrawArrays(GL_LINES, 0, v.size());

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        lightShader.use();
        lightShader.setMat4("projection", projection);

        model = glm::translate(model, l.pos);
        model = glm::scale(model, glm::vec3(l.size));
        lightShader.setMat4("model", model);
        lightShader.setMat4("view", view);
        lightShader.setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
        glBindVertexArray(VAO_c);
        glDrawElements(GL_TRIANGLES, 96, GL_UNSIGNED_INT, 0);

        for (int i = 0; i < S.size(); i++) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, S[i].pos);
            model = glm::scale(model, glm::vec3(S[i].r));
            lightShader.setVec3("color", glm::vec3(0.5f - i * 0.05f));
            lightShader.setMat4("model", model);
            glBindVertexArray(VAO_c);
            glDrawElements(GL_TRIANGLES, 96, GL_UNSIGNED_INT, 0);
        }



        model = glm::mat4(1.0f);
        lineshader.use();
        lineshader.setMat4("projection", projection);
        lineshader.setMat4("view", view);
        lineshader.setMat4("model", model);
        if (grid) {
            glBindVertexArray(VAO_grid);
            glDrawArrays(GL_LINES, 0, gridVertices.size());
        }
        if (ax) {
            glBindVertexArray(VAO_plane);
            glDrawArrays(GL_LINES, 0, 12);
        }


        //start of imgui init stuff
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(viewport, dockspace_flags);


        ImGui::Begin("Render Settings");
        ImGui::DragFloat3("Light Pos", &lightPos[0], 0.05);
        ImGui::Checkbox("Orthographic", &ortho);
        if (ortho)
            ImGui::DragFloat("Orthographic Scale", &orthoScale, 0.05, 0.01f, 100.0f);
        ImGui::Checkbox("Grid", &grid);

        if (grid) {
            if (ImGui::DragInt("Size", &gridSize), 1, 100) {
                generateGridData(gridVertices, gridSize);
                updateBufferData(VBO_grid, gridVertices);
            }
        }
        ImGui::Checkbox("Axis", &ax);

        if (ImGui::DragFloat("Ground Size", &g.size, 0.01f)) { change = true; }
        if (ImGui::DragInt("Ground Tex Res", &g.w, 4.0f, 1)) { change = true; }
        if (ImGui::DragFloat("Light Size", &l.size, 0.01f, 0.01f)) { change = true; }
        if (ImGui::DragFloat3("Light Loc", &l.pos[0], 0.01f, 0.01f)) { change = true; }
        if (ImGui::TreeNode("Sphere Tree")) {
            ImGui::DragFloat3("Sphere Loc", &s.pos[0], 0.01f, 0.01f);
            ImGui::DragFloat("Sphere Size", &s.r, 0.01f, 0.01f);
            if (ImGui::Button("Add Sphere")) { S.push_back(s); }
            for (int i = 0; i < S.size(); i++) {
                if (ImGui::TreeNode((void*)(intptr_t)i, "Sphere %d", i)) {
                    if (ImGui::DragFloat3("Sphere Loc", &S[i].pos[0], 0.01f, 0.01f)) { change = true; }
                    if (ImGui::DragFloat("Sphere Size", &S[i].r, 0.01f, 0.01f)) { change = true; }
                    if (ImGui::DragFloat("Sphere Alpha", &S[i].r, 0.01f, 0.01f)) { change = true; }
                    ImGui::TreePop();
                }

            }
            ImGui::TreePop();
        }



        if (change) {
            change = false;
            g.h = g.w;
            generatePlane(vertices, g.size);
            calculateShadow(g, S, l);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glBindTexture(GL_TEXTURE_2D, texture1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g.w, g.h, 0, GL_RGB, GL_UNSIGNED_BYTE, g.tex);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        ImGui::Checkbox("Bilinear Filter", &bilin);
        if (bilin) {
            glBindTexture(GL_TEXTURE_2D, texture1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, texture1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        ImGui::End();
        ImGui::Render();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    delete g.tex;
    //stbi_image_free(data);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (focused) {

        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY; // reversed since y-coordinates go from bottom to top
        lastX = xpos;
        lastY = ypos;

        float mouseSens = 0.2f;
        xoffset *= mouseSens;
        yoffset *= mouseSens;

        yaw -= xoffset;
        pitch -= yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.z = sin(glm::radians(pitch));
        camFront = glm::normalize(front);

    }
    else {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);
        lastX = xpos;
        lastY = ypos;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int modsdouble)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {

    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        if (focused)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        focused = !focused;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

    sensitivity += 0.2f * static_cast<float>(yoffset);
    if (sensitivity < 0) {
        sensitivity = 0.01f;
    }

    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

