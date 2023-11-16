#version 330 core
precision mediump float;
layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 txt;
layout (location = 2) in vec4 col;

uniform mat4 u_mvp;

out vec2 v_text;
out vec4 v_col;

void main() {
    gl_Position = u_mvp * pos;
    v_text = txt;
    v_col = col;
}

# frag
#version 330 core
precision mediump float;
layout (location = 0) out vec4 color;

in vec2 v_text;
in vec4 v_col;

uniform sampler2D u_tex;
uniform bool u_shape;

vec4 text;

void main() {
    text = texture(u_tex, v_text);
    color = u_shape ? vec4(text.r * v_col.rgba) : vec4(mix(text.rgb, v_col.rgb, v_col.a), text.a);
    // color = vec4(textColor, 1.0) * sampled;
}
