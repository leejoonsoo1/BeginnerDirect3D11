
// ======================================================
// Vertex Shader용 상수 버퍼 (b0 슬롯)
// CPU에서 전달되는 행렬 데이터 저장
// ======================================================

cbuffer vsConstants : register(b0)
{
    // 모델 -> 클립 공간 변환 행렬
    float4x4 modelViewProj;
    
    // 모델 -> 뷰(카메라) 공간 변환 행렬
    float4x4 modelView;
    
    // 노멀 벡터 변환용 행렬
    // 보통 modelView의 inverse-transpose 사용
    float3x3 normalMatrix;
};

// ======================================================
// 방향광(Directional Light) 구조체
// ======================================================
struct DirectionalLight
{
    // 광원이 향하는 방향이 아니라
    // "빛 방향" (fragment -> light 방향)
    float4 dirEye; //NOTE: Direction *towards* the light
    
    // RGB 광원 색상
    float4 color;
};

// ======================================================
// 점광(Point Light) 구조체
// ======================================================
struct PointLight
{
    // 뷰 공간 기준 광원 위치
    float4 posEye;
    
    // RGB 광원 색상
    float4 color;
};

// ======================================================
// Pixel Shader용 상수 버퍼 (b0 슬롯)
// 조명 정보 저장
// ======================================================
// Create Constant Buffer for our Blinn-Phong vertex shader
cbuffer fsConstants : register(b0)
{
    // 방향광 1개
    DirectionalLight dirLight;
    
    // 점광 2개
    PointLight pointLights[2];
};

// ======================================================
// Vertex Shader 입력 구조체
// ======================================================
struct VS_Input {
    // 정점 위치
    float3 pos : POS;
    
    // UV 좌표
    float2 uv : TEX;
    
    // 정점 노멀
    float3 norm : NORM;
};

// ======================================================
// Vertex Shader 출력 구조체
// Pixel Shader 입력으로 전달됨
// ======================================================
struct VS_Output {
    // 최종 클립 공간 좌표
    float4 pos : SV_POSITION;
    
    // 뷰 공간 위치
    float3 posEye : POSITION;
    
    // 뷰 공간 노멀
    float3 normalEye : NORMAL;
    
    // UV 좌표
    float2 uv : TEXCOORD;
};

// ======================================================
// 텍스처 및 샘플러
// ======================================================
Texture2D    mytexture : register(t0);
SamplerState mysampler : register(s0);

// ======================================================
// Vertex Shader
// ======================================================
VS_Output vs_main(VS_Input input)
{
    VS_Output output;
    
    // 정점 위치를 클립 공간으로 변환
    output.pos = mul(float4(input.pos, 1.0f), modelViewProj);
    
    // 정점 위치를 뷰 공간으로 변환
    output.posEye = mul(float4(input.pos, 1.0f), modelView).xyz;
    
    // 노멀 벡터를 뷰 공간으로 변환
    output.normalEye = mul(input.norm, normalMatrix);
    
    // UV 좌표 전달
    output.uv = input.uv;
    return output;
}

// ======================================================
// Pixel Shader
// ======================================================
float4 ps_main(VS_Output input) : SV_Target
{
    // 텍스처 색상 샘플링
    float3 diffuseColor = mytexture.Sample(mysampler, input.uv).xyz;

    // Fragment -> Camera 방향 벡터
    // 카메라는 뷰 공간 원점(0, 0, 0)
    float3 fragToCamDir = normalize(-input.posEye);
    
    // ==================================================
    // 방향광 계산
    // ================================================== 
    // Directional Light
    float3 dirLightIntensity;
    {
        // Ambient 강도
        float ambientStrength = 0.1;
        
        // Specular 강도
        float specularStrength = 0.9;
        
        // Specular 지수 (광택 정도)
        float specularExponent = 100;
        
        // 빛 방향
        float3 lightDirEye = dirLight.dirEye.xyz;
        
        // 광원 색상
        float3 lightColor = dirLight.color.xyz;

        // --------------------------
        // Ambient
        // --------------------------
        float3 iAmbient = ambientStrength;

        // --------------------------
        // Diffuse (Lambert)
        // N·L
        // --------------------------
        float diffuseFactor = max(0.0, dot(input.normalEye, lightDirEye));
        float3 iDiffuse = diffuseFactor;

        // --------------------------
        // Specular (Blinn-Phong)
        // --------------------------
        
        // Halfway Vector  계산
        float3 halfwayEye = normalize(fragToCamDir + lightDirEye);
        
        // N·H
        float specularFactor = max(0.0, dot(halfwayEye, input.normalEye));
        
        // 최종 스페큘러
        float3 iSpecular = specularStrength * pow(specularFactor, 2*specularExponent);

        // 조명 합산
        dirLightIntensity = (iAmbient + iDiffuse + iSpecular) * lightColor;
    }
    
    // ==================================================
    // 점광 계산
    // ==================================================
    // Point Light
    float3 pointLightIntensity = float3(0,0,0);
    for(int i=0; i<2; ++i)
    {
        float ambientStrength = 0.1;
        float specularStrength = 0.9;
        float specularExponent = 100;
        
        // Fragment -> Light 방향 벡터
        float3 lightDirEye = pointLights[i].posEye.xyz - input.posEye;
        
        // 거리 계산
        float inverseDistance = 1 / length(lightDirEye);
        
        // 정규화
        lightDirEye *= inverseDistance; //normalise
        
        // 광원 색상
        float3 lightColor = pointLights[i].color.xyz;

        // --------------------------
        // Ambient
        // --------------------------
        float3 iAmbient = ambientStrength;

        // --------------------------
        // Diffuse
        // --------------------------
        float diffuseFactor = max(0.0, dot(input.normalEye, lightDirEye));
        float3 iDiffuse = diffuseFactor;

        // --------------------------
        // Specular
        // --------------------------
        float3 halfwayEye = normalize(fragToCamDir + lightDirEye);
        float specularFactor = max(0.0, dot(halfwayEye, input.normalEye));
        float3 iSpecular = specularStrength * pow(specularFactor, 2*specularExponent);

        // 거리 감쇠 포함
        pointLightIntensity += (iAmbient + iDiffuse + iSpecular) * lightColor * inverseDistance;
    }

    // ==================================================
    // 최종 색상 계산
    // 조명 * 텍스처 색상
    // ==================================================
    float3 result = (dirLightIntensity + pointLightIntensity) * diffuseColor;

    return float4(result, 1.0);
}
