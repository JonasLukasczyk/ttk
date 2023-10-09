precision highp float;

// varying float vScalar;
// uniform sampler2D colorMap;

varying vec3 vColor;
varying vec3 vPosition;
varying float vDepth;

void main(){

    // vec3 final = texture2D(colorMap, vec2(vScalar,0)).rgb;

    // vec3 final = color;
    // vec3 final = vec3(1,0,0);
    // final = vPosition.y>0.325 ? vec3(0.4) : final;

    gl_FragColor = vec4(
        vColor,
        vDepth
    );
}