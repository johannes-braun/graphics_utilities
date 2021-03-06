#version 460 core

layout(location = 0) in vec3 uv;
layout(location = 1) in vec3 campos;
layout(location = 0) out vec4 color;

float cnoise(vec3 P);

layout(binding = 0) uniform Camera
{
	mat4 view;
	mat4 proj;
	vec3 position;
} camera;

layout(binding = 1) uniform Time
{
	float time;
};

void main()
{
	vec3 bottom = vec3(0.2f);
	vec3 top = vec3(0.3f, 0.5f, 1.f);
	vec3 eve = vec3(0.9f, 0.5f, 0.1f);

	float up_angle = dot(normalize(uv), normalize(vec3(0,1,0)));

	vec3 result = bottom;

	float sy = 1.1f + 0.5f * sin(time);
	float sx = sin(time*0.1f) *1;
	float sz = cos(time*0.1f) * 1;

	vec3 sundir = normalize(vec3(sx, sy, sz));
	float up_angle_sun = dot(sundir, normalize(vec3(0,1,0)));

	result = mix(bottom, mix(eve, top, up_angle_sun), smoothstep(-0.4f, 0.1f, up_angle));

	float angle = dot(normalize(uv), sundir);
	result = mix(result, vec3(1.f), 0.5f*smoothstep(0.15f, 1.15f, angle));
	result = mix(result, vec3(8.f), smoothstep(0.985f, 1.08f, angle));
	
	vec3 tc = normalize(uv);
	tc.y *= 4.f;

	float cn1 = cnoise(1.4f*normalize(tc));
	float cn10 = cnoise(5.f*normalize(tc));
	float cn100 = cnoise(36.f*normalize(tc));
	float ncn1 = smoothstep(-0.92f, 0.22, cn1);
	float ncn10 = smoothstep(-1.f, 0.4f, cn10);
	float ncn100 = smoothstep(-1.f, 0.5f, cn100);
	float cl = smoothstep(0.0f, 0.9f, pow(ncn1, 4.f) * mix(ncn100, ncn10, ncn1));

	cl = mix(0, cl, smoothstep(0.01f, 0.4f, up_angle));

	tc += 0.14f * sundir;
	float cn1x = cnoise(1.4f*normalize(tc));
	float cn10x = cnoise(5.f*normalize(tc));
	float cn100x = cnoise(36.f*normalize(tc));
	float ncn1x = smoothstep(-0.92f, 0.42, cn1x);
	float ncn10x = smoothstep(-1.f, 0.5f, cn10x);
	float ncn100x = smoothstep(-1.f, 1, cn100x);
	float clx = smoothstep(0.6f, 1.2f, pow(ncn1x, 4.f) * mix(ncn100x, ncn10x, ncn1x));

	result = vec3(mix(result, vec3(1.f), 0.7f * cl));
	result = vec3(mix(result, mix(mix(eve, top, up_angle_sun), vec3(0.6f), 0.4f), mix(0, clx, cl)));
	result = mix(result, vec3(8.f), smoothstep(0.9955f, 1.05f, angle));

	color = vec4(result, 1);
}



//	Classic Perlin 3D Noise 
//	by Stefan Gustavson
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
vec3 fade(vec3 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}

float cnoise(vec3 P){
	P += vec3(0.1f*time + campos.x * 0.001f, 0, 0.1f*time + campos.z * 0.001f);

  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod(Pi0, 289.0);
  Pi1 = mod(Pi1, 289.0);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 / 7.0;
  vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 / 7.0;
  vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x); 
  return 2.2 * n_xyz;
}