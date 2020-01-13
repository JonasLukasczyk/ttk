R"(
//VTK::System::Dec  // always start with this line
//VTK::Output::Dec  // always have this line in your FS

uniform sampler2D tex0;

uniform vec2 uResolution;
uniform vec2 uBlurDirection;
uniform float uBlurRadius;

varying vec4 vPos;

vec3 blur13(vec2 uv, vec2 resolution, vec2 direction) {
  vec3 color = vec3(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction / resolution;
  vec2 off2 = vec2(3.2941176470588234) * direction / resolution;
  vec2 off3 = vec2(5.176470588235294) * direction / resolution;

  color += texture2D(tex0, uv).rgb * 0.1964825501511404;
  color += texture2D(tex0, uv + off1).rgb * 0.2969069646728344;
  color += texture2D(tex0, uv - off1).rgb * 0.2969069646728344;
  color += texture2D(tex0, uv + off2).rgb * 0.09447039785044732;
  color += texture2D(tex0, uv - off2).rgb * 0.09447039785044732;
  color += texture2D(tex0, uv + off3).rgb * 0.010381362401148057;
  color += texture2D(tex0, uv - off3).rgb * 0.010381362401148057;
  return color;
}

void main(){
    vec3 blurred = blur13(vPos.xy, uResolution, uBlurDirection * uBlurRadius);
    gl_FragData[0] = vec4(blurred, 1);
}

)"