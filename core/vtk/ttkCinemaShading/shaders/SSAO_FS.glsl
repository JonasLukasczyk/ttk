R"(
//VTK::System::Dec  // always start with this line
//VTK::Output::Dec  // always have this line in your FS

uniform sampler2D tex0;
varying vec4 vPos;

uniform vec2 uResolution;
uniform vec2 uCamNearFar;

uniform float uSSAORadius;
uniform float uSSAOScale;
uniform float uSSAODiffArea;

const float gDisplace = 0.5;  // gauss bell center

const int samples = 128;
const float samplesF = float(samples);

#define DL 2.399963229728653  // PI * ( 3.0 - sqrt( 5.0 ) )
#define EULER 2.718281828459045

float readDepth( const in vec2 coord ){
    float cameraFarMinusNear = uSSAOScale - 1.0;
    float cameraFarPlusNear = uSSAOScale + 1.0;

    float z = texture2D( tex0, coord ).r;
    return 2.0 / ( cameraFarPlusNear - z * cameraFarMinusNear );
}

float compareDepths( const in float depth1, const in float depth2, inout int far ) {
    float garea = 8.0;                         // gauss bell width
    float diff = ( depth1 - depth2 ) * 100.0;  // depth difference (0-100)

    // reduce left bell width to avoid self-shadowing
    if(diff<gDisplace){
        garea = uSSAODiffArea;
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
    vec2 pixelSize = 1./uResolution;

    // Computing ao factor
    float ao = 0.0;
    {
        float dz = 1.0 / samplesF;
        float l = 0.0;
        float z = 1.0 - dz / 2.0;

        vec2 adjustedPixelSize = pixelSize/depth;
        for(int i=0; i<samples; i++){
            float r = sqrt( 1.0 - z ) * uSSAORadius;
            float pw = cos( l ) * r;
            float ph = sin( l ) * r;
            ao += calcAO( depth, pw * adjustedPixelSize.x, ph * adjustedPixelSize.y, vPos.xy );
            z = z - dz;
            l = l + DL;
        }

        float isNotBackground = 1.0 - floor( texture2D( tex0, vPos.xy ).r );
        ao *= isNotBackground/samplesF;
    }

    gl_FragData[0] = vec4(vec3(1-ao), 1);

}

)"