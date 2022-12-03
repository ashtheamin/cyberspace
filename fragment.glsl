#extension GL_OES_standard_derivatives: enable

uniform highp vec3 object_color;
uniform highp vec3 light_color;
uniform highp vec3 light_position;
uniform highp vec3 camera_position;

varying highp vec3 fragment_position;
varying highp vec3 fragment_normal;
varying highp vec3 fragment_color;

void main(void) {
    highp vec3 x_tangent = dFdx(fragment_position.xyz);
    highp vec3 y_tangent = dFdy(fragment_position.xyz);
    highp vec3 face_normal = normalize(cross(x_tangent, y_tangent));

    highp vec3 norm = normalize(face_normal);
    
    highp float ambient_strength = 0.2;
    highp float specular_strength = 0.5;


    highp vec3 ambient = ambient_strength * light_color;
    //highp vec3 norm = normalize(fragment_normal);
    highp vec3 light_direction = normalize(light_position - fragment_position);
    highp float diff = max(dot(norm, light_direction), 0.0);
    highp vec3 diffuse = diff * light_color;
    
    highp vec3 view_direction = normalize(camera_position - fragment_position);
    highp vec3 reflect_direction = reflect(-light_direction, norm);
    highp float spec = pow(max(dot(view_direction, reflect_direction), 0.0), 32.0);

    highp vec3 specular = specular_strength * spec * light_color;

    highp vec3 color = fragment_color.xyz;
    highp vec3 result = (ambient + diffuse + specular) * color;

    gl_FragColor = vec4(result, 1.0);
}