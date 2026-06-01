#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "Lights.hlsli"


SamplerState pointSamplerState : register(s0);
SamplerState linearSamplerState : register(s1);
SamplerState anisotropicSamplerState : register(s2);
SamplerState linearBorderBlackSamplerState : register(s3);
SamplerState linearBorderWhiteSamplerState : register(s4);
SamplerState linearClampSamplerState : register(s5);
SamplerState linearMirrorSamplerState : register(s6);

SamplerComparisonState comparisonSamplerState : register(s7);

#define PI 3.14159265358979

/*
	lowFrequencyPerlinWorleyTexture
	r channel is perln-worley noise, gba channels are worley noise at increasing frequencies 
*/
Texture3D<float4> lowFrequencyPerlinWorleyTexture : register(t0);
/*
	lowFrequencyPerlinWorleyTexture
	rgb channels are worley noise at increasing frequencies 
*/
Texture3D<float3> highFrequencyWorleyTexture : register(t1);
/*
	weatherTexture
	- cloud coverage(r channel): the precentage of cloud coverage in the sky
	- precipitation(g channel): the chance that the cloud overhead will produce rain
	- cloud type(b channel): a value of 0.0 indicates stratus, 0.5 indicates stratocumulus, and 1.0 indicate cumulus cloud
*/
Texture2D<float3> weatherTexture : register(t2);
Texture2D<float3> curlNoiseTexture : register(t3);

static const float timeOffset = 10000.0;

float4 SampleLowFrequencyNoises(float3 samplePoint, float mipLevel)
{
    return lowFrequencyPerlinWorleyTexture.SampleLevel(linearMirrorSamplerState, samplePoint * lowFrequentlyPerlinMorleySamplingScale, mipLevel);
}

float3 SampleHighFrequencyNoises(float3 samplePoint, float mipLevel)
{
    return highFrequencyWorleyTexture.SampleLevel(linearMirrorSamplerState, samplePoint * highFrequentlyMorleySamplingScale, mipLevel);
}

float3 SampleWeatherData(float2 samplePoint)
{
    float2 offset = ((elapsedTime + timeOffset) * 0.001) * normalize(-windDirection) * windSpeed;
#if 1
    float horizonDistance = sqrt(cloudAltitudesMinMax.x + cloudAltitudesMinMax.x - earthRadius * earthRadius) * horizonDistanceScale;
    return weatherTexture.Sample(linearMirrorSamplerState, float2(samplePoint.x + horizonDistance, horizonDistance - samplePoint.y) / (2.0 * horizonDistance) + offset);
#else
    const float weatherScale = 0.00006;
    return weatherTexture.Sample(linearMirrorSamplerState, samplePoint * weatherScale + 0.5 * offset, 0);
#endif
}

float Hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p += 17.0;
    return (frac(p.x * p.y * p.z * (p.x + p.y + p.z)));
}

// utility function that maps a value from one range to another
float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

// henyey-greenstein phase function
float HenyeyGreenstein(float cosTheta, float g)
{
    return (1.0 - g * g) / (pow(1.0 + g * g - 2.0 * g * cosTheta, 1.50) * 4 * PI);
}

// simple analytic sky
float3 Atmosphere(float3 eyeDir)
{
    // sun constants
#if 0
    const float3 sunDirection = normalize(-directionalLightDirection[0].xyz);
#else
    const float distanceToSun = 1000.0;
    const float3 sunDirection = normalize(normalize(-lightDirection.xyz) * distanceToSun - cameraPositon.xyz);
#endif	
    const float3 lightColor = float3(1.0, 1.0, 1.0);
    const float3 groundColor = float3(1.0, 1.0, 1.0);
	
	// optical length at zenith for molecules
    const float rayleighZenithSize = 8.4e3;
    const float mieZenithSize = 1.25e3;
	//const float rayleigh = 2.0; // [0.0, 64.0]
    const float rayleigh = 1.0; // [0.0, 64.0]
    const float4 rayleighColor = float4(0.26, 0.41, 0.58, 1.0);
    const float mie = 0.005; // [0.0, 1.0]
	//const float mieEccentricity = 0.8; // [-1.0, +1.0]
    const float mieEccentricity = 0.9; // [-1.0, +1.0]
    const float4 mieColor = float4(0.63, 0.77, 0.92, 1.0);

    const float turbidity = 10.0; // [0.0, 1000.0]
    const float exposure = 0.1; // [0.0, 128.0]
	
    float zenithAngle = clamp(dot(float3(0.0, 1.0, 0.0), sunDirection), -1.0, 1.0);
    float sunEnergy = max(0.0, 1.0 - exp(-((PI * 0.5) - acos(zenithAngle)))) * 1000.0;
    float sunFade = 1.0 - clamp(1.0 - exp(sunDirection.y), 0.0, 1.0);

	// rayleigh coefficients
    float rayleighCoefficient = rayleigh - (1.0 * (1.0 - sunFade));
    float3 rayleighBeta = rayleighCoefficient * rayleighColor.rgb * 0.0001;
	// mie coefficients from Preetham
    float3 mieBeta = turbidity * mie * mieColor.rgb * 0.000434;

	// optical length
    float zenith = acos(clamp(dot(float3(0.0, 1.0, 0.0), eyeDir), 0.0, 1.0));

    float opticalMass = 1.0 / (cos(zenith) + 0.15 * pow(93.885 - degrees(zenith), -1.253));
    float rayleighScatter = rayleighZenithSize * opticalMass;
    float mieScatter = mieZenithSize * opticalMass;

	// light extinction based on thickness of atmosphere
    float3 extinction = exp(-(rayleighBeta * rayleighScatter + mieBeta * mieScatter));

	// in scattering
    float cosTheta = dot(eyeDir, sunDirection);

    float rayleighPhase = (3.0 / (16.0 * PI)) * (1.0 + pow(cosTheta * 0.5 + 0.5, 2.0));
    float3 rayleighBetaTheta = rayleighBeta * rayleighPhase;

    float miePhase = HenyeyGreenstein(cosTheta, mieEccentricity);
    float3 mieBetaTheta = mieBeta * miePhase;

    float3 Li = pow(sunEnergy * ((rayleighBetaTheta + mieBetaTheta) / (rayleighBeta + mieBeta)) * (1.0 - extinction), 1.5);
    
    Li *= lerp(1.0, pow(sunEnergy * ((rayleighBetaTheta + mieBetaTheta) / (rayleighBeta + mieBeta)) * extinction, 0.5), clamp(pow(1.0 - zenithAngle, 5.0), 0.0, 1.0));

	// hack in the ground color
    Li *= lerp(groundColor, 1.0, smoothstep(-0.1, 0.1, dot(float3(0.0, 1.0, 0.0), eyeDir)));

#if 0
	float3 L0 = 0;
#else
    const float solSize = 0.00872663806;
    const float sunDiskScale = 2.0; // [0.0, 360.0]
	// solar disk and out-scattering
    float sunAngularDiameterCosMin = cos(solSize * sunDiskScale);
    float sunAngularDiameterCosMax = cos(solSize * sunDiskScale * 0.5);
    float sunDisk = smoothstep(sunAngularDiameterCosMin, sunAngularDiameterCosMax, cosTheta);
    float3 Lo = (sunEnergy * 1900.0 * extinction) * sunDisk * lightColor;
	// note: add nightime here: L0 += nightSky * extinction
#endif
	
    float3 color = (Li + Lo) * 0.04;
    color = pow(color, 1.0 / (1.2 + (1.2 * sunFade)));
    color *= exposure;
	
    return color;
}

// fractional value for sample position in the cloud layer
float GetHeightFractionForPoint(float position)
{
	// get global fractional position in cloud zone
    float heightFraction = (position - cloudAltitudesMinMax.x) / (cloudAltitudesMinMax.y - cloudAltitudesMinMax.x);
    return clamp(heightFraction, 0.0, 1.0);
}

float4 MixGradients(float cloudType)
{
    const float4 stratusGradient = float4(0.02f, 0.05f, 0.09f, 0.11f);
    const float4 stratocumulusGradient = float4(0.02f, 0.2f, 0.48f, 0.625f);
    const float4 cumulusGradient = float4(0.01f, 0.0625f, 0.78f, 1.0f);
	
    float stratus = 1.0f - clamp(cloudType * 2.0f, 0.0, 1.0);
    float stratocumulus = 1.0f - abs(cloudType - 0.5f) * 2.0f;
    float cumulus = clamp(cloudType - 0.5f, 0.0, 1.0) * 2.0f;
	
    return stratusGradient * stratus + stratocumulusGradient * stratocumulus + cumulusGradient * cumulus;
}

float GetDensityHeightGradient(float heightFraction, float cloudType)
{
    float densityGradient = 0.0;

#if 0
	const float stratusThreshold = 0.1;
	const float stratocumulusThreshold = 0.9;
	//const float cumulusThreshold = 1.0;

	int type = 2;
    // cloud type: {0: stratus, 1: cumulus, 2: cumulonimbus}
    if (cloudType < stratusThreshold)
	{
		type = 0;
	}
    else if (cloudType < stratocumulusThreshold)
	{
		type = 1;
	}
	
	
	// sample from gradient texture
	if (type == 0) // stratus clouds
	{
        densityGradient = GradientStratusTexture.SampleLevel(sampler_states[LINEAR_BORDER_BLACK], float2(0.5, 1.0 - height_fraction), 0);
    }
	else if (type == 1) // cumulus clouds
	{
        densityGradient = gradient_cumulus_texture.SampleLevel(sampler_states[LINEAR_BORDER_BLACK], float2(0.5, 1.0 - height_fraction), 0);
    }
	else if (type == 2)// cumulunimbusclouds
	{
        densityGradient = gradient_cumulonimbus_texture.SampleLevel(sampler_states[LINEAR_BORDER_BLACK], float2(0.5, 1.0 - height_fraction), 0);
    }
#else
    const float4 stratusGradient = float4(0.02f, 0.05f, 0.09f, 0.11f);
    const float4 stratocumulusGradient = float4(0.02f, 0.2f, 0.48f, 0.625f);
    const float4 cumulusGradient = float4(0.01f, 0.0625f, 0.78f, 1.0f);
	
    float stratus = 1.0f - clamp(cloudType * 2.0f, 0.0, 1.0);
    float stratocumulus = 1.0f - abs(cloudType - 0.5f) * 2.0f;
    float cumulus = clamp(cloudType - 0.5f, 0.0, 1.0) * 2.0f;

    float4 cloudGradient = stratusGradient * stratus + stratocumulusGradient * stratocumulus + cumulusGradient * cumulus;
    densityGradient = smoothstep(cloudGradient.x, cloudGradient.y, heightFraction) - smoothstep(cloudGradient.z, cloudGradient.w, heightFraction);
#endif
    return densityGradient;

}

float IntersectSphere(float3 pos, float3 dir, float r)
{
	// define the center of the sphere as the origin of coordinates
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, pos);
    float c = dot(pos, pos) - (r * r);
    float d = sqrt((b * b) - 4.0 * a * c);
    float p = -b - d;
    float p2 = -b + d;
    return max(p, p2) / (2.0 * a);
}

#define ENABLE_CLOUD_ANIMATION 
// Andrew Schneider "Real-Time Volumetric Cloudscape" In GPU Pro 7, pp. 97-127
// returns density at a given point
float SampleCloudDensity(float3 samplePoint, float3 weatherData, float mipLevel, bool cheapSample = false)
{
	// get the height-fraction for use with blending noise types over height
    float heightFraction = GetHeightFractionForPoint(length(samplePoint));
	
#ifdef ENABLE_CLOUD_ANIMATION
    samplePoint.xz += (elapsedTime + timeOffset) * 20.0 * normalize(-windDirection) * windSpeed * 0.6;
#endif
	
	
	// read the low-frequency perlin-worley and worley noises
    float4 lowFrequencyNoises = SampleLowFrequencyNoises(samplePoint, mipLevel - 2.0);
	
	// build an fbm(fractal brownian motion) out of the low-frequency worley noises that can be used to add detail to the low-frequency perlin-worley noise
    float lowFrequencyFbm = lowFrequencyNoises.g * 0.625 + lowFrequencyNoises.b * 0.25 + lowFrequencyNoises.a * 0.125;
    
	// define the base cloud shape by dilating it with the low-frequency fbm(fractal brownian motion) mode of worley noise
    float baseCloud = Remap(lowFrequencyNoises.r, -(1.0 - lowFrequencyFbm), 1.0, 0.0, 1.0);
	
	// get the density-height gradient using the density height function
    float cloudType = clamp(weatherData.b * cloudTypeScale, 0.0, 1.0);
    float densityHeightGradient = GetDensityHeightGradient(heightFraction, cloudType);
	
	// apply the height function to the base cloud shape
    baseCloud *= densityHeightGradient;
	
	// cloud coverage is stored in weather_data's red channel
    float cloudCoverage = weatherData.r * cloudCoverageScale;
	
	// use remap to apply the cloud  coverage attribute
    float baseCloudWithCoverage = Remap(baseCloud, 1.0 - cloudCoverage, 1.0, 0.0, 1.0);
    baseCloudWithCoverage *= cloudCoverage;

    float finalCloud = baseCloudWithCoverage;
    if (!cheapSample && baseCloudWithCoverage > 0.0)
    {
		// constract with worley noise at the top produces billowing detail
	
#ifdef ENABLE_CLOUD_ANIMATION
        if (windSpeed != 0.0)
        {
            samplePoint.xz -= (elapsedTime + timeOffset) * normalize(-windDirection) * 40.0;
            samplePoint.y -= (elapsedTime + timeOffset) * 40.0;
        }
#endif
		
#if 1
		// add some turbulence to bottom of clouds
        float3 curlNoise = curlNoiseTexture.SampleLevel(linearMirrorSamplerState, samplePoint.xy * 0.00008, 0);
        samplePoint.xy = samplePoint.xy + curlNoise.xy * (1.0 - heightFraction);
#endif
		
		// sample high-frequency noises
        float3 highFrequencyNoises = SampleHighFrequencyNoises(samplePoint, mipLevel);
	
		// build high-frequency worley noise fbm(fractal brownian motion)
        float highFrequencyFbm = highFrequencyNoises.r * 0.625 + highFrequencyNoises.g * 0.25 + highFrequencyNoises.b * 0.125;
	
		// transition from wispy shape to billowy shape over height
#if 0
		float highFrequencyNoiseModifier = lerp(highFrequencyFbm, 1.0 - highFrequencyFbm, clamp(heightFraction * 10.0, 0.0, 1.0));
#else
        float highFrequencyNoiseModifier = lerp(highFrequencyFbm, 1.0 - highFrequencyFbm, clamp(heightFraction * 4.0, 0.0, 1.0));
#endif
	
		// erode the base cloud shape with distorted high-frequency worley noises
#if 1
        finalCloud = Remap(baseCloudWithCoverage, highFrequencyNoiseModifier * 0.2, 1.0, 0.0, 1.0);
#else	
		finalCloud = Remap(baseCloudWithCoverage, highFrequencyNoiseModifier * 0.4 * heightFraction, 1.0, 0.0, 1.0);
#endif
    }

#if 0
	return clamp(finalCloud, 0.0, 1.0);
#else
    return pow(clamp(finalCloud, 0.0, 1.0), (1.0 - heightFraction) * 0.8 + 0.5);
#endif
}

static const float3 noiseKernel[6] =
{
    float3(0.38051305f, 0.92453449f, -0.02111345f),
    float3(-0.50625799f, -0.03590792f, -0.86163418f),
    float3(-0.32509218f, -0.94557439f, 0.01428793f),
    float3(0.09026238f, -0.27376545f, 0.95755165f),
    float3(0.28128598f, 0.42443639f, -0.86065785f),
    float3(-0.16852403f, 0.14748697f, 0.97460106f)
};

// a function to gather density in a cone for use with lighting clouds
float SampleCloudDensityAlongCone(float3 rayOrigin, float3 rayDirection /*normalized*/, float coneSpreadMultplier /*how wide to make the cone*/)
{
	
    float3 samplePoint = rayOrigin;
	
    float3 weatherData = 0.0;
    float densityAlongCone = 0.0;
	
	// lighting ray-march loop
	[unroll]
    for (int i = 0; i < 6; i++)
    {
		// add the current step offset to the sample position
        samplePoint += (rayDirection + noiseKernel[i] * float(i)) * coneSpreadMultplier;
        weatherData = SampleWeatherData(samplePoint.xz);
#if 0
		// sample cloud density the cheap way, using only one level of noise
		densityAlongCone += SampleCloudDensity(samplePoint, weatherData, float(i) /*mipLevel*/, densityAlongCone > 0.3 /*cheapSample*/);
#else
		// always sample cloud density the expensive way
        densityAlongCone += SampleCloudDensity(samplePoint, weatherData, float(i) /*mipLevel*/, false /*cheapSample*/);
#endif
    }
    return densityAlongCone;
}

// long distance light sample combined with the cone samples
float SampleCloudDensityLongDistance(float3 rayOrigin, float3 rayDirection /*normalized*/, float coneSpreadMultplier /*how wide to make the cone*/)
{
	//const float cloud_density_long_distance_scale = 18.0;
    const float longDistance = cloudDensityLongDistanceScale * coneSpreadMultplier;
	
    float3 samplePoint = rayOrigin + rayDirection * longDistance;
	
    float heightFraction = GetHeightFractionForPoint(length(samplePoint));
    float3 weatherData = SampleWeatherData(samplePoint.xz);
	
    return pow(SampleCloudDensity(samplePoint, weatherData, 5.0 /*mipLevel*/, false /*cheapSample*/), (1.0 - heightFraction) * 0.8 + 0.5);
}

float4 RayMarch(float3 rayOrigin, float3 rayStep, int steps)
{
    float stepSize = length(rayStep);
	
	// Use a coordinate system with the center of the earth as the origin
#if 0
	float3 samplePoint = rayOrigin + rayStep;
#else
    float3 samplePoint = rayOrigin + rayStep * Hash(rayOrigin * 10.0); // dithering ? 
#endif
	
    float3 sunDirection = normalize(-lightDirection.xyz);
	
    float cosTheta = dot(sunDirection, normalize(rayStep));
	// stack multiple phase functions to emulate some backscattering
#if 1
    float g = 0.2; // eccentricity
    float henyeyGreensteinPhase = HenyeyGreenstein(cosTheta, g);
#else
	float henyeyGreensteinPhase = max(max(HenyeyGreenstein(cosTheta, 0.6), HenyeyGreenstein(cosTheta, (0.4 - 1.4 * sunDirection.y))), HenyeyGreenstein(cosTheta, -0.2)); // TODO
#endif
	
	// precalculate atmosphere color
    float3 atmosphereColor = Atmosphere(sunDirection) * stepSize * 0.1;
	
    const float coneSpreadMultplier = ((cloudAltitudesMinMax.y - cloudAltitudesMinMax.x) / 36.0); // how wide to make the cone
	
    float3 color = 0.0;
    float density = 0;
    float transmittence = 1.0;
    float cloudTest = 0;
    int zeroDensitySampleCount = 0;
	
	// start the main ray-march loop
	[loop]
    for (int i = 0; i < steps; i++)
    {
		// cloudTest starts as zero as we always evaluate the second cas from the beginning
        if (cloudTest > 0.0)
        {
			// sample density the expensive way the setting the last parameter to false, indicating a full sample
            float3 weatherData = SampleWeatherData(samplePoint.xz);
            float sampledDensity = SampleCloudDensity(samplePoint, weatherData, 0.0, false /*cheapSample*/);
			// if we just sample a zero, increment the counter
            if (sampledDensity == 0.0)
            {
                zeroDensitySampleCount++;
            }
			// if we are doing an expensive sample that is still potentially in the cloud
            if (zeroDensitySampleCount != 6)
            {
                density += sampledDensity;
                if (sampledDensity != 0.0)
                {
					// the accumulating variable for transmittence
                    transmittence *= exp(-densityScale * sampledDensity * stepSize);

					// sample_cloud_density_along_cone just walks in the given direction from the start point and takes 6 number of lighting samples
                    float densityAlongLightRay = 0.0;
                    densityAlongLightRay += SampleCloudDensityAlongCone(samplePoint, sunDirection, coneSpreadMultplier);
                    densityAlongLightRay += SampleCloudDensityLongDistance(samplePoint, sunDirection, coneSpreadMultplier);
			
					// captures the direct lighting from the sun
                    float opticalThickness = densityScale * densityAlongLightRay * coneSpreadMultplier;
                    float rainCloudAbsorption = exp(-weatherData.g) * rainCloudAbsorptionScale;
					// beer's law models the attenuation of light as it passes through a material
#if 1
                    float beersLaw = exp(-opticalThickness * rainCloudAbsorption);
#else
					float beersLaw = max(exp(-opticalThickness * rainCloudAbsorption), exp(-opticalThickness * 0.25 * rainCloudAbsorption) * 0.7);
#endif
					// in-scattering probability function (powdered sugar effect)
                    float powderedSugar = enablePowderedSugarEffect ? 1.0 - exp(-opticalThickness) : 0.5;

                    color += (2.0 * beersLaw * powderedSugar * henyeyGreensteinPhase) * atmosphereColor * transmittence * sampledDensity;
                }
                samplePoint += rayStep;
            }
			// if not, then set cloudTest to zero so that we go back to the cheap sample case
            else
            {
                cloudTest = 0.0;
                zeroDensitySampleCount = 0;
            }
        }
        else
        {
			// sample density the cheap way, only using the low-frequency noise 
            float3 weatherData = SampleWeatherData(samplePoint.xz);
            cloudTest = SampleCloudDensity(samplePoint, weatherData, 0.0, true /*cheapSample*/);
            if (cloudTest == 0.0)
            {
                samplePoint += rayStep;
            }
        }
    }
	
    float alpha = 1.0 - transmittence;
    return max(0.0, float4(color, alpha));
}


float4 main(VS_OUT pin) : SV_TARGET
{
	// from texture-space to ndc
    float4 ndc = float4(2.0 * pin.texcoord.x - 1.0, 1.0 - 2.0 * pin.texcoord.y, 0.0, 1.0);

	// from ndc to world-space
    float4 pos = mul(ndc, inverseViewProjection);
    pos /= pos.w;

    float3 rayDir = normalize(pos.xyz - cameraPositon.xyz);
	
#if 0
	const float4x4 ditherPatterns =
	{
		{ 0.0f, 0.5f, 0.125f, 0.625f },
		{ 0.75f, 0.22f, 0.875f, 0.375f },
		{ 0.1875f, 0.6875f, 0.0625f, 0.5625 },
		{ 0.9375f, 0.4375f, 0.8125f, 0.3125 }
	};
	rayDir *= ditherPatterns[pin.position.x % 4][pin.position.y % 4];
#endif	
	
	
    float3 color = 0.0;
    if (rayDir.y > 0.0)
    {
		// define the center of the earth as the origin of coordinates
        float3 eyePos = float3(0.0, earthRadius, 0.0) + cameraPositon.xyz;
        float3 rayOrigin = eyePos + rayDir * IntersectSphere(eyePos, rayDir, cloudAltitudesMinMax.x);
        float3 rayEndpoint = eyePos + rayDir * IntersectSphere(eyePos, rayDir, cloudAltitudesMinMax.y);
        float shellDist = length(rayEndpoint - rayOrigin);
		
        float steps = rayMarchingSteps;
        if (autoRayMarchingSteps)
        {
            const float attenuation = 0.5625;
#if 1
			// take fewer steps towards horizon
            steps = lerp(rayMarchingSteps * attenuation, rayMarchingSteps, clamp(dot(rayDir, float3(0.0, 1.0, 0.0)), 0.0, 1.0));
#else
			// take more steps towards horizon
			steps = lerp(rayMarchingSteps, rayMarchingSteps * attenuation, clamp(dot(rayDir, float3(0.0, 1.0, 0.0)), 0.0, 1.0));
#endif
        }

		
        float3 rayStep = rayDir * shellDist / steps;
        float4 volume = RayMarch(rayOrigin, rayStep, int(steps));
		
		//return volume;
		
        float3 background = Atmosphere(rayDir);
		// draw cloud shape
        color = background * (1.0 - volume.a) + volume.xyz;
		
		// blend distant clouds into the sky
		//color = lerp(clamp(color, 0.0, 1.0), clamp(background, 0.0, 1.0), smoothstep(0.6, 1.0, 1.0 - rayDir.y));
        color = lerp(max(color, 0.0), max(background, 0.0), smoothstep(0.6, 1.0, 1.0 - rayDir.y));
    }
    else
    {
        color = Atmosphere(rayDir);
    }
	
    return float4(color, 1);
}