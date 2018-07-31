#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "ScreenPos.glsl"

varying vec2 vTexCoord;
varying vec2 vScreenPos;

#ifdef COMPILEPS
uniform float cSSRStep;                 
uniform float cSSRMaxSteps;
uniform float cSSRNumBinarySearchSteps;  
uniform float cSSRReflectionFalloff;
uniform float cSSRClipSSRDistance;
#endif

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    vTexCoord = GetQuadTexCoord(gl_Position);
    vScreenPos = GetScreenPosPreDiv(gl_Position);
}

#ifdef COMPILEPS

//HASH
#define Scale vec3(.9, .9, .9)
#define K 19.19

vec3 hash(vec3 a)
{
    a = fract(a * Scale);
    a += dot(a, a.yxz + K);
    return fract((a.xxy + a.yxx) * a.zyx);
}

vec3 BinarySearch(vec3 dir, vec3 hitCoord)
{
    float sampledDepth;
    vec4 projectedCoord;
    vec4 sampleClipPos;
    float dDepth;

    for(float i = 0.0; i < cSSRNumBinarySearchSteps; i=i+1.0)
    {
        projectedCoord = vec4(hitCoord, 1.0) * cViewProj;
        sampleClipPos = projectedCoord;
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5; 
        sampledDepth = DecodeDepth(texture2D(sDepthBuffer, projectedCoord.xy).rgb);
        dDepth = dot(sampleClipPos.zw, cDepthMode.zw) - sampledDepth;

        dir *= 0.5;
        if(dDepth > 0.0)
            hitCoord -= dir;
        else
            hitCoord += dir; 
               
    }

    projectedCoord = vec4(hitCoord, 1.0) * cViewProj;
    projectedCoord.xy /= projectedCoord.w;
    projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5; 
    return vec3(projectedCoord.xy, sampledDepth);
}

vec4 RayMarch(vec3 dir, vec3 hitCoord)
{
    dir *= cSSRStep;
    float depth = 0;
    vec4 projectedCoord = vec4(0,0,0,0);
    vec4 sampleClipPos = vec4(0,0,0,0);

    for(float i = 0.0; i < cSSRMaxSteps; i=i+1.0)
    {
        hitCoord += dir;

        projectedCoord = vec4(hitCoord, 1.0) * cViewProj;
        sampleClipPos = projectedCoord;
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5; 
        float sampledDepth = DecodeDepth(texture2D(sDepthBuffer, projectedCoord.xy).rgb);
        vec3 viewPos = texture2D(sSpecMap, projectedCoord.xy).xyz;
        depth = dot(sampleClipPos.zw, cDepthMode.zw);


        float fDepthDiff = depth - sampledDepth;
        if(fDepthDiff >= 0.0 && fDepthDiff < 0.005)
        {   
            vec4 Result;
            Result = vec4(BinarySearch(dir, hitCoord), 1.0);
            return Result;
        }
    }
    return vec4(0.0, 1.0, 0.0, 0.0);
}

void PS()
{
    vec4 albedoInput = texture2D(sAlbedoBuffer, vScreenPos);
    vec4 normalInput = texture2D(sNormalBuffer, vScreenPos);
    vec4 positionInput = texture2D(sSpecMap, vScreenPos);
    float depth = DecodeDepth(texture2D(sDepthBuffer, vScreenPos.xy).rgb);

    if(albedoInput.a < 0.5)
    {
        gl_FragColor = albedoInput; 
        return;
    }
        
    vec3 albedo = albedoInput.rgb;
    vec3 normal = normalize(normalInput.rgb * 2.0 - 1.0);
    float specIntensity = albedoInput.a;
    float specPower = normalInput.a;
    float ssr = specPower;
    

    vec3 hitPos = positionInput.xyz;
    vec3 viewPos = positionInput.xyz;
    vec3 viewNor = normalize(viewPos - cCameraPosPS);
    vec3 reflected = normalize(reflect(viewNor, normal));
    vec3 viewReflected = (vec4(reflected, 1.0) * cViewProj).xyz;

    vec3 jitt = mix(vec3(0.0), vec3(hash(viewPos)), ssr / 50.0);
    vec4 coords = RayMarch(reflected, jitt + hitPos);
    vec2 dCoords = smoothstep(0.3, 0.6, abs(vec2(0.5, 0.5) - vScreenPos.xy));
    
    float screenEdgeFactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);
    vec3 fresnel = vec3(1.0) * clamp(pow(1 - dot(normalize(viewPos), normal), 1), 0, 1);

    vec3 reflectionColor = texture2D(sDiffMap, coords.xy, 0).rgb;
    float reflectionMultiplier = pow(ssr, cSSRReflectionFalloff) * screenEdgeFactor * depth * 2.0;

    gl_FragColor = vec4(albedo + (reflectionColor * clamp(reflectionMultiplier, 0.0, 0.9) * fresnel), specIntensity);
}
#endif