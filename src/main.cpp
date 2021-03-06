#include "Helpers.h"
#include <GLFW/glfw3.h>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <chrono>
#include <iostream>
using namespace std;
using namespace Eigen;

// VertexBufferObject Definition: vertices, other properties
VertexBufferObject VBO, VBO_1, VBO_C;

// Matrix Definition: defined as MatrixXf M(rows,cols)
Eigen::MatrixXf V(2,3); // store the vertex positions of the initial example
Eigen::MatrixXf C(3,3); // store the property: color

// Global Variable: Triangle Insertion Mode
int clicks = 0, insertion = 1;
Eigen::MatrixXf Vertex(2,3); // store the vertex positions; each triangle has the size of 6 = 2#d * 3#v values
// Global Variable: Triangle Translation Mode
int selectTri = 0;
Vector2f beginning(0, 0), ending(0, 0);
// Global Variable: Triangle Deletion Mode
int deleteTri = 0, deletion = 0;

// Global Variable: Triangle Rotation and Scaling Mode
Eigen::Matrix4f rotation(4,4);
Eigen::Matrix4f scaling(4,4);
Eigen::Matrix4f trsToOri(4,4), trsBack(4,4);
Eigen::Matrix4f trs(4,4); // contains the transformation matrix; uploaded to GPU as a uniform in order to execute the transformation in the shader
int rotTri = 0;
const double pi = 3.14159265358979323846;
float clockwise = (float)(-10*pi/180), counter_clockwise = (float)(10*pi/180);
float scale_up = 1.25, scale_down = 0.75;
Vector2f curBarycenter;

// Global Variable: Triangle Coloring Mode
#define INF (float)1e+300
float closer = INF;
int recolorVer = 0;
Eigen::MatrixXf Color(3,3); // store the vertex property: each vertex has its own color
Eigen::MatrixXf newColors(3,9);
int recoloring = 10;

// Global Variable: View Control
Eigen::MatrixXf zoomio(4, 4);
Eigen::MatrixXf pan(4, 4);
Eigen::Matrix4f view(4,4); // contains the view transformation
float viewCtrl = 0.2f; // zoom and pan 20%

// Global Variable: Keyframing
float kfFactor = (rand()%(10 - 1 + 1) + 1) / 10.0f;
Eigen::MatrixXf NextVertex(4, 4);

// Mode Control
struct modeFlags{
    bool modeI, modeO, modeP; // section 1.1
    bool modeH, modeJ, modeK, modeL; // section 1.2 (transformation done on the CPU side) & 1.8 (transformation done in the shader)
    bool modeC; // section 1.3
    bool modeW, modeA, modeS, modeD, modeMinus, modePlus; // section 1.4
    bool keyFraming; // section 1.5
};
modeFlags mode = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

/*
 * helper function: Calculating the alpha, beta, gamma based on current cursor position
 * a, b, c: 3 vertices of the triangle, counter-clockwise (a, b, c)
 * p_x, p_y: the position of current cursor (p_x, p_y)
 * return: the list of alpha, beta, gamma values
 * */
Vector3f parameterList(Vector2f a, Vector2f b, Vector2f c, float p_x, float p_y){
    float a_x = a(0), a_y = a(1), b_x = b(0), b_y = b(1), c_x = c(0), c_y = c(1);
    float numerator_abc = a_x * (b_y - c_y) + b_x * (c_y - a_y) + c_x * (a_y - b_y);
    float area_abc = (float)(abs(numerator_abc) / 2.0);
    float numerator_pbc = p_x * (b_y - c_y) + b_x * (c_y - p_y) + c_x * (p_y - b_y);
    float area_pbc = (float)(abs(numerator_pbc) / 2.0);
    float numerator_apc = a_x * (p_y - c_y) + p_x * (c_y - a_y) + c_x * (a_y - p_y);
    float area_apc = (float)(abs(numerator_apc) / 2.0);
    float numerator_abp = a_x * (b_y - p_y) + b_x * (p_y - a_y) + p_x * (a_y - b_y);
    float area_abp = (float)(abs(numerator_abp) / 2.0);

    // Figure out whether p is inside the triangle constructed by points a, b, c
    float alpha = area_pbc / area_abc, beta = area_apc / area_abc, gamma = area_abp / area_abc;
    Vector3f result(alpha, beta, gamma);
    return result;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    double xpos, ypos; // Get the position of the mouse in the window
    glfwGetCursorPos(window, &xpos, &ypos);
    int width, height; // Get the size of the window
    glfwGetWindowSize(window, &width, &height);
    double xworld = ((xpos/double(width))*2)-1;// Convert screen position to world coordinates
    double yworld = (((height-1-ypos)/double(height))*2)-1;// NOTE: y axis is flipped in glfw

    // Keep track of the mouse clicks
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS){
        printf("mouse enter: (%f, %f)\n", xworld, yworld);

        // section 1.1
        if(mode.modeI == true){ // Update the info of triangle vertices (previous info kept) on the CPU side
            Vertex.conservativeResize(2, clicks + 1);
            Vertex.col(clicks) << (float)xworld, (float)yworld;
            clicks++;
            C.conservativeResize(3, Vertex.cols());
        }// end of Triangle Insertion Mode
        if(mode.modeO == true){
            float p_x = (float)(((xpos / float(width)) * 2) - 1.0);
            float p_y = (float)((((height - 1 - ypos) / float(height)) * 2) - 1.0);
            beginning << p_x, p_y;

            // Loop on each triangle inserted: i is the index of triangle
            for (int i = 0; i < Vertex.cols() / 3; i++) {
                Vector3f judgement = parameterList(Vertex.col(i*3), Vertex.col(i*3 + 1), Vertex.col(i*3 + 2), p_x, p_y);
                if ((judgement(0) >= 0 && judgement(0) <= 1) && (judgement(1) >= 0 && judgement(1) <= 1) && (judgement(2) >= 0 && judgement(2) <= 1)) {
                    selectTri = i;
                }// Figure out whether p is inside the triangle constructed by points a, b, c
            }// end of inside/outside judgement on cursor
            std::cout << "Triangle No." << selectTri+1 << " Selected! " << std::endl;
        }// end of Triangle Translation Mode
        if(mode.modeP == true){
            float p_x = (float)(((xpos / float(width)) * 2) - 1.0);
            float p_y = (float)((((height - 1 - ypos) / float(height)) * 2) - 1.0);

            // Loop on each triangle inserted: i is the index of triangle
            for (int i = 0; i < Vertex.cols() / 3; i++) {
                Vector3f judgement = parameterList(Vertex.col(i*3), Vertex.col(i*3 + 1), Vertex.col(i*3 + 2), p_x, p_y);
                if ((judgement(0) >= 0 && judgement(0) <= 1) && (judgement(1) >= 0 && judgement(1) <= 1) && (judgement(2) >= 0 && judgement(2) <= 1)) {
                    deleteTri = i;
                    deletion++;
                }
            }// end of inside/outside judgement on cursor
            std::cout << "Triangle No." << deleteTri+1 << " Deleted! " << std::endl;

            // Delete the group of selected columns
            int numRows = Vertex.rows(), numCols = Vertex.cols();
            if( 3*deleteTri < numCols ){
                for(unsigned i = 0; i < 3; i++){
                    Vertex.block(0,3*deleteTri,numRows,numCols-3*deleteTri) = Vertex.block(0,3*deleteTri+1,numRows,numCols-3*deleteTri);
                }
            }
            Vertex.conservativeResize(numRows,numCols);
            C.conservativeResize(3, Vertex.cols()); // Update the size of vertex colors
        }// end of Triangle Deletion Mode

        // section 1.2
        if(mode.modeH == true || mode.modeJ == true || (mode.modeK == true || mode.modeL == true)){
            float p_x = (float)(((xpos / float(width)) * 2) - 1.0);
            float p_y = (float)((((height - 1 - ypos) / float(height)) * 2) - 1.0);

            // Loop on each triangle inserted: i is the index of triangle
            for (int i = 0; i < Vertex.cols() / 3; i++) {
                Vector3f judgement = parameterList(Vertex.col(i*3), Vertex.col(i*3 + 1), Vertex.col(i*3 + 2), p_x, p_y);
                if ((judgement(0) >= 0 && judgement(0) <= 1) && (judgement(1) >= 0 && judgement(1) <= 1) && (judgement(2) >= 0 && judgement(2) <= 1)) {
                    rotTri = i;
                }
            }// end of inside/outside judgement on cursor
            std::cout << "Triangle No." << rotTri+1 << " Rotated Clockwise! " << std::endl;
        }// end of Triangle Rotation & Scaling Mode

        // section 1.3
        if(mode.modeC == true) {
            // Loop on each triangle inserted: i is the index of triangle
            closer = INF; // Initialize the closer definition for each mouse click
            for (int i = 0; i < Vertex.cols() / 3; i++) {
                Vector2f a, b, c; // Get the position of triangle vertices
                a << Vertex.col(i*3);
                b << Vertex.col(i*3 + 1);
                c << Vertex.col(i*3 + 2);
                float a_x = a(0), a_y = a(1), b_x = b(0), b_y = b(1), c_x = c(0), c_y = c(1);
                float distance_1 = (float)(sqrt((xworld - a_x)*(xworld - a_x)+(yworld - a_y)*(yworld - a_y)));
                if(distance_1 < closer){
                    closer = distance_1;
                    recolorVer = i*3;
                }
                float distance_2 = (float)(sqrt((xworld - b_x)*(xworld - b_x)+(yworld - b_y)*(yworld - b_y)));
                if(distance_2 < closer){
                    closer = distance_2;
                    recolorVer = i*3 + 1;
                }
                float distance_3 = (float)(sqrt((xworld - c_x)*(xworld - c_x)+(yworld - c_y)*(yworld - c_y)));
                if(distance_3 < closer){
                    closer = distance_3;
                    recolorVer = i*3 + 2;
                }
                std::cout << "Vertex No." << recolorVer << " will be assigned a new color! " << std::endl;
            }// end of finding the closer vertex

            newColors << 128.0f/255.0f, 178.0f/255.0f, 1,            1,             1, 124.0f/255.0f, 0,             30.0f/255.0f,  188.0f/255.0f,
                         0,             34.0f/255.0f,  69.0f/255.0f, 165.0f/255.0f, 1, 252.0f/255.0f, 1,             144.0f/255.0f, 143.0f/255.0f,
                         0,             34.0f/255.0f,  0,            0,             0, 0,             127.0f/255.0f, 1,             143.0f/255.0f;
        }// end of Triangle Coloring Mode
    }// end of mouse PRESSED

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE){
        printf("mouse leave: (%f, %f)\n", xworld, yworld);

        // section 1.2
        if(mode.modeO == true){
            ending << (float)xworld, (float)yworld;
            printf("mouse direction: (%f, %f)\n", ending(0)-beginning(0), ending(1)-beginning(1));
            int index = selectTri*3;

            // Update the position of vertices on the CPU side
            Vertex.col(index) << Vertex.col(index) + ending - beginning;
            Vertex.col(index + 1) << Vertex.col(index + 1) + ending - beginning;
            Vertex.col(index + 2) << Vertex.col(index + 2) + ending - beginning;
            VBO.update(Vertex);
        }
    }// end of mouse RELEASED
    // Upload the vertex set to the GPU
    VBO.update(Vertex);
    VBO_1.update(Vertex);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    switch (key){ // Key Options
        case  GLFW_KEY_I:
            std::cout << "Triangle Insertion Mode: Enabled\n" << std::endl;
            mode = {true, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_O:
            std::cout << "Triangle Translation Mode: Enabled\n" << std::endl;
            mode = {false, true, false, false, false, false, false, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_P:
            std::cout << "Triangle Deletion Mode: Enabled\n" << std::endl;
            mode = {false, false, true, false, false, false, false, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_H:
            std::cout << "Triangle Rotation (clockwise) Mode: Enabled\n" << std::endl;
            mode = {false, false, false, true, false, false, false, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_J:
            std::cout << "Triangle Rotation (counter-clockwise) Mode: Enabled\n" << std::endl;
            mode = {false, false, false, false, true, false, false, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_K:
            std::cout << "Triangle Scaling (up) Mode: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, true, false, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_L:
            std::cout << "Triangle Scaling (down) Mode: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, true, false, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_C:
            std::cout << "Triangle Coloring Mode: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, true, false, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_1:
            recoloring = 1;
            std::cout << "New Color: Maroon" << std::endl;
            break;
        case GLFW_KEY_2:
            recoloring = 2;
            std::cout << "New Color: Firebrick" << std::endl;
            break;
        case GLFW_KEY_3:
            recoloring = 3;
            std::cout << "New Color: Tomato" << std::endl;
            break;
        case GLFW_KEY_4:
            recoloring = 4;
            std::cout << "New Color: Orange" << std::endl;
            break;
        case GLFW_KEY_5:
            recoloring = 5;
            std::cout << "New Color: Gold" << std::endl;
            break;
        case GLFW_KEY_6:
            recoloring = 6;
            std::cout << "New Color: Lawn" << std::endl;
            break;
        case GLFW_KEY_7:
            recoloring = 7;
            std::cout << "New Color: Spring" << std::endl;
            break;
        case GLFW_KEY_8:
            recoloring = 8;
            std::cout << "New Color: Dodger" << std::endl;
            break;
        case GLFW_KEY_9:
            recoloring = 9;
            std::cout << "New Color: Rosy" << std::endl;
            break;
        case  GLFW_KEY_W:
            std::cout << "Translate Whole Scene Down: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, true, false, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_A:
            std::cout << "Translate Whole Scene Right: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, false, true, false, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_S:
            std::cout << "Translate Whole Scene Up: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, false, false, true, false, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_D:
            std::cout << "Translate Whole Scene Left: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, false, false, false, true, false, false, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_MINUS:
            std::cout << "Zoom Whole Scene In: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, false, false, false, false, true, false, false};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_KP_ADD:
            std::cout << "Zoom Whole Scene Out: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, false, false, false, false, false, true, false};
            glfwSetTime (10.0);
            break;
        case  GLFW_KEY_F:
            std::cout << "Keyframing: Enabled\n" << std::endl;
            mode = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, true};
            glfwSetTime (10.0);
            break;
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, 1);
        default:
            break;
    }
}

/*
 * helper function: Calculating the Barycenter of A Triangle
 * a, b, c: 3 vertices of the triangle, counter-clockwise (a, b, c)
 * return: the barycenter of the given triangle
 * */
Vector2f barycenter(Vector2f a, Vector2f b, Vector2f c){
    float a_x = a(0), a_y = a(1);
    float b_x = b(0), b_y = b(1);
    float c_x = c(0), c_y = c(1);
    float barycenter_x = (a_x + b_x + c_x) / 3;
    float barycenter_y = (a_y + b_y + c_y) / 3;
    Vector2f barycenter(barycenter_x, barycenter_y);
    return barycenter;
}

int main(void) {
    GLFWwindow* window; // Initializing the window
    if (!glfwInit()){// Initialize the library
        return -1;
    }

    // Activate supersampling
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    // On apple we have to load a core profile with forward compatibility
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(921, 921, "2D-Vector-Graphics-Editor", NULL, NULL);
    if (!window){
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);// Make the window's context current

    #ifndef __APPLE__
      glewExperimental = true;
      GLenum err = glewInit();
      if(GLEW_OK != err){
        /* Problem: glewInit failed, something is seriously wrong. */
       fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
      }
      glGetError();
      fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
    #endif

    int major, minor, rev;
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    rev = glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
    printf("OpenGL version recieved: %d.%d.%d\n", major, minor, rev);
    printf("Supported OpenGL is %s\n", (const char*)glGetString(GL_VERSION));
    printf("Supported GLSL is %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Initialize the VAO
    VertexArrayObject VAO;
    VAO.init();
    VAO.bind();

    VBO.init(); // Initialize the VBO with the initial vertices data
    VBO_1.init(); // Initialize the VBO with the initial vertices data
    V.resize(2,3);
    V << 0,  0.5, -0.5, 0.5, -0.5, -0.5;
    VBO.update(V);
    VBO_1.update(V);
    VBO_C.init();// Initialize the second VBO for the property color
    C.resize(3,3);
    C << 1,  0, 0, 0,  1, 0, 0,  0, 1;
    VBO_C.update(C);

    // Initialize the OpenGL Program
    Program program;
    const GLchar* vertex_shader =
            "#version 150 core\n"
                    "in vec2 position;"
                    "in vec2 position1;"
                    "in vec3 color;"
                    "uniform mat4 view;"
                    "uniform mat4 Translation;"
                    "uniform float delta;"
                    "out vec3 f_color;"
                    "vec4 newPosition;"
                    "void main()"
                    "{"
                    "    vec2 intermediatePos = position;"
                    "    if(delta >= 0.0f) intermediatePos = (position1 - position) * delta;"
                    "    gl_Position = view * Translation * vec4(intermediatePos, 0.0, 1.0);"
                    "    f_color = color;"
                    "}";
    const GLchar* fragment_shader =
            "#version 150 core\n"
                    "in vec3 f_color;"
                    "out vec4 outColor;"
                    "uniform vec3 triangleColor;"
                    "void main()"
                    "{"
                    "    outColor = vec4(f_color, 1.0);"
                    "}";

    // Compile the two shaders and upload the binary to the GPU
    program.init(vertex_shader,fragment_shader,"outColor");
    program.bind();

    // Inputs of vertex shader: vertices and any other properties
    program.bindVertexAttribArray("position",VBO);
    program.bindVertexAttribArray("position1",VBO_1);
    program.bindVertexAttribArray("color",VBO_C);

    // Save the current time --- it will be used to dynamically change the triangle color
    auto t_start = std::chrono::high_resolution_clock::now();

    // Register the keyboard and mouse callback
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Update viewport
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Loop until closing the current window
    while (!glfwWindowShouldClose(window)){
        VAO.bind();
        program.bind();

        // Set the uniform value of color decpending on the time difference
        auto t_now = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration_cast<std::chrono::duration<float>>(t_now - t_start).count();
        glUniform1f(program.uniform("delta"), -1.0f);
        float redPercent = 0.0f, greenPercent = (float)(sin(time * 4.0f) + 1.0f) / 2.0f, bluePercent = (float)(sin(time * 4.0f) + 1.0f) / 2.0f;
        glUniform3f(program.uniform("triangleColor"), redPercent, greenPercent, bluePercent);

        // Initialize the View Matrix adjusting as window size changing if the view control DISABLED
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        float aspect_ratio = float(height)/float(width);
        view << aspect_ratio,0, 0, 0,
                0,           1, 0, 0,
                0,           0, 1, 0,
                0,           0, 0, 1;
        glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, view.data());

        // Initialize the Translation Matrix as an identity matrix if the triangle is NOT selected
        trs << 1, 0, 0, 0,
               0, 1, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1;
        glUniformMatrix4fv(program.uniform("Translation"), 1, GL_FALSE, trs.data());

        // Clear the FrameBuffer
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Enable blending test
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//        glEnable(GL_DEPTH_TEST);// Enable Depth Test: (side effect) will disable the highlight effects

        if(clicks == 0){ // Draw an initial triangle
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }else if (clicks > 0){
            glDrawArrays(GL_TRIANGLES, 0, clicks);
        }

        if(mode.modeI == true){
            if(clicks % 3 == 1){// The first click: draw a starting point, then preview a line segment
                glUniform3f(program.uniform("triangleColor"), 1.0f, 1.0f, 1.0f);
                C.col(clicks - 1) << 1.0f, 1.0f, 1.0f;
                VBO_C.update(C);
                glDrawArrays(GL_POINTS, clicks - 1, 1);

                // Get the position of the mouse in the window to generate preview
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                int w, h;
                glfwGetWindowSize(window, &w, &h);
                double xworld = ((xpos/double(width))*2)-1, yworld = (((height-1-ypos)/double(height))*2)-1;
                Eigen::Vector2f cur(xworld, yworld);

                // Preview a line segment
                if(!Vertex.col(clicks).isApprox(cur)){
                    Vertex.conservativeResize(2, clicks + 1);
                    Vertex.col(clicks) << (float)xworld, (float)yworld;
                    VBO.update(Vertex);
                    glUniform3f(program.uniform("triangleColor"), 1.0f, 1.0f, 1.0f);
                    C.col(clicks) << 1.0f, 1.0f, 1.0f;
                    VBO_C.update(C);
                    glDrawArrays(GL_LINE_STRIP, clicks - 1, 2);
                }
            }

            if(clicks % 3 == 2){// The second click: draw a line segment, then preview a triangle
                glUniform3f(program.uniform("triangleColor"), 1.0f, 1.0f, 1.0f);
                C.col(clicks - 1) << 1.0f, 1.0f, 1.0f;
                C.col(clicks - 2) << 1.0f, 1.0f, 1.0f;
                VBO_C.update(C);
                glDrawArrays(GL_LINE_STRIP, clicks - 1, 2);
                glDrawArrays(GL_LINE_STRIP, clicks - 2, 2);

                // Get the position of the mouse in the window to generate preview
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                int w, h;
                glfwGetWindowSize(window, &w, &h);
                double xworld = ((xpos/double(width))*2)-1, yworld = (((height-1-ypos)/double(height))*2)-1;
                Eigen::Vector2f cur(xworld, yworld);

                // Preview a triangle
                if(!Vertex.col(clicks).isApprox(cur)){
                    Vertex.conservativeResize(2, clicks + 1);
                    Vertex.col(clicks) << (float)xworld, (float)yworld;
                    VBO.update(Vertex);
                    C.col(clicks) << 1.0f, 1.0f, 1.0f;
                    VBO_C.update(C);
                    glDrawArrays(GL_LINE_LOOP, clicks - 2, 3);
                }
            }

            if(clicks != 0 && clicks % 3 == 0){// The third click: draw a triangle
                glDrawArrays(GL_TRIANGLES, clicks - 3, 3);
                glUniform3f(program.uniform("triangleColor"), 0.0f, (float)(sin(time * 4.0f) + 1.0f) / 2.0f, (float)(sin(time * 4.0f) + 1.0f) / 2.0f);
                for(int i = 0; i < C.cols(); i++){
                    if(i % 3 == 0){
                        C.col(i) << 0, 0, 0;
                    }else if(i % 3 == 1){
                        C.col(i) << 0, (float)(sin(time * 4.0f) + 1.0f) / 2.0f, 0;
                    }else if(i % 3 == 2){
                        C.col(i) << 0, 0, (float)(sin(time * 4.0f) + 1.0f) / 2.0f;
                    }
                }
                VBO_C.update(C);
                glDrawArrays(GL_LINE_LOOP, clicks - 3, 3);
                insertion = clicks / 3; // Figure out the total number of inserted triangles
            }
            glDrawArrays(GL_TRIANGLES, 0, clicks);
        }// end of Triangle Insertion Mode

        if(mode.modeO == true) {
            glDrawArrays(GL_TRIANGLES, 0, Vertex.cols());

            // Highlight the selected triangle
            int index = selectTri*3;
            glUniform3f(program.uniform("triangleColor"), 1.0f, 1.0f, 0.0f);
            for(int i = 0; i < Vertex.cols(); i++){
                if(i == index || i == index+1 || i == index+2){
                    C.col(i) << 1, 1, 0;
                }else{
                    if(i % 3 == 0){
                        C.col(i) << redPercent, 0, 0;
                    }else if(i % 3 == 1){
                        C.col(i) << 0, greenPercent, 0;
                    }else if(i % 3 == 2){
                        C.col(i) << 0, 0, bluePercent;
                    }
                }
            }
            VBO_C.update(C);
            glDrawArrays(GL_TRIANGLES, index, 3);
        }// end of Triangle Translation Mode

        if(mode.modeP == true) {
            for(int i = 0; i < C.cols(); i++){
                if(i % 3 == 0){
                    C.col(i) << redPercent, 0, 0;
                }else if(i % 3 == 1){
                    C.col(i) << 0, greenPercent, 0;
                }else if(i % 3 == 2){
                    C.col(i) << 0, 0, bluePercent;
                }
            }
            VBO_C.update(C);
            glDrawArrays(GL_TRIANGLES, 0, Vertex.cols());
        }// end of Triangle Deletion Mode

        if(mode.modeH == true || mode.modeJ == true || mode.modeK == true || mode.modeL == true) {
            for(int i = 0; i < C.cols(); i++){
                if(i % 3 == 0){
                    C.col(i) << redPercent, 0, 0;
                }else if(i % 3 == 1){
                    C.col(i) << 0, greenPercent, 0;
                }else if(i % 3 == 2){
                    C.col(i) << 0, 0, bluePercent;
                }
            }
            VBO_C.update(C);
            glDrawArrays(GL_TRIANGLES, 0, Vertex.cols());

            // Highlight the selected triangle
            int index_rot = rotTri*3;
            glUniform3f(program.uniform("triangleColor"), (float)(205.0/255.0), (float)(133.0/255.0), (float)(63.0/255.0));

            for(int i = 0; i < Vertex.cols(); i++){
                if(i == index_rot || i == index_rot+1 || i == index_rot+2){
                    C.col(i) << (float)(205.0/255.0), (float)(133.0/255.0), (float)(63.0/255.0);
                }else{
                    if(i % 3 == 0){
                        C.col(i) << redPercent, 0, 0;
                    }else if(i % 3 == 1){
                        C.col(i) << 0, greenPercent, 0;
                    }else if(i % 3 == 2){
                        C.col(i) << 0, 0, bluePercent;
                    }
                }
            }
            VBO_C.update(C);

            // Make rotation and scaling are done around the barycenter
            curBarycenter = barycenter(Vertex.col(index_rot), Vertex.col(index_rot+1), Vertex.col(index_rot+2));
            trsToOri << 1, 0, 0, -curBarycenter(0),
                        0, 1, 0, -curBarycenter(1),
                        0, 0, 1, 0,
                        0, 0, 0, 1;
            trsBack <<  1, 0, 0, curBarycenter(0),
                        0, 1, 0, curBarycenter(1),
                        0, 0, 1, 0,
                        0, 0, 0, 1;

            // Update the transformation matrix: transformation done in the vertex shader
            if(mode.modeH == true){
                rotation << cos(clockwise), -sin(clockwise), 0, 0,
                            sin(clockwise), cos(clockwise),  0, 0,
                            0,              0,               1, 0,
                            0,              0,               0, 1;
                trs <<  trsBack * rotation * trsToOri;
            }else if (mode.modeJ == true){
                rotation << cos(counter_clockwise), -sin(counter_clockwise), 0, 0,
                            sin(counter_clockwise), cos(counter_clockwise),  0, 0,
                            0,                      0,                       1, 0,
                            0,                      0,                       0, 1;
                trs <<  trsBack * rotation * trsToOri;
            }else if(mode.modeK == true){
                scaling <<  scale_up,   0,           0, 0,
                            0,          scale_up,    0, 0,
                            0,          0,           1, 0,
                            0,          0,           0, 1;
                trs <<  trsBack * scaling * trsToOri;
            }else if (mode.modeL == true){
                scaling <<  scale_down, 0,           0, 0,
                            0,          scale_down,  0, 0,
                            0,          0,           1, 0,
                            0,          0,           0, 1;
                trs <<  trsBack * scaling * trsToOri;
            }
            glUniformMatrix4fv(program.uniform("Translation"), 1, GL_FALSE, trs.data());
            glDrawArrays(GL_TRIANGLES, index_rot, 3); // Re-draw the transformed triangle
        }// end of Triangle Rotation & Scaling Mode

        if(mode.modeC == true) { // OpenGl uses barycentric coordinates for interpolation
            glDrawArrays(GL_TRIANGLES, 0, Vertex.cols());
            Vector3f newColor = newColors.col(recoloring - 1);

            if(recoloring != 10){ // New color of the closer vertex should be linearly interpolated inside the triangle
                glUniform3f(program.uniform("triangleColor"), newColor(0), newColor(1), newColor(2));
                glDrawArrays(GL_TRIANGLES, (recolorVer/3) * 3, 3);

                for(int i = 0; i < C.cols(); i++){
                    if(i == recolorVer){
                        C.col(i) << newColor(0), newColor(1), newColor(2);
                    }else{
                        if(i % 3 == 0){
                            C.col(i) << redPercent, 0, 0;
                        }else if(i % 3 == 1){
                            C.col(i) << 0, greenPercent, 0;
                        }else if(i % 3 == 2){
                            C.col(i) << 0, 0, bluePercent;
                        }
                    }
                }
                VBO_C.update(C);

            }
        }// end of Triangle Coloring Mode

        if(mode.modeW == true || mode.modeA == true || mode.modeS == true || mode.modeD == true || mode.modeMinus == true || mode.modePlus == true) {
            float redPercent = (float)(sin(time * 4.0f) + 1.0f) / 2.0f, greenPercent = (float)(sin(time * 4.0f) + 0.85f) / 2.0f, bluePercent = (float)(sin(time * 4.0f) + 0.73f) / 2.0f;
            glUniform3f(program.uniform("triangleColor"), redPercent, greenPercent, bluePercent);

            for(int i = 0; i < C.cols(); i++){
                if(i % 3 == 0){
                    C.col(i) << redPercent, 0, 0;
                }else if(i % 3 == 1){
                    C.col(i) << 0, greenPercent, 0;
                }else if(i % 3 == 2){
                    C.col(i) << 0, 0, bluePercent;
                }
            }
            VBO_C.update(C);

            int width, height;
            glfwGetWindowSize(window, &width, &height);
            float panReplacement_x = viewCtrl * width, panReplacement_y = viewCtrl * height;
            // Update the transformation matrix: transformation done in the vertex shader
            if(mode.modeW == true){ // down
                pan <<  1, 0, 0, 0,
                        0, 1, 0, (float)(((panReplacement_y/double(height))*2)-1),
                        0, 0, 1, 0,
                        0,  0, 0, 1;
                view = pan * view;
            }else if (mode.modeA == true){ // right
                pan << 1, 0, 0, -(float)(((panReplacement_x/double(width))*2)-1),
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1;
                view = pan * view;
            }else if(mode.modeS == true){ // up
                pan <<  1, 0, 0, 0,
                        0, 1, 0, -(float)(((panReplacement_y/double(height))*2)-1),
                        0, 0, 1, 0,
                        0, 0, 0, 1;
                view = pan * view;
            }else if (mode.modeD == true){ // left
                pan << 1, 0, 0, (float)(((panReplacement_x/double(width))*2)-1),
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1;
                view = pan * view;
            }else if (mode.modeMinus == true){ // zoom in 20%
                zoomio << 1.0f-viewCtrl,   0,                 0, 0,
                          0,               1.0f-viewCtrl,     0, 0,
                          0,               0,                 1, 0,
                          0,               0,                 0, 1;
                view = zoomio * view;
            }else if(mode.modePlus == true){ // zoom out 20%
                zoomio << 1.0f+viewCtrl,   0,                 0, 0,
                          0,               1.0f+viewCtrl,     0, 0,
                          0,               0,                 1, 0,
                          0,               0,                 0, 1;
                view = zoomio * view;
            }
            glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, view.data());
            glDrawArrays(GL_TRIANGLES, 0, Vertex.cols()); // Re-draw the whole scene
        }// end of View Control Mode

        if(mode.keyFraming == true){
            // Set up the interpolation factor
            auto t_now2 = std::chrono::high_resolution_clock::now();
            float curDuration = std::chrono::duration_cast<std::chrono::duration<float>>(t_now2 - t_start).count();
            glUniform1f(program.uniform("delta"), (float)(sin(curDuration * 4.0f) + 1.0f) / 2.0f);
            // Prepare for a new frame
            zoomio << 1.0f+viewCtrl,   0,                 0, 0,
                      0,               1.0f+viewCtrl,     0, 0,
                      0,               0,                 1, 0,
                      0,               0,                 0, 1;
            rotation << cos(clockwise), -sin(clockwise), 0, 0,
                        sin(clockwise), cos(clockwise),  0, 0,
                        0,              0,               1, 0,
                        0,              0,               0, 1;
            //Update all states
            for(int i = 0; i < Vertex.cols(); i++){
                NextVertex.conservativeResize(4, 4+i);
                NextVertex.col(i) << Vertex.col(i), 0.0f, 1.0f;
            }
            VBO_1.update(rotation * zoomio * NextVertex);
            for(int i = 0; i < C.cols(); i++){
                if(i % 3 == 0){
                    C.col(i) << redPercent, 0, 0;
                }else if(i % 3 == 1){
                    C.col(i) << 0, greenPercent, 0;
                }else if(i % 3 == 2){
                    C.col(i) << 0, 0, bluePercent;
                }
            }
            VBO_C.update(C);
            glDrawArrays(GL_TRIANGLES, 0, Vertex.cols()); // Re-draw the whole scene
        }// end of Keyframing Animation Mode

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    std::cout << "Triangle Inserted: " << insertion << std::endl;
    std::cout << "Triangle Deleted: " << deletion << std::endl;
    std::cout << "Current # of Triangles:" << insertion - deletion << std::endl;

    program.free();
    VAO.free();
    VBO.free();
    VBO_1.free();
    VBO_C.free();
    glfwTerminate();

    return 0;
}
