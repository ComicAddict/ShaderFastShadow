#version 330
precision mediump float;
uniform float iTime;
uniform float rad;
uniform float part;
uniform vec2 iMouse;
uniform vec2 plane;
uniform vec2 res;
uniform vec3 cpos;
uniform vec3 lpos;
uniform vec3 rads;
uniform float sposs[9];
in vec2 fragpos;
out vec4 fragColor;


#define MAX_DIST 1e10
#define PATH_LENGTH 2
#define LAMBERTIAN 2.
#define EMIT 3.


float intersectPlane( in vec3 rayOrigin, in vec3 rayDirection, in vec2 bounds, inout vec3 normal, in vec3 planeNormal, in float planeLevel) {
    float intensity = dot(rayDirection, planeNormal);
    float d = -(dot(rayOrigin, planeNormal) + planeLevel)/intensity;
    if (d < bounds.x || d > bounds.y) {
        return MAX_DIST;
    } else {
        normal = planeNormal;
        //vec3 p = rayOrigin + d * rayDirection;
        //if(p.x > plane.x || p.x < -plane.x)
          //  return MAX_DIST;
        //if(p.z > plane.y || p.z < -plane.y)
           // return MAX_DIST;
    	return d;
    }
}

float iSphere( in vec3 rayOrigin, in vec3 rayDirection, in vec2 bounds, inout vec3 normal, float sphereRadius ) {
    float b = dot(rayOrigin, rayDirection);
    float c = dot(rayOrigin, rayOrigin) - sphereRadius*sphereRadius;
	//h is the formula of a sphere
    float h = b*b - c;
    if (h < 0.) {
		// we have no intersection
        return MAX_DIST;
    } else {
		//return back to normal
	    h = sqrt(h);
        float d1 = -b-h;
        float d2 = -b+h;
        if (d1 >= bounds.x && d1 <= bounds.y) {
            normal = normalize(rayOrigin + rayDirection*d1);
            return d1;
        } else if (d2 >= bounds.x && d2 <= bounds.y) {
            normal = normalize(rayOrigin + rayDirection*d2);
            return d2;
        } else {
            return MAX_DIST;
        }
    }
}

float iEllipsoid( in vec3 ro, in vec3 rd, in vec2 distBound, inout vec3 normal,
                  in vec3 rad ) {
    vec3 ocn = ro / rad;
    vec3 rdn = rd / rad;

    float a = dot( rdn, rdn );
	float b = dot( ocn, rdn );
	float c = dot( ocn, ocn );
	float h = b*b - a*(c-1.);

    if (h < 0.) {
        return MAX_DIST;
    }

	float d = (-b - sqrt(h))/a;

    if (d < distBound.x || d > distBound.y) {
        return MAX_DIST;
    } else {
        normal = normalize((ro + d*rd)/rad);
    	return d;
    }
}


uint baseHash( uvec2 p ) {
    p = 1103515245U*((p >> 1U)^(p.yx));
    uint h32 = 1103515245U*((p.x)^(p.y>>3U));
    return h32^(h32 >> 16);
}

vec2 randomHashNoise( inout float seed ) {
    uint n = baseHash(floatBitsToUint(vec2(seed+=.1,seed+=.1)));
    uvec2 radZ = uvec2(n, n*48271U);
    return vec2(radZ.xy & uvec2(0x7fffffffU))/float(0x7fffffff);
}

vec3 roughnessFactor(const float type, const vec3 normal, const vec3 n, const float roughness, inout float seed ) {
    vec2 r = vec2(0,0);
	vec3 axis1;
	if(abs(n.y) > .5){
		axis1 = cross(n, vec3(1.,0.,0.));
	} else {
		axis1 = cross(n, vec3(0.,1.,0.));
	}
	vec3  axis2 = cross(axis1, n);
	
	float radZ = type < LAMBERTIAN + 0.5 ? sqrt(1.-r.y) : sqrt(abs((1.0-r.y) / clamp(1.+(roughness*roughness - 1.)*r.y,.00001,1.)));
	float radiusA = type < LAMBERTIAN + 0.5 ? sqrt(r.y) : sqrt(abs(1.-radZ*radZ));
	float radX = radiusA*cos(6.28318530718*r.x);
	float radY = radiusA*sin(6.28318530718*r.x);
	vec3 normDirection = vec3(radX*axis1 + radY*axis2 + radZ*n);

    vec3 ret = normalize(normDirection);
	if(type < LAMBERTIAN + 0.5){
		return ret;
	}
    return dot(ret,normal) > 0. ? ret : n;
}

vec3 firstIntersection( vec3 d, float iResult, float mat ) {
	return (iResult < d.y) ? vec3(d.x, iResult, mat) : d;
}

vec3 worldhit( in vec3 rayOrigin, in vec3 rayDirection, in vec2 dist, out vec3 normal ) {
    vec3 d = vec3(dist, 0.);
    vec3 sp1 = vec3(sposs[0],sposs[1],sposs[2]);
    vec3 sp2 = vec3(sposs[3],sposs[4],sposs[5]);
    vec3 sp3 = vec3(sposs[6],sposs[7],sposs[8]);
    d = firstIntersection(d, intersectPlane(rayOrigin, rayDirection, d.xy, normal, vec3(0,1,0), 0.), 1.);//plane
    d = firstIntersection(d, iEllipsoid(rayOrigin-lpos, rayDirection, d.xy, normal, vec3(rad,0.05,rad)), 3.);//light
    d = firstIntersection(d, iSphere(rayOrigin-sp1, rayDirection, d.xy, normal, rads.x), 1.7);//sphere
    d = firstIntersection(d, iSphere(rayOrigin-sp2, rayDirection, d.xy, normal, rads.y), 1.8);//sphere
    d = firstIntersection(d, iSphere(rayOrigin-sp3, rayDirection, d.xy, normal, rads.z), 1.9);//sphere
    //d = firstIntersection(d, iSphere(rayOrigin, rayDirection, d.xy, normal, 45.0), 3.0);//sphere
    
    return d;
}

vec3 noHit( vec3 rayDirection ) {
    vec3 col = vec3(0.5,0.5,0.9);
    float sun = clamp(dot(normalize(vec3(-.1,.1,-.1)),rayDirection), 0., 1.);
    col += vec3(0.75,0.8,0.9)*(2.*pow(sun,8.) + 10.*pow(sun,32.));
    return col;
}

void getMaterialProperties(in vec3 pos, in float mat, out vec3 albedo, out float matType, out float roughness) {
	if(mat < 1.5){
		//plane
		albedo = vec3(.6);//vec3(.25 + .25*mod(floor(pos.x*5.) + floor(pos.z*5.), 2.));
		roughness = 1.;
        matType = LAMBERTIAN;
	} else if( mat < 2.5){
		//sphere
		albedo = vec3(.6);
		matType = LAMBERTIAN;
		roughness = 1.;
    } else if( mat < 3.5){
		//emitter
		albedo = vec3(5.0f);
		matType = EMIT;
		roughness = 0.;
    }
}


vec3 render( in vec3 rayOrigin, in vec3 rayDirection, inout float seed ) {
    vec3 albedo, normal, col = vec3(1.);
    float roughness, matType;
    vec3 disttosphere = vec3(0);
    vec3 rayHit = worldhit( rayOrigin, rayDirection, vec2(.0001, 100), normal );
	if(rayHit.z < 2.5 && rayHit.z > 0.){
        // we are hit to plane and sphere
        // sample 10 constant points on the 
        rayOrigin += rayDirection * rayHit.y; // update hit pos
        getMaterialProperties(rayOrigin, rayHit.z, albedo, matType, roughness);//get materials
        col *= albedo;
        float a = 0;
        float d = 0;
        vec3 lray = lpos - rayOrigin;
        vec3 spos;
        vec3 pray = spos - rayOrigin;
        int res = 16;
        if(rayHit.z > 1.85){
            spos = vec3(sposs[6],sposs[7], sposs[8]);
            pray = spos - rayOrigin;
            float c = 0;
            for(int i = 0; i < res; i++){
                vec3 p = lpos + vec3(rad * cos(i * 12.56 / res), 0.0, rad * sin(i * 12.56 / res));
                lray = p - rayOrigin;
                c = (dot(normalize(lray), normal)+1)/2.0;
                d = d + c - d*c;
            }
            col = vec3(d);
            return col;
        } else if(rayHit.z > 1.75){
            spos = vec3(sposs[3],sposs[4], sposs[5]);
            pray = spos - rayOrigin;
            float c = 0;
            for(int i = 0; i < res; i++){
                vec3 p = lpos + vec3(rad * cos(i * 12.56 / res), 0.0, rad * sin(i * 12.56 / res));
                lray = p - rayOrigin;
                c = (dot(normalize(lray), normal)+1)/2.0;
                d = d + c - d*c;
            }
            col = vec3(d);
            return col;
        } else if(rayHit.z > 1.65){
            spos = vec3(sposs[0],sposs[1], sposs[2]);
            pray = spos - rayOrigin;
            float c = 0;
            for(int i = 0; i < res; i++){
                vec3 p = lpos + vec3(rad * cos(i * 12.56 / res), 0.0, rad * sin(i * 12.56 / res));
                lray = p - rayOrigin;
                c = (dot(normalize(lray), normal)+1)/2.0;
                d = d + c - d*c;
            }
            col = vec3(d);
            return col;
        }
        for( int i = 0; i < 3; i++){
            spos = vec3(sposs[3*i],sposs[3*i+1], sposs[3*i + 2]);
            pray = spos - rayOrigin;
            float ratio =  length(rayOrigin - lpos) / length(rayOrigin - spos);
            vec3 projp = pray * ratio + rayOrigin; 
            float r2 = rads[i] * ratio;
            float r1 = rad;
            float d = length(projp - lpos);
            float a_max = pow(r1, 2);
            float b;
            if (d > r2 + r1) {
                b = 0.0f;
            }
            else if (d <= (r1 - r2) && r1 >= r2) {
                b = pow(r2, 2) / a_max;
            }
            else if (d <= (r2 - r1) && r2 >= r1) {
                b = pow(r1, 2) / a_max;
            }
            else {
                float alpha = acos((r1 * r1 + d * d - r2 * r2) / (2 * r1 * d)) * 2;
                float beta = acos((r2 * r2 + d * d - r1 * r1) / (2 * r2 * d)) * 2;
                float a1 = 0.5 * beta * r2 * r2 - 0.5f * r2 * r2 * sin(beta);
                float a2 = 0.5 * alpha * r1 * r1 - 0.5f * r1 * r1 * sin(alpha);
                b = (a1 + a2)/(a_max * 3.14);
                
            }
            a = a+b-a*b;
        }
        
        col *= vec3(1.0f-a);
        col += vec3(0.75,0.8,0.9)/3;
        col /= 2; 
        //col /= pow(length(lray),2);
        return col;
    } else if(rayHit.z < 2.5 && rayHit.z > 1.5){
        rayOrigin += rayDirection * rayHit.y; // update hit pos
        getMaterialProperties(rayOrigin, rayHit.z, albedo, matType, roughness);//get materials
        col *= albedo;
        vec3 lray = lpos - rayOrigin;
        rayHit = worldhit( rayOrigin, normalize(lray), vec2(.0001, 100), normal );
        if(rayHit.z < 2.5){
            return vec3(0);
        }
        return col;
    } else if(rayHit.z > 2.5){
        col *= vec3(5.f);
        return col;
    } else {
        col *= noHit(rayDirection);
		return col;
    }
    return vec3(0);
}

//
// Ray tracer helper functions
//

float FresnelSchlickRoughness( float cosTheta, float F0, float roughness ) {
    return F0 + (max((1. - roughness), F0) - F0) * pow(abs(1. - cosTheta), 5.0);
}


float hash1( inout float seed ) {
    uint n = baseHash(floatBitsToUint(vec2(seed+=.1,seed+=.1)));
    return float(n)/float(0xffffffffU);
}

vec2 hash2( inout float seed ) {
    uint n = baseHash(floatBitsToUint(vec2(seed+=.1,seed+=.1)));
    uvec2 rz = uvec2(n, n*48271U);
    return vec2(rz.xy & uvec2(0x7fffffffU))/float(0x7fffffff);
}
vec3 cosWeightedRandomHemisphereDirection( const vec3 n, inout float seed ) {
  	vec2 r = hash2(seed);
	vec3  uu = normalize(cross(n, abs(n.y) > .5 ? vec3(1.,0.,0.) : vec3(0.,1.,0.)));
	vec3  vv = cross(uu, n);
	float ra = sqrt(r.y);
	float rx = ra*cos(6.28318530718*r.x);
	float ry = ra*sin(6.28318530718*r.x);
	float rz = sqrt(1.-r.y);
	vec3  rr = vec3(rx*uu + ry*vv + rz*n);
    return normalize(rr);
}

vec3 modifyDirectionWithRoughness( const vec3 normal, const vec3 n, const float roughness, inout float seed ) {
    vec2 r = hash2(seed);

	vec3  uu = normalize(cross(n, abs(n.y) > .5 ? vec3(1.,0.,0.) : vec3(0.,1.,0.)));
	vec3  vv = cross(uu, n);

    float a = roughness*roughness;

	float rz = sqrt(abs((1.0-r.y) / clamp(1.+(a - 1.)*r.y,.00001,1.)));
	float ra = sqrt(abs(1.-rz*rz));
	float rx = ra*cos(6.28318530718*r.x);
	float ry = ra*sin(6.28318530718*r.x);
	vec3  rr = vec3(rx*uu + ry*vv + rz*n);

    vec3 ret = normalize(rr);
    return dot(ret,normal) > 0. ? ret : n;
}

vec2 randomInUnitDisk( inout float seed ) {
    vec2 h = hash2(seed) * vec2(1,6.28318530718);
    float phi = h.y;
    float r = sqrt(h.x);
	return r*vec2(sin(phi),cos(phi));
}

float schlick(float cosine, float r0) {
    return r0 + (1.-r0)*pow((1.-cosine),5.);
}
vec3 render2( in vec3 ro, in vec3 rd, inout float seed ) {
    vec3 albedo, normal, col = vec3(1.);
    float roughness, type;

    for (int i=0; i<PATH_LENGTH; ++i) {
    	vec3 res = worldhit( ro, rd, vec2(.0001, 100), normal );
		if (res.z > 0.) {
			ro += rd * res.y;

            getMaterialProperties(ro, res.z, albedo, type, roughness);

            if (type < LAMBERTIAN+.5) { // Added/hacked a reflection term
                //float F = FresnelSchlickRoughness(max(0.,-dot(normal, rd)), .04, roughness);
                //if (F > hash1(seed)) {
                  //  rd = modifyDirectionWithRoughness(normal, reflect(rd,normal), roughness, seed);
                //} else {
                    col *= albedo;
			        rd = cosWeightedRandomHemisphereDirection(normal, seed);
                //}
            } else if(type < EMIT +.5){
                col *= albedo;
                return col;
            }
        } else {
            col *= noHit(rd);
            //col *= vec3(0,0,0);
			return col;
        }
    }
    return col;
}


mat3 setCamera( in vec3 rayOrigin, in vec3 ta, float cr ) {
	vec3 camw = normalize(ta-rayOrigin);
	vec3 camp = vec3(sin(cr), cos(cr),0.0);
	vec3 camu = normalize( cross(camw,camp) );
	vec3 camv = ( cross(camu,camw) );
    return mat3( camu, camv, camw );
}

void main() {
    vec2 mo = iMouse.xy == vec2(0) ? vec2(.125) : abs(iMouse.xy)/vec2(res.x,res.y) - .5;
	
    vec3 rayOrigin = vec3(cpos.x+3.2*cos(1.5+6.*mo.x), cpos.y+2.*mo.y, cpos.z+3.2*sin(1.5+6.*mo.x));
    vec3 ta = cpos;
    mat3 ca = setCamera(rayOrigin, ta, 0.);
    vec2 p = (-vec2(res.x,res.y) + 2.*gl_FragCoord.xy - 1.)/res.y;
    float seed = float(baseHash(floatBitsToUint(p - iTime)))/float(0xffffffffU);
	
	//p += 2.*randomHashNoise(seed)/600.;
	vec3 rayDirection = ca * normalize( vec3(p.xy,1.6) );  

    vec4 temp = vec4(0);
    if (fragpos.x < part)
        temp = vec4(render(rayOrigin, rayDirection, seed),1);
    else{
        for(int i = 0; i < 32; i++){
            temp += vec4(render2(rayOrigin, rayDirection, seed),1);
        }
    }
    
    temp = vec4(temp.rgb / temp.w, 1);
    temp = max( vec4(0), temp - 0.004);
    fragColor = (temp*(6.2*temp + .5)) / (temp*(6.2*temp+1.7) + 0.06);
}