#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Program status variables
#define RUNNING 1
#define QUIT 0

// Screen dimensions
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Shader attributes.
struct attributes {
    GLint position;
    GLint fragment_position;
    GLint vertex_color;
    GLint fragment_color;
    GLint vertex_normal;
    GLint fragment_normal;
};

// Shader uniforms
struct uniforms {
    GLint model;
    GLint view;
    GLint projection;
    GLint light_color;
    GLint light_position;
    GLint camera_position;
};

// Store mouse data
struct mouse {
    float last_x;
    float last_y;
    float sensitivity;
    bool used_before;
};

// Store camera data
struct camera {
    vec3 position;
    vec3 front;
    vec3 up;

    float speed;
    float yaw;
    float pitch;

    struct mouse mouse;
};

// Store timing data to render the scene smoothly.
struct timing {
    float delta_time;
    float last_time;
};

// Store light data.
struct light {
    vec3 light_color;
    vec3 light_position;
};

// Vertex data
struct vertex {
    vec3 position;
    vec4 vertex_color;
    vec3 normal;
};

// A shader program struct.
// This stores the link to a compiled shader program as well as attributes and uniforms.
struct shader {
    GLuint shader;
    struct attributes attributes;
    struct uniforms uniforms;
    struct shader* next;
};

// A mesh of verticies, vertex colours and faces.
// It also contains data for VBO, EBO and VAO so OpenGL can load the data into GPU and process it.
struct mesh {
    struct vertex *vertices;
    unsigned int num_vertices;
    GLushort *indices;
    unsigned int num_indices;
    GLuint VBO;
    GLuint EBO;
    GLuint VAO;
    struct mesh* next;
};

// A template to create objects.
// This stores mesh information, world position, size, rotation and a link to the next object.
struct object {
    char* name;
    struct mesh* meshes;
    vec3 position;
    vec3 scale;
    float rotation;
    struct object* next;
};

// The global program state.
// Contained within it are all lists and necessary data for the scene.
struct program {
    GLFWwindow* window;
    struct camera camera;
    struct timing timing;
    int status;
    struct object* objects;
    struct light light;
    struct shader* shaders;
    GLuint shader;
    bool opengl_initialised;
};

// A pointer to the globla state on the heap.
struct program* program = NULL;

// Open a file, read all the lines, and return a heap allocated string that contains all the characters in the line.
// This function supports up to 4096 characters per line to be read.
// All resources will be freed and NULL returned if there is an error in any stage of the process.
char* helper_file_to_string(char* filename) {
    // Return on filename is NULL
    if (filename == NULL) {
        printf("file_to_string(): Filename is NULL. Returning NULL.\n");
        return NULL;
    }

    // Open the file
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("file_to_string(): Failed to open '%s'. Returning NULL.\n", filename);
        return NULL;
    }

    // Create the buffer
    char* buffer = malloc(sizeof(char) * 4096 + 1);
    if (buffer == NULL) {
        printf("file_to_string(): malloc() failed to allocate memory for read buffer. Returning NULL.\n");
        fclose(fp);
        return NULL;
    }

    // Calculate the size of the string we will want to put on the heap.
    size_t filesize = 0;
    while (fgets(buffer, 4096, fp) != NULL) {
        filesize = filesize + sizeof(char) * strlen(buffer) + 1;
    }

    // Close the file.
    fclose(fp);

    // Open the file again
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("file_to_string(): Failed to open '%s'. Returning NULL.\n", filename);
        free(buffer);
        return NULL;
    }

    // Allocate the memory for the file onto the heap
    char* file_string = malloc(filesize + 1);
    if (file_string == NULL) {
        printf("file_to_string(): malloc() failed to allocate memory for file_string. Returning NULL.\n");
        free(buffer);
        return NULL;
    }

    // Copy the file contents to the file_string heap pointer.
    strcpy(file_string, "");
    strcpy(buffer, "");

    while (fgets(buffer, 4096, fp) != NULL) {
        strncat(file_string, buffer, 4096);
    }

    free(buffer);
    fclose(fp);
    
    return file_string;
}

// Print an OpenGL Log for an object:
void helper_opengl_print_log(GLuint object) {
    GLint log_length = 0;

    bool is_shader = false;
    bool is_program = false;

    if (glIsShader(object) == GL_TRUE) {
        is_shader = true;
        glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
    }

    if (glIsProgram(object) == GL_TRUE) {
        is_program = true;
        glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
    }

    char* log = malloc(log_length * sizeof(char));

    if (is_shader == true) {
        glGetShaderInfoLog(object, log_length, NULL, log);
    }

    if (is_program == true) {
        glGetProgramInfoLog(object, log_length, NULL, log);
    }

    printf("OpenGL Log:\n%sOpenGL End Log.\n", log);
    free(log);
    return;
}

// Read a shader file and compile a shader.
// Free resources and return 0 on failure.
GLuint helper_opengl_create_shader(char* filename, GLenum type) {
    // Load the source to the shader
    const GLchar* source = helper_file_to_string(filename);
    if (source == NULL) {
        printf("helper_opengl_print_log(): Failed to read shader file from file_to_string(). Returning.\n");
        return 0;
    }

    // Create the shader
    GLuint shader = glCreateShader(type);

    // Configure and load shader source code.
    const GLchar* sources[] = {
        "#version 100\n", source
    };

    glShaderSource(shader, 2, sources, NULL);

    // Free opened shader source file
    free((void*) source);

    // Compile shader.
    glCompileShader(shader);
    GLint compile_status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    // Print error and return 0 on error.
    if (compile_status == GL_FALSE) {
        printf("Failed to create '%s' shader\n", filename);
        helper_opengl_print_log(shader);
        glDeleteShader(shader);
        return 0;
    }

    // Return compiled shader.
    return shader;
}

// Create an object instance by loading a mesh and initialising the VBO and VAO and vertex information.
struct object* object_new(char* object_filename) {
    // Allocate memory for the object. 
    struct object* object = malloc(sizeof(struct object));
    if (object == NULL) {
        printf("object_new(): Failed to allocate memory for object. Exiting.\n");
        exit(-1);
    }
    // Store the name of the object.
    object->name = strdup(object_filename);
    if (object->name == NULL) {
        printf("object_new(): Failed to allocate memory for object name. Exiting.\n");
        exit(-1);
    }

    // Initialise the mesh
    object->meshes = malloc(sizeof(struct mesh));
    if (object->meshes == NULL) {
        exit(-1);
    }
    struct mesh* mesh = object->meshes;

    // Open the file and calculate how many bytes to allocate for the mesh.
    // Do this by counting how many vertices and indices there are.
    // Vertices are denoted by a v before the line, and indicie faces start with an f.
    FILE* obj_file = fopen(object_filename, "r");
    if (obj_file == NULL) {
        printf("object_new(): Failed to open file '%s'. Exiting.\n", object_filename);
        exit(-1);
    }
    size_t num_vertices = 0;
    size_t num_indices = 0;

    char* buffer = malloc(sizeof(char) * 4096);
    if (buffer == NULL) exit(-1);
    strcpy(buffer, "");

    while (fgets(buffer, 4096, obj_file) != NULL) {
        if (buffer[0] == 'v' && buffer[1] == ' ') num_vertices++;
        if (buffer[0] == 'f') num_indices++;
    }

    mesh->num_vertices = num_vertices;
    mesh->num_indices = num_indices;

    if (num_vertices < 1 || num_indices < 1) {
        printf("object_new(): There are no verticies and/or faces detected. Returning.");
        exit(-1);
    }

    rewind(obj_file);
    strcpy(buffer, "");

    // Allocate the vertex and index array:
    mesh->vertices = malloc(sizeof(struct vertex) * num_vertices);
    if (mesh->vertices == NULL) exit(-1);

    mesh->indices = malloc(sizeof(unsigned int) * num_indices);
    if (mesh->indices == NULL) exit(-1);

    size_t vertices_index = 0;
    size_t indices_index = 0;

    // Copy the indicies and verticies into their arrays
    while (fgets(buffer, 4096, obj_file) != NULL) {
        // Load vertex positions and colors
        if (buffer[0] == 'v' && buffer[1] == ' ') {
            sscanf(buffer, "v %f %f %f %f %f %f %f %f %f %f", 
            &mesh->vertices[vertices_index].position[0],
            &mesh->vertices[vertices_index].position[1],
            &mesh->vertices[vertices_index].position[2],
            &mesh->vertices[vertices_index].vertex_color[0],
            &mesh->vertices[vertices_index].vertex_color[1],
            &mesh->vertices[vertices_index].vertex_color[2],
            &mesh->vertices[vertices_index].vertex_color[3],
            &mesh->vertices[vertices_index].normal[0],
            &mesh->vertices[vertices_index].normal[1],
            &mesh->vertices[vertices_index].normal[2]);
            vertices_index++;
        }

        // Load list of faces
        if (buffer[0] == 'f') {
            sscanf(buffer, "f %hd", &mesh->indices[indices_index]);
            indices_index++;
        }
    }
    // Close the mesh data file
    fclose(obj_file);
    free(buffer);
    mesh->next = NULL;

    // Load the mesh into the GPU and store a vertex array object pointer for future access to the mesh
    while (mesh != NULL) {
        // Initialise the VAO which will be used later to tell the GPU where the mesh is.
        glBindVertexArray(mesh->VAO);
        glGenVertexArrays(1, &mesh->VAO);

        // LOad the mesh into the GPU
        glGenBuffers(1, &mesh->VBO);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
        glBindVertexArray(mesh->VAO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex) * mesh->num_vertices, mesh->vertices, GL_STATIC_DRAW);

        // Load the vertex positions into GPU and into the positions attribute
        glEnableVertexAttribArray(program->shaders->attributes.position);
        glVertexAttribPointer(program->shaders->attributes.position, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)0);

        // Load the vertex colors into GPU and into the colors attribute
        glEnableVertexAttribArray(program->shaders->attributes.vertex_color);
        glVertexAttribPointer(program->shaders->attributes.vertex_color, 4, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, vertex_color));

        // Load the vertex normals into GPU and into the colors attribute
        glEnableVertexAttribArray(program->shaders->attributes.vertex_normal);
        glVertexAttribPointer(program->shaders->attributes.vertex_normal, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, normal));
        
        // Load the indices to form the triangle faces.
        glGenBuffers(1, &mesh->EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * mesh->num_indices, mesh->indices, GL_STATIC_DRAW);
        glBindVertexArray(0);

        // Load the next mesh if applicable
        mesh = mesh->next;
    }
    

    // Set object position, scale and rotation
    glm_vec3_copy((vec3){0.0, 0.0, 0.0}, object->position);
    glm_vec3_copy((vec3){1.0, 1.0, 1.0}, object->scale);
    object->rotation = glm_rad(90.0);

    object->next = NULL;
    return object;
}

// Resize the scene when window is resized
void program_resize_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glfwSetInputMode(program->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glViewport(0, 0, width, height);
}

// Move camera when mouse to point at a new location when mouse is moved.
void program_mouse_callback(GLFWwindow* window, double x, double y) {
    (void)window;
    // Set the last x and y mouse positions
    if (program->camera.mouse.used_before == false) {
        program->camera.mouse.used_before = true;
        program->camera.mouse.last_x = x;
        program->camera.mouse.last_y = y;
    }

    // Get the difference between the last mouse position and current
    float x_offset = x - program->camera.mouse.last_x;
    float y_offset = y - program->camera.mouse.last_y;

    // Store the current mouse positions to be the last
    program->camera.mouse.last_x = x;
    program->camera.mouse.last_y = y;

    // Multiply the difference in mouse position with sensitivty
    x_offset = x_offset * program->camera.mouse.sensitivity;
    y_offset = y_offset * program->camera.mouse.sensitivity;

    // Add the movement difference to the camera's viewing angles.
    program->camera.yaw = program->camera.yaw + x_offset;
    program->camera.pitch = program->camera.pitch - y_offset;

    // Lock the camera's pitch so it does not go too far up or down
    if (program->camera.pitch > 89.0) {
        program->camera.pitch = 89.0;
    }

    if (program->camera.pitch < -89.0) {
        program->camera.pitch = -89.0;
    }

    // Calculate the angles that the vector the camera is looking at needs to change with.
    vec3 direction = {0.0, 0.0, 0.0};
    direction[0] = cos(glm_rad(program->camera.yaw)) * cos(glm_rad(program->camera.pitch));
    direction[1] = sin(glm_rad(program->camera.pitch));
    direction[2] = sin(glm_rad(program->camera.yaw)) * cos(glm_rad(program->camera.pitch));

    // Add the movement difference to the vector the camera is looking at.
    glm_vec3_normalize_to(direction, program->camera.front);
}

// Initialise the program state
void program_init() {
    // Initialise the global program state
    program = malloc(sizeof(struct program));
    if (program == NULL) exit(-1);

    // Initialise GLFW
    if (glfwInit() == GLFW_FALSE) {
        printf("program_init(): Failed to initialise GLFW. Exit.\n");
        exit(-1);
    }

    // Create GLFW context.
    program->window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Window", NULL, NULL);
    glfwMakeContextCurrent(program->window);
    if (program->window == NULL) {
        printf("program_init(): Failed to create GLFW window. Exit.\n");
        exit(-1);
    }

    // Set OpenGL hints.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialise GLEW
    GLenum glew_status = glewInit();
    if (glew_status != GLEW_OK) {
        printf("program_init(): glewInit() failed to initailise. glewGetErrorString(): %s. Exiting.\n", glewGetErrorString(glew_status));
        exit(-1);
    }

    // Ensure the VAO is available as this program relies on it.
    if (!GL_ARB_vertex_array_object) {
        printf("program_init(): glewInit() failed to initailise. glewGetErrorString(): %s. Exiting.\n", glewGetErrorString(glew_status));
        exit(-1);
    }

    // Initialise the shader program.
    program->shaders = malloc(sizeof(struct shader));
    if (program->shaders == NULL) {
        printf("program_init(): Failed to initialise shader program. Exit.\n");
        exit(-1);
    }
    program->shaders->next = NULL;

    GLuint vertex_shader = helper_opengl_create_shader("vertex.glsl", GL_VERTEX_SHADER);
    GLuint fragment_shader = helper_opengl_create_shader("fragment.glsl", GL_FRAGMENT_SHADER);

    program->shaders->shader = glCreateProgram();
    glAttachShader(program->shaders->shader, vertex_shader);
    glAttachShader(program->shaders->shader, fragment_shader);
    glLinkProgram(program->shaders->shader);
    if (program->shaders->shader == (GLuint)-1) {
        printf("program_init(): Failed to create valid shader. Exit.\n");
        exit(-1);
    }

    // Initialise attributes:
    program->shaders->attributes.position = glGetAttribLocation(program->shaders->shader, "position");
    program->shaders->attributes.fragment_position = glGetAttribLocation(program->shaders->shader, "fragment_position");
    program->shaders->attributes.vertex_color = glGetAttribLocation(program->shaders->shader, "vertex_color");
    program->shaders->attributes.fragment_color = glGetAttribLocation(program->shaders->shader, "fragment_color");
    program->shaders->attributes.vertex_normal = glGetAttribLocation(program->shaders->shader, "vertex_normal");
    program->shaders->attributes.fragment_normal = glGetAttribLocation(program->shaders->shader, "fragment_normal");

    // Initialise uniforms:
    program->shaders->uniforms.view = glGetUniformLocation(program->shaders->shader, "view");
    program->shaders->uniforms.model = glGetUniformLocation(program->shaders->shader, "model");
    program->shaders->uniforms.projection = glGetUniformLocation(program->shaders->shader, "projection");
    program->shaders->uniforms.light_color = glGetUniformLocation(program->shaders->shader, "light_color");
    program->shaders->uniforms.light_position = glGetUniformLocation(program->shaders->shader, "light_position");
    program->shaders->uniforms.camera_position = glGetUniformLocation(program->shaders->shader, "camera_position");
    
    // Initialise light:
    glm_vec3((vec3){1.0, 1.0, 1.0}, program->light.light_color);
    glm_vec3((vec3){0.0, -3.0, 0.0}, program->light.light_position);
    

    // Load object mesh:
    program->objects = object_new("output_model");
    if (program->objects == NULL) {
        printf("program_init(): Failed to load object. Returning.\n");
        exit(-1);
    }

    // Initialise camera:
    memcpy(program->camera.position, (vec3){-2.0, -13.0, 10.0}, sizeof(vec3));
    memcpy(program->camera.front, (vec3){0.0, 0.0, -1.0}, sizeof(vec3));
    memcpy(program->camera.up, (vec3){0.0, 1.0, 0.0}, sizeof(vec3));
    program->camera.yaw = -90.0;
    program->camera.pitch = 0.0;
    program->camera.speed = 5;

    program->camera.mouse.last_x = SCREEN_WIDTH/2;
    program->camera.mouse.last_y = SCREEN_HEIGHT/2;
    program->camera.mouse.used_before = false;
    program->camera.mouse.sensitivity = 0.1;
 
    // Initialise timings
    program->timing.delta_time = 0.0;
    program->timing.last_time = 0.0;

    // Set program status:
    program->status = RUNNING;
    program->opengl_initialised = false;

    // Set resize callback:
    glfwSetFramebufferSizeCallback(program->window, program_resize_callback);

    // Set mouse callback:
    glfwSetCursorPosCallback(program->window, program_mouse_callback);
    glfwSetInputMode(program->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

// Render the objects in the program.
void program_render() {
    // Initialise OpenGL elements
    if (program->opengl_initialised == false) {
        glClearColor(0.52, 0.80, 0.92, 0.0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        // Load program shader
        glUseProgram(program->shaders->shader);
        program->opengl_initialised = true;
    }
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // Go through the list of objects to render
    struct object* object = program->objects;
    while (object != NULL) {
        // Initialise world transformation matricies.
        // Model moves/translates objects to the correct location in the world.
        // View moves objects in front of a camera.
        // Projection defines how objects appear to the camera to create proper depth and perspective.
        mat4 model = GLM_MAT4_IDENTITY_INIT;
        mat4 view = GLM_MAT4_IDENTITY_INIT;
        mat4 projection = GLM_MAT4_IDENTITY_INIT;

        // Set object's scale and position to the model matrix.
        glm_translate(model, object->position);
        glm_scale(model, object->scale);

        // Create the view and camera matrix.
        // Make sure the view takes data from the current camera position and what it is looking at to know how to draw things
        vec3 lookingat = {0.0, 0.0, 0.0};
        glm_vec3_add(program->camera.position, program->camera.front, lookingat);
        glm_lookat(program->camera.position, lookingat, program->camera.up, view);

        // Create the world light position to pass into the shader.
        //glm_vec3_copy(program->camera.position, program->light.light_position);
        glm_vec3_copy((vec3){0.0, 1000.0, 1000.0}, program->light.light_position);

        // Create the camera perspective
        glm_perspective(glm_rad(45.0f), (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT, 0.1f, 1000.f, projection);
        
        // Copy information on light, camera position and uniforms to the shader
        // Also copy transformation matricies for vertex positions to the shader for processing.
        // This ensures that vertices then appear on the screen from our camera's perspective correctly.
        glUniform3fv(program->shaders->uniforms.light_color, 1, program->light.light_color);
        glUniform3fv(program->shaders->uniforms.light_position, 1, program->light.light_position);
        glUniform3fv(program->shaders->uniforms.camera_position, 1, program->camera.position);
        glUniformMatrix4fv(program->shaders->uniforms.model, 1, GL_FALSE, model[0]);
        glUniformMatrix4fv(program->shaders->uniforms.view, 1, GL_FALSE, view[0]);
        glUniformMatrix4fv(program->shaders->uniforms.projection, 1, GL_FALSE, projection[0]);

        // Go through the object's mesh list and draw the mesh
        struct mesh* mesh = object->meshes;
        while (mesh != NULL) {
            glBindVertexArray(mesh->VAO);
            glDrawElements(GL_TRIANGLES, mesh->num_indices, GL_UNSIGNED_SHORT, 0);
            mesh = mesh->next;
        }
        
        // Go to the next object if applicable
        object = object->next;
    }
    // Show the result on screen.
    glfwSwapBuffers(program->window);
}

// Calculate time between frames to get smooth movement
void program_update_timing() {
    float current_time = glfwGetTime();
    program->timing.delta_time = current_time - program->timing.last_time;
    program->timing.last_time = current_time;
}

// Updates the camera's position in a direction based on what key is hit to move it and the speed it should move at
void program_input() {
    float speed = program->camera.speed * program->timing.delta_time;
    vec3 updated_position = {0.0, 0.0, 0.0};

    // Transform the position the camera exists at by using speed as a factor 
    // as to how much it should change per keypress and in a direction based on the keypress
    if (glfwGetKey(program->window, GLFW_KEY_W) == GLFW_PRESS) {
        glm_vec3_scale(program->camera.front, speed, updated_position);
        glm_vec3_add(program->camera.position, updated_position, program->camera.position);
    }

    if (glfwGetKey(program->window, GLFW_KEY_S) == GLFW_PRESS) {
        glm_vec3_scale(program->camera.front, speed, updated_position);
        glm_vec3_negate(updated_position);
        glm_vec3_add(program->camera.position, updated_position, program->camera.position);
    }

    if (glfwGetKey(program->window, GLFW_KEY_A) == GLFW_PRESS) {
        glm_vec3_cross(program->camera.front, program->camera.up, updated_position);
        glm_vec3_normalize(updated_position);
        glm_vec3_scale(updated_position, speed, updated_position);
        glm_vec3_negate(updated_position);
        glm_vec3_add(updated_position, program->camera.position, program->camera.position);
    }

    if (glfwGetKey(program->window, GLFW_KEY_D) == GLFW_PRESS) {
        glm_vec3_cross(program->camera.front, program->camera.up, updated_position);
        glm_vec3_normalize(updated_position);
        glm_vec3_scale(updated_position, speed, updated_position);
        glm_vec3_add(updated_position, program->camera.position, program->camera.position);
    }
}

// This function is the main program loop function.
// It checks whether the program should close, and it scans for input and draws/updates the program and calls OpenGL to draw objects on the screen.
void program_loop() {
    if (glfwWindowShouldClose(program->window)) {
        program->status = QUIT;
    }
    program_update_timing();
    program_input();
    program_render();
    glfwPollEvents();
}

int main(void) {
    // Initialise the global state
    program_init();
    
    // Set the main loop for the web if using emscripten platform.
    #ifdef __EMSCRIPTEN__
        emscripten_set_main_loop(program_loop, 60, 1);
    #endif

    // Start the program main loop.
    while (program->status != QUIT) {
        program_loop();
    }
    
    // Close resources on exit.
    glfwTerminate();
    return 0;
}