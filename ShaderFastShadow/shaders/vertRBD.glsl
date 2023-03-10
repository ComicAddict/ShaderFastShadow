#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
//layout (location = 2) in vec2 aTexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

flat out vec3 fragPos;
flat out vec3 fNormal;

void main() {
	fragPos = vec3(model * vec4(aPos, 1.0f));
	fNormal = (vec4(aNormal, 1.0f)).xyz;
	gl_Position = projection * view * model * vec4(aPos, 1.0f);
}