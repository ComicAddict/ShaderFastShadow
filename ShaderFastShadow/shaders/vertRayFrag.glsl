#version 330 core

layout (location = 0) in vec2 a_position;
out vec2 fragpos;
void main() {
	fragpos = a_position;
	gl_Position = vec4(a_position, 0., 1.);
}