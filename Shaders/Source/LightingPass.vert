#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(set = 0) uniform LWCVPM { mat4 lightWorldCameraViewProjectionMatrix; } lwcvpm;

layout(set = 1) uniform Globals
{
  mat4 data;
} globals;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outEyePosition;
layout(location = 1) out vec2 outScreenSize;

void main()
{
	gl_Position = lwcvpm.lightWorldCameraViewProjectionMatrix * vec4(inPosition, 1.0);
	
	outEyePosition = globals.data[0].xyz;
	outScreenSize = globals.data[1].xy;
}