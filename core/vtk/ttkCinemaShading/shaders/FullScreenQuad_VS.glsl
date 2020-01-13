R"(
//VTK::System::Dec

attribute vec4 vertexMC;

varying vec4 vPos;

void main () {
    vPos = vertexMC/2. + vec4(0.5,0.5,0.5,0);
    gl_Position = vertexMC;
}
)"