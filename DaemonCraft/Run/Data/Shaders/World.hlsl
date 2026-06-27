//------------------------------------------------------------------------------------------------
// World shader for SimpleMiner voxel lighting (Assignment 5 Phase 8)
//
// Implements day/night cycle with indoor/outdoor lighting and directional shading.
// Vertex colors encode lighting data: r=outdoor, g=indoor, b=directional shading
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Input to the Vertex shader stage.
//------------------------------------------------------------------------------------------------
struct VertexInput
{
	float3	a_position		: VERTEX_POSITION;
	float4	a_color			: VERTEX_COLOR; // r=outdoor, g=indoor, b=directional, a=unused
	float2	a_uvTexCoords	: VERTEX_UVTEXCOORDS;
	uint	a_vertexID		: SV_VertexID;
};


//------------------------------------------------------------------------------------------------
// Output passed from Vertex shader to Pixel shader (barycentric interpolated)
//------------------------------------------------------------------------------------------------
struct VertexOutPixelIn
{
	float4 v_position		: SV_Position;
	float4 v_color			: SURFACE_COLOR;      // Lighting data encoded in RGB
	float2 v_uvTexCoords	: SURFACE_UVTEXCOORDS;
	float3 v_worldPosition	: WORLD_POSITION;     // For fog calculation
};


//------------------------------------------------------------------------------------------------
// CONSTANT BUFFERS
// Guildhall conventions: b0=System, b1=Frame, b2=Camera, b3=Model, b4-b7=Engine, b8-b13=Game
// IMPORTANT: Must match Default.hlsl register assignments: Camera=b3, Model=b4
//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b3)
{
	float4x4	c_worldToCamera;
	float4x4	c_cameraToRender;
	float4x4	c_renderToClip;
};


cbuffer ModelConstants : register(b4)
{
	float4x4	c_modelToWorld;
	float4		c_modelTint;
};


//------------------------------------------------------------------------------------------------
// Assignment 5 Phase 8: World-specific lighting constants
// Must match WorldConstants struct in World.cpp
//------------------------------------------------------------------------------------------------
cbuffer WorldConstants : register(b8)
{
	float4 c_cameraPosition;      // Camera world position (for fog distance)
	float4 c_indoorLightColor;    // Indoor light color (default: warm orange 255,230,204)
	float4 c_outdoorLightColor;   // Outdoor light color (varies with day/night)
	float4 c_skyColor;            // Sky/fog color (for distance fog)
	float  c_fogNearDistance;     // Fog near distance
	float  c_fogFarDistance;      // Fog far distance
	float  c_gameTime;            // Current game time in seconds
	float  c_padding;             // 16-byte alignment
};


//------------------------------------------------------------------------------------------------
// TEXTURE and SAMPLER
//------------------------------------------------------------------------------------------------
Texture2D<float4>	t_diffuseTexture : register(t0);
SamplerState		s_diffuseSampler : register(s0);


//------------------------------------------------------------------------------------------------
// VERTEX SHADER
//------------------------------------------------------------------------------------------------
VertexOutPixelIn VertexMain( VertexInput input )
{
	VertexOutPixelIn output;

	float4 modelPos  = float4( input.a_position, 1.0 );
	float4 worldPos  = mul( c_modelToWorld, modelPos );
	float4 cameraPos = mul( c_worldToCamera, worldPos );
	float4 renderPos = mul( c_cameraToRender, cameraPos );
	float4 clipPos   = mul( c_renderToClip, renderPos );

	output.v_position      = clipPos;
	output.v_color         = input.a_color;  // Lighting data: r=outdoor, g=indoor, b=directional
	output.v_uvTexCoords   = input.a_uvTexCoords;
	output.v_worldPosition = worldPos.xyz;

	return output;
}


//------------------------------------------------------------------------------------------------
// PIXEL SHADER
//
// Assignment 5 Phase 8: Voxel lighting with day/night cycle
// Vertex color encoding:
//   r channel = outdoor light intensity (0-1, from 0-255 byte)
//   g channel = indoor light intensity (0-1, from 0-255 byte)
//   b channel = directional shading (1.0=top, 0.8=sides, 0.6=bottom)
//
// Final brightness = max(outdoor * outdoorColor, indoor * indoorColor) * directional
//------------------------------------------------------------------------------------------------
float4 PixelMain( VertexOutPixelIn input ) : SV_Target0
{
	// Sample diffuse texture
	float2 uvCoords = input.v_uvTexCoords;
	float4 diffuseTexel = t_diffuseTexture.Sample( s_diffuseSampler, uvCoords );

	// Extract lighting data from vertex color
	float outdoorLight    = input.v_color.r; // 0-1 range (outdoor light intensity)
	float indoorLight     = input.v_color.g; // 0-1 range (indoor light intensity)
	float directionalShading = input.v_color.b; // 1.0=top, 0.8=sides, 0.6=bottom

	// Calculate outdoor contribution (modulated by day/night cycle)
	float3 outdoorContribution = outdoorLight * c_outdoorLightColor.rgb;

	// Calculate indoor contribution (warm constant light)
	float3 indoorContribution = indoorLight * c_indoorLightColor.rgb;

	// Take the brighter of outdoor vs indoor, then apply directional shading
	float3 lightColor = max(outdoorContribution, indoorContribution);
	float3 finalLight = lightColor * directionalShading;

	// Apply lighting to diffuse color
	float4 litColor = float4(diffuseTexel.rgb * finalLight * c_modelTint.rgb, diffuseTexel.a * c_modelTint.a);

	// Calculate distance fog
	float3 worldPos = input.v_worldPosition;
	float3 camPos = c_cameraPosition.xyz;
	float distanceToCamera = length(worldPos - camPos);
	float fogFactor = saturate((distanceToCamera - c_fogNearDistance) / (c_fogFarDistance - c_fogNearDistance));

	// Blend between lit color and sky color based on distance
	float4 finalColor = lerp(litColor, c_skyColor, fogFactor);

	// Alpha test (discard fully transparent pixels)
	if (finalColor.a <= 0.001)
	{
		discard;
	}

	return finalColor;
}
