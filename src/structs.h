#ifndef CG_STRUCTS
#define CG_STRUCTS

#include "glm/ext.hpp"

namespace cg
{

struct Version
{
    const int gl_major;
    const int gl_minor;
    const char* glsl_version;
};
extern const Version version;

struct Window
{
    const int window_width;
    const int window_height;
    const char* window_title;
};
extern const Window window;

struct Perspective
{
    float fov;
    float aspect;
    float z_near;
    float z_far;
};
extern Perspective perspective;

struct Camera
{
    glm::vec3 eye;
    glm::vec3 center;
    glm::vec3 up;
};
extern Camera camera;

struct Mesh
{
    unsigned int vao = 0;
    int vertex_count = 0;
};

struct Meshes
{
    Mesh cube;
    Mesh tower;
    Mesh nose;
    Mesh blade;
};

struct Textures
{
    unsigned int grass = 0;
    unsigned int metal = 0;
    unsigned int blade = 0;
    unsigned int windsock = 0;
    unsigned int lamp = 0;
};

struct OrbitCamera
{
    float angle = glm::radians(30.0f);
    float distance = 16.0f;
    float height = 3.0f;
};

struct Rotor
{
    float angle = 0.0f;
    float speed = 60.0f;
    float current_speed = 0.0f;
    bool running = true;
};

struct Wind
{
    float angle = 0.0f;
    float nacelle_yaw = 0.0f;
};

struct DayNight
{
    bool is_day = true;
    float daylight = 1.0f;
    glm::vec3 sky_color = glm::vec3(0.0f);
    float beacon = 0.0f;
};

struct Light
{
    glm::vec3 pos = glm::vec3(10.0f, 20.0f, 15.0f);
    glm::vec3 color = glm::vec3(1.0f);
};

struct Settings
{
    float orbit_speed = 60.0f;
    float zoom_speed = 5.0f;
    float cam_min_distance = 3.0f;
    float cam_max_distance = 25.0f;
    float rotor_speed_step = 20.0f;
    float rotor_speed_max = 300.0f;
    float rotor_acceleration = 40.0f;
    float wind_turn_speed = 45.0f;
    float yaw_speed = 20.0f;
    float day_night_speed = 0.5f;

    glm::vec3 sky_day = glm::vec3(0.55f, 0.75f, 0.95f);
    glm::vec3 sky_night = glm::vec3(0.02f, 0.03f, 0.08f);
    glm::vec3 light_day = glm::vec3(1.0f, 0.97f, 0.9f);
    glm::vec3 light_night = glm::vec3(0.2f, 0.25f, 0.45f);
};

} // namespace cg

#endif

