#version 460 core
#include "../../api.glsl"

#define MSIZE 9
layout(location = 0) in vec2 unused_uv;

layout(loc_gl(0) loc_vk(0, 0)) uniform sampler2D src_textures[1];
#define this_texture (src_textures[0])

const float gauss_sigma = 0.5f;
const float gauss_bsigma = 0.055f;

layout(location=0) out vec4 color;

float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

float normpdf3(in vec4 v, in float sigma)
{
	return 0.39894*exp(-0.5*dot(v,v)/(sigma*sigma))/sigma;
}

void main(void)
{
    vec2 sketchSize = textureSize(this_texture, 0);
	vec4 c = texture( this_texture, vec2(0.0, 0.0) + (gl_FragCoord.xy/sketchSize) );

	if(true || gauss_sigma==0)
	{
		color = c;
		return;
	}
		
		
	//declare stuff
	const int kSize = (MSIZE-1)/2;
	float kernel[MSIZE];
	vec4 final_colour = vec4(0.0);
	
	//create the 1-D kernel
	float Z = 0.0;
	for (int j = 0; j <= kSize; ++j)
	{
		kernel[kSize+j] = kernel[kSize-j] = normpdf(float(j), gauss_sigma);
	}
	
	
	vec4 cc;
	float factor;
	float bZ = 1.0/normpdf(0.0, gauss_bsigma);
	//read out the texels
	for (int i=-kSize; i <= kSize; ++i)
	{
		for (int j=-kSize; j <= kSize; ++j)
		{
			cc = texture(this_texture, vec2(0.0, 0.0) + ( gl_FragCoord.xy + vec2(float(i),float(j)) ) );
			factor = normpdf3(cc-c, gauss_bsigma)*bZ*kernel[kSize+j]*kernel[kSize+i];
			Z += factor;
			final_colour += factor*cc;

		}
	}
	
	color = final_colour/Z;

}