R"(
//VTK::System::Dec  // always start with this line
//VTK::Output::Dec  // always have this line in your FS

uniform sampler2D tex0;
varying vec4 vPos;

void main(void) {
    vec3 color = texture2D( tex0, vPos.xy ).rgb;
    gl_FragData[0] = vec4( color, 1 );
}

)"