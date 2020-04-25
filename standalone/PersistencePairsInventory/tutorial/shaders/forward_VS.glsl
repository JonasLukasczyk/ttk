precision highp float;

attribute vec3 position;
attribute vec3 color;
varying vec3 vPosition;

uniform mat4 projectionMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelMatrix;

varying float vDepth;
varying vec3 vColor;

void main(){
    gl_Position = projectionMatrix * modelViewMatrix * vec4( position, 1 );

    vPosition = position;
    vDepth = gl_Position.w;
    vColor = color;
}