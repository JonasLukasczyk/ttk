class KDERenderer {
  constructor(leafletMap){

    this.vtkDataSet = null;

    // Renderer
    this.renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true
    });

    this.mask = null;

    // Camera
    this.camera = new THREE.OrthographicCamera( -1, 1, 1, -1, 0.1, 2 );
    this.camera.position.set(0,0,1);

    // Scene
    this.scene = new THREE.Scene();

    this.consts = {
      opacity: 0.8,
      resolution: [0,0],
      nContours: 7,
      contourWidth: 1,
      nHatching: 7,
      hatchingWidth: 1,
    };

    this.planeMaterial = new THREE.MeshBasicMaterial({color:'red'});
    this.uniforms = {
      texColor: {type:'t', value: null},
      texKDE: {type:'t', value: null},
      texMask: {type:'t', value: null},
      texSelection: {type:'t', value: null},
    };

    this.quad = new THREE.Mesh(
        new THREE.PlaneBufferGeometry(2,2),
        this.planeMaterial
    );
    this.scene.add(this.quad);

    // Leaflet
    this.leafletMap = leafletMap;
    this.leafletMap.on('zoomstart', ()=>{
        this.renderer.domElement.style.display = 'none';
    });
    this.leafletMap.on('zoomend', ()=>{
        this.renderer.domElement.style.display = 'block';
    });
  }

  appendCanvas(lat0, lon0, lat1, lon1){
    if(!this.imageOverlay){
      // Create empty ImageOverlay
      this.imageOverlay = new L.ImageOverlay(
          '',	// no image
          [[lon0,lat0], [lon1, lat1]] // image bounds
      );

      this.imageOverlay.addTo(this.leafletMap);

      // Exchange the image of the ImageOverlay with the WebGL canvas
      this.imageOverlay._image.parentElement.replaceChild(
          this.renderer.domElement,	// new Element
          this.imageOverlay._image		// old Element
      );
      this.imageOverlay._image = this.renderer.domElement;

      // Force Leaflet to update the canvas size and position
      this.leafletMap.setView([(lon0 + lon1) / 2, (lat0 + lat1) / 2], 12);
    }
  }

  getVertexShader(){
      return `
precision highp float;

attribute vec3 position;
varying vec2 vUV;

void main(){
    vUV = position.xy/2. + vec2(0.5);
    gl_Position = vec4(position,1);
}
      `;
  }

  getFragmentShader(){
        return `
#extension GL_OES_standard_derivatives : enable
precision highp float;

uniform sampler2D texColor;
uniform sampler2D texKDE;
uniform sampler2D texMask;
uniform sampler2D texSelection;

const float opacity = ${this.consts.opacity};
// const vec2 resolution = vec2(${this.consts.resolution[0]},${this.consts.resolution[1]});
varying vec2 vUV;

float isolineIntensity(float scalar, float n, float w){

    float v = scalar*n+0.1;

    float d = fract(v);
    if(mod(v, 2.0) > 1.) d = 1.-d;

    return clamp( d/(w*fwidth(v)), 0.0 , 1.0);
    // return v<0.5 ? 1.0 : clamp( d/(w*fwidth(v)), 0.0 , 1.0);
}

void main() {
    float nColors = 12.0;

    float hatching = isolineIntensity(10.0 * (vUV.x + vUV.y), ${(8*this.consts.nHatching).toFixed(1)}, ${(1/2 * this.consts.hatchingWidth).toFixed(2)});
    float iso = isolineIntensity(texture2D(texKDE, vUV).a, ${(2*this.consts.nContours).toFixed(1)}, ${this.consts.contourWidth.toFixed(2)});
    float mask = texture2D(texMask, vUV).a*255.0;
    float selection = texture2D(texSelection, vUV).a*255.0;

    vec4 isoColor = vec4(0,0,0,1.0-iso);
    vec3 catColor = texture2D( texColor, vec2(
        0,
        mod(mask,nColors)/15.0
      )).rgb;
    
    vec4 isoCatColor = vec4(catColor * iso * opacity, opacity);
    
    vec4 color = vec4(0,0,0,0);
    if (mask < 254.5) {
      if (selection == 1.0) {
        color = mix(vec4(0, 0, 0, 0.6), isoCatColor, hatching);
      } else {
        color = isoCatColor;
      }
    }
    
    gl_FragColor = isoColor + color;
}
        `;
    }

  createTexture(nComponents, resolution, array){
    return new THREE.DataTexture(
      array,
      resolution[0],
      resolution[1],
      nComponents===4 ? THREE.RGBAFormat : nComponents===3 ? THREE.RGBFormat : THREE.AlphaFormat,
      array.constructor === Float32Array ? THREE.FloatType : THREE.UnsignedByteType,
      THREE.UVMapping,
      THREE.ClampToEdgeWrapping,
      THREE.ClampToEdgeWrapping,
      THREE.NearestFilter,
      THREE.NearestFilter,
      1
    );
  }

  hexToRgb(hex) {
    // from https://stackoverflow.com/questions/5623838/rgb-to-hex-and-hex-to-rgb
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : null;
  }

  setColorMap(cscheme, cmap) {
    let elems = [];
    cscheme.forEach(cHex => {
      const __tmp = this.hexToRgb(cHex);
      elems = elems.concat([__tmp.r, __tmp.g, __tmp.b]);
    });
    elems = elems.concat(new Array((16 - cscheme.length) * 3).fill(0));  // fill with zeroes, so we get a 2^x size

    this.colorScheme = new Uint8Array(elems);
    this.colorMapping = cmap;
    this.colorMapping[255] = 255;

    this.uniforms.texColor = {type:'t', value: this.createTexture(3,[1,16],this.colorScheme)};
    this.uniforms.texColor.value.needsUpdate = true;

    this.update_render();
  }

  computeSelection(branchId_list, scalar_value) {
    const n = this.mask.length;
    const kde = this.vtkDataSet.pointData.KDE.data;
    const branchIds = this.vtkDataSet.pointData.BranchId.data;
    for (let i = 0; i < n; i++) {
      if (branchId_list.includes(branchIds[i]) && kde[i] >= scalar_value) {
        this.selection[i] = 1;
      } else {
        this.selection[i] = 0;
      }
    }

    this.uniforms.texSelection.value.needsUpdate = true;
  }

  computeMaskNoSelection() {
    if (this.mask === null) {
      return;
    }

    const n = this.mask.length;
    const kde = this.vtkDataSet.pointData.KDE.data;
    const branchIds = this.vtkDataSet.pointData.BranchId.data;
    for (let i = 0; i < n; i++) {
      this.mask[i] = this.colorMapping[this.tree.getColorIDbyBranchIdAndScalar(branchIds[i], kde[i].toString())];
    }

    this.uniforms.texMask.value.needsUpdate = true;
  }

  setVtkDataSet(vtkDataSet){

    this.vtkDataSet = vtkDataSet;

    const size = new THREE.Vector2();
    this.renderer.getSize(size);
    const hd = 1.0;
    const resX = this.vtkDataSet.dimension[0] * hd;
    const resY = this.vtkDataSet.dimension[1] * hd;

    // update size if necessary (possible optimization: only create data textures if no swap possible)
    if(size.x!==this.vtkDataSet.dimension[0] || size.y!==this.vtkDataSet.dimension[1]){
      this.renderer.setSize(
        resX, resY
      );
    } else {
    }

    this.consts.resolution = [resX, resY];

    const uniforms = this.uniforms;

    uniforms.texKDE.value = this.createTexture(1,vtkDataSet.dimension, vtkDataSet.pointData.KDE.data);

    this.segmentation = vtkDataSet.pointData.NodeId.data;

    this.mask = new Uint8Array(vtkDataSet.dimension[0]*vtkDataSet.dimension[1]).fill(255);
    this.selection = new Uint8Array(vtkDataSet.dimension[0]*vtkDataSet.dimension[1]).fill(0);
    uniforms.texMask.value = this.createTexture(1,vtkDataSet.dimension, this.mask);
    uniforms.texSelection.value = this.createTexture(1, vtkDataSet.dimension, this.selection);

    this.appendCanvas(
      vtkDataSet.origin[0],
      vtkDataSet.origin[1],
      vtkDataSet.origin[0]+vtkDataSet.spacing[0]*vtkDataSet.dimension[0],
      vtkDataSet.origin[1]+vtkDataSet.spacing[1]*vtkDataSet.dimension[1]
    );

    this.computeMaskNoSelection();
  }

  setTree(tree) {
    this.tree = tree;
  }

  render(opacity, nContours, contourWidth){
    this.consts.opacity = opacity.toFixed(2);
    this.consts.nContours = nContours;
    this.consts.contourWidth = contourWidth;

    this.update_render();
  }

  update_render() {
    this.quad.material = new THREE.RawShaderMaterial({
      vertexShader: this.getVertexShader(),
      fragmentShader: this.getFragmentShader(),
      uniforms: this.uniforms
    });

    this.renderer.render( this.scene, this.camera );
  }

  update_nContours(n) {
    this.consts.nContours = n;
    this.update_render();
  }

  update_nHatching(n) {
    this.consts.nHatching = n;
    this.update_render();
  }

  update_HatchingWidth(n) {
    this.consts.hatchingWidth = n;
    this.update_render();
  }
}