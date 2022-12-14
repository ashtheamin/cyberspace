attribute highp vec3 position;
attribute highp vec3 vertex_normal;
attribute highp vec4 vertex_color;

varying highp vec3 fragment_position;
varying highp vec3 fragment_normal;
varying highp vec4 fragment_color;

uniform highp mat4 model;
uniform highp mat4 view;
uniform highp mat4 projection;

void main(void) {
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragment_position = vec3(model * vec4(position, 1.0));
    fragment_normal = vertex_normal;
    fragment_color = vertex_color;
}