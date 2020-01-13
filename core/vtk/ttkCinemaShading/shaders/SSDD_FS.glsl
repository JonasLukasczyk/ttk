R"(
//VTK::System::Dec  // always start with this line
//VTK::Output::Dec  // always have this line in your FS
// Screen Space Depth Darkening (SSDD) based on:
// @book{luft2006image,
//  title={Image enhancement by unsharp masking the depth buffer},
//  author={Luft, Thomas and Colditz, Carsten and Deussen, Oliver},
//  volume={25},
//  number={3},
//  year={2006},
//  publisher={ACM}
// }

uniform sampler2D tex0; // color tex
uniform sampler2D tex1; // original depth tex
uniform sampler2D tex2; // blurred depth tex

varying vec4 vPos;
uniform float uSSDDStrength;

void main(void) {
    float depthOriginal = texture2D( tex1, vPos.xy ).r;
    float depthBlurred = texture2D( tex2, vPos.xy ).r;
    float delta = min(depthBlurred - depthOriginal, 0) * uSSDDStrength;

    vec3 color = texture2D( tex0, vPos.xy ).rgb + delta;
    gl_FragData[0] = vec4( color, 1 );
}

)"