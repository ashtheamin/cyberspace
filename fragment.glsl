#extension GL_OES_standard_derivatives: enable

uniform highp vec3 object_color;
uniform highp vec3 light_color;
uniform highp vec3 light_position;

varying highp vec3 fragment_position;
varying highp vec4 fragment_color;
varying highp vec3 fragment_normal;

void main(void) {
    // Flat shading code
    //highp vec3 x_tangent = dFdx(fragment_position.xyz);
    //highp vec3 y_tangent = dFdy(fragment_position.xyz);
    //highp vec3 face_normal = normalize(cross(x_tangent, y_tangent));
    //highp vec3 norm = normalize(face_normal);

    // Else use passed in normal to use smooth shading
    highp vec3 norm = normalize(fragment_normal);


    highp float ambient_strength = 0.5;
    highp vec3 ambient = ambient_strength * light_color;
    highp vec3 light_direction = normalize(light_position - fragment_position);
    highp float diff = max(dot(norm, light_direction), 0.0);
    highp vec3 diffuse = diff * light_color;
    highp vec3 result = (ambient + diffuse) * fragment_color.xyz;

    
    gl_FragColor = vec4(result, fragment_color.w);
}