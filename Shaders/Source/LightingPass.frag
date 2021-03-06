#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define PI 3.1415926535897932384626433832795

layout(set = 2, binding = 0) uniform sampler2D inGBuffer0;
layout(set = 2, binding = 1) uniform sampler2D inGBuffer1;
layout(set = 2, binding = 2) uniform sampler2D inGBuffer2;

layout(set = 3) uniform sampler2D inShadowMap;

layout(set = 4) uniform Light
{
	mat4 data;
} lightData;

layout(set = 5) uniform ShadowMap
{
	mat4 viewProjMatrix;
} shadowMap;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inEyePosition;
layout(location = 1) in vec2 inScreenSize;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

const float G_SCATTERING = 0.2;
const float NB_STEPS = 100.0;

// Mie scaterring approximated with Henyey-Greenstein phase function
float ComputeScattering(float lightDotView)
{
  float result = 1.0f - G_SCATTERING * G_SCATTERING;
  result /= (4.0f * PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) * lightDotView, 1.5f));
  return result;
}
void main()
{
  vec2 uv = gl_FragCoord.xy / inScreenSize;
  
  vec3 lightPosition = lightData.data[0].xyz;
  float lightType = lightData.data[0].w;
  vec3 lightColor = lightData.data[1].xyz;
  float lightIntensity = lightData.data[1].w;
  float lightRange = lightData.data[2].x;
  float lightSpecularPower = lightData.data[2].y;
  bool lightCastShadows = lightData.data[2].z > 0.5;
  
  vec4 positionMetallic = texture(inGBuffer0, uv);
  vec3 position = positionMetallic.rgb;
  float metallic = positionMetallic.a;
  
  vec4 albedoOcclusion = texture(inGBuffer1, uv);
	vec3 albedo = albedoOcclusion.rgb;
	float occlusion = 1.0 - albedoOcclusion.a;
	
	vec4 normalRoughness = texture(inGBuffer2, uv);
	vec3 normal = normalize(normalRoughness.rgb);
	float roughness = normalRoughness.a;
  
  vec3 light = vec3(0.0, 0.0, 0.0);
  
  if (lightType < 0.5)
  // directional light
  {
    vec3 lightDirection = normalize(lightPosition);
    float diffuseFactor = dot(normal, -lightDirection);
    float specularFactor = 0.0;
		
		vec3 diffuseColor = vec3(0.0, 0.0, 0.0);
		vec3 specularColor = vec3(0.0, 0.0, 0.0);
    
    if (diffuseFactor > 0.0)
		{
			diffuseColor = lightColor * lightIntensity * diffuseFactor;
    
      vec3 fragmentToEye = normalize(inEyePosition - position);
			vec3 lightReflect = normalize(reflect(lightDirection, normal));
			specularFactor = pow(dot(fragmentToEye, lightReflect), lightSpecularPower);

			if (specularFactor > 0.0)
			{
				specularColor = lightColor * lightIntensity * specularFactor * roughness;
			}
		}
		
		light = diffuseColor + specularColor;
  }
  else if (lightType < 1.5)
  // point light
  {
    vec3 lightToFragment = position - lightPosition;
    float distance = length(lightToFragment);
    lightToFragment = normalize(lightToFragment);
    float diffuseFactor = dot(normal, -lightToFragment);
		float specularFactor = 0.0;
		
		vec3 diffuseColor = vec3(0.0, 0.0, 0.0);
		vec3 specularColor = vec3(0.0, 0.0, 0.0);
		
		if (diffuseFactor > 0.0)
		{
			diffuseColor = lightColor * lightIntensity * diffuseFactor;
			
			vec3 fragmentToEye = normalize(inEyePosition - position);
			vec3 lightReflect = normalize(reflect(lightToFragment, normal));
			specularFactor = pow(dot(fragmentToEye, lightReflect), lightSpecularPower);

			if (specularFactor > 0.0)
			{
				specularColor = lightColor * lightIntensity * specularFactor * roughness;
			}
		}
		
		float attenuationFactor = 0.0;

		if (lightRange > 0.0)
		{
			attenuationFactor = clamp(1.0 - sqrt(distance / lightRange), 0.0, 1.0);
    }
			
		light = (diffuseColor + specularColor) * attenuationFactor;
  }
  
  vec3 accumFog = 0.0f.xxx;
  float shadow = 1.0;
  if (lightCastShadows)
  {
    vec4 shadowCoord = biasMat * shadowMap.viewProjMatrix * vec4(position, 1.0);	
    
    if (texture(inShadowMap, shadowCoord.xy).r < shadowCoord.z - 0.001)
    {
      shadow = 0.0;
    }
    
    vec3 lightDirection = normalize(lightPosition);
    
    vec3 worldPos = position;
    vec3 startPosition = inEyePosition;
     
    vec3 rayVector = worldPos - startPosition;
     
    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;
     
    float stepLength = rayLength / NB_STEPS;
     
    vec3 step = rayDirection * stepLength;
     
    vec3 currentPosition = startPosition;
     
    for (int i = 0; i < NB_STEPS; i++)
    {
      shadowCoord = biasMat * shadowMap.viewProjMatrix * vec4(currentPosition, 1.0);	
      if (texture(inShadowMap, shadowCoord.xy).r > shadowCoord.z + 0.001)
      {
        accumFog += ComputeScattering(dot(rayDirection, lightDirection)).xxx * lightColor;
      }
      
      currentPosition += step;
    }
    
    accumFog /= NB_STEPS;
  }
  
  outColor = vec4((light * max(albedo - occlusion, 0.0) * shadow) + accumFog * 4.0, 1.0);
}