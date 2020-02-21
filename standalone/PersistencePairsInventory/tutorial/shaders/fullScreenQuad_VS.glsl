precision highp float;

attribute vec3 position;
varying vec4 vPos;

void main(){
    vPos = vec4(position,1)/2. + vec4(0.5);
    gl_Position = vec4(position,1);
}