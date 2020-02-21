precision highp float;

attribute vec3 position;
varying vec3 vPosition;

uniform mat4 projectionMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelMatrix;

varying float vDepth;

void main(){
    vPosition = position;
    gl_Position = projectionMatrix * modelViewMatrix * vec4( position, 1 );
    // vDepth = gl_Position.z;
    vDepth = gl_Position.w;
}