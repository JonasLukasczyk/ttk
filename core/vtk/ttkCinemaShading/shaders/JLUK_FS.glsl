R"(
//VTK::System::Dec  // always start with this line
//VTK::Output::Dec  // always have this line in your FS

uniform sampler2D tex0; // color
uniform sampler2D tex1; // depth
varying vec4 vPos;

uniform vec2 uResolution;
uniform vec2 uCamNearFar;

uniform float uJLUKRadius;
uniform float uJLUKScale;
uniform float uJLUKDiffArea;
uniform float uJLUKNoise;
uniform float uJLUKAOFactor;
uniform float uJLUKNormalFactor;
uniform float uJLUKLuminanceFactor;


const int samples = 128;
const float samplesF = float(samples);

#define DL 2.399963229728653  // PI * ( 3.0 - sqrt( 5.0 ) )
#define EULER 2.718281828459045

vec2 rand( const vec2 coord ){
    vec2 noise;
    float nx = dot ( coord, vec2( 12.9898, 78.233 ) );
    float ny = dot ( coord, vec2( 12.9898, 78.233 ) * 2.0 );
    noise = clamp( fract ( 43758.5453 * sin( vec2( nx, ny ) ) ), 0.0, 1.0 );
    return ( noise * 2.0  - 1.0 ) * uJLUKNoise/1000;
}

float readDepth( const in vec2 coord ){
    float cameraFarMinusNear = uJLUKScale - 1.0;
    float cameraFarPlusNear = uJLUKScale + 1.0;

    float z = texture2D( tex1, coord ).r;
    return 2.0 / ( cameraFarPlusNear - z * cameraFarMinusNear );
}

const float gDisplace = 0.5;  // gauss bell center
float compareDepths( const in float depth1, const in float depth2, inout int far ) {
    float garea = 8.0;        // gauss bell width
    float diff = ( depth1 - depth2 ) * 100.0;  // depth difference (0-100)

    // reduce left bell width to avoid self-shadowing
    if(diff<gDisplace){
        garea = uJLUKDiffArea;
    } else {
        far = 1;
    }

    float dd = diff - gDisplace;
    return pow( EULER, -2.0 * ( dd * dd ) / ( garea * garea ) );
}

float calcAO( float depth, float dw, float dh, vec2 uv ) {
    vec2 vv = vec2( dw, dh );
    vec2 coord1 = uv + vv;
    vec2 coord2 = uv - vv;
    float temp1 = 0.0;
    float temp2 = 0.0;
    int far = 0;

    temp1 = compareDepths( depth, readDepth( coord1 ), far );
    if ( far > 0 ) {
        temp2 = compareDepths( readDepth( coord2 ), depth, far );
        temp1 += ( 1.0 - temp1 ) * temp2;
    }
    return temp1;
}

void main() {
    float depth = readDepth( vPos.xy );
    vec2 noise = rand(vPos.xy);
    vec2 pixelSize = 1./uResolution;

    float isNotBackground = 1.0 - floor( texture2D( tex1, vPos.xy ).r );

    // Computing ao factor
    float ao = 0.0;
    {
        float dz = 1.0 / samplesF;
        float l = 0.0;
        float z = 1.0 - dz / 2.0;

        vec2 adjustedPixelSize = pixelSize/depth + noise - noise*noise;
        for(int i=0; i<samples; i++){
            float r = sqrt( 1.0 - z ) * uJLUKRadius;
            float pw = cos( l ) * r;
            float ph = sin( l ) * r;
            ao += calcAO( depth, pw * adjustedPixelSize.x, ph * adjustedPixelSize.y, vPos.xy );
            z = z - dz;
            l = l + DL;
        }


        ao /= samplesF;
        ao = 1.0 - ao*uJLUKAOFactor;
    }

    // Get Diffuse Color
    vec3 color = texture2D( tex0, vPos.xy ).rgb;

    // Compute Luminance
    vec3 lumcoeff = vec3( 0.299, 0.587, 0.114 );
    vec3 luminance = vec3( dot( color, lumcoeff ) );

    // Silhouette Effect
    vec3 eps = vec3( pixelSize.x, pixelSize.y, 0 );
    float dxdz = texture2D(tex1, vPos.xy + eps.xz).r - texture2D(tex1, vPos.xy - eps.xz).r;
    float dydz = texture2D(tex1, vPos.xy + eps.yz).r - texture2D(tex1, vPos.xy - eps.yz).r;
    vec3 n = normalize( vec3(dxdz, dydz, 1./uJLUKNormalFactor) );
    vec3 lightPos = vec3(0,0,1);
    float lightInt = 1.5*dot(n,normalize(lightPos));

    vec3 cAO = vec3( color * mix( vec3(ao), vec3(1.0), luminance * uJLUKLuminanceFactor ) );

    vec3 final = isNotBackground > 0.5
        // ? cAO
        ? cAO*0.2 + cAO*lightInt
        // ? n
        : vec3(1);

    gl_FragData[0] = vec4(final, 1);

}

)"