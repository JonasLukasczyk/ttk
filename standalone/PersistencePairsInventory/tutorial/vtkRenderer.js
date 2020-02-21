'use strict'; /* globals $ THREE */
class vtkRenderer{
    constructor(containerID, width, height){
        // Parameters
        this.containerID = containerID;
        this.container = $('#'+this.containerID);
        this.width = width;
        this.height = height;

        console.log('Loading Resources', this);
        this.resources = {
            'forward_VS': null,
            'forward_FS': null,
            'fullScreenQuad_VS': null,
            'SSAO_FS': null,
            'FXAA_VS': null,
            'FXAA_FS': null
        };
        let resourcesToLoad = Object.keys(this.resources).length;
        let resourcesLoaded = 0;
        const loadResource = resource => {
            $.ajax({
                type:   'GET',
                url:    './shaders/'+resource+'.glsl',
                dataType: 'text',
                success: result => {
                    this.resources[resource] = result;
                    if(++resourcesLoaded>=resourcesToLoad)
                        this.init();
                }
            });
        };

        for(var resource in this.resources)
            loadResource(resource);
    }

    init(){
        // Renderer
        this.renderer = new THREE.WebGLRenderer({
            antialias: true,
            alpha: true
        });
        this.renderer.setSize(this.width, this.height);
        this.renderer.setClearColor('#fff',1);
        this.container.append(this.renderer.domElement);

        // Cameras
        this.cameraFP = new THREE.PerspectiveCamera(90, 1, 0.01, 1000);
        this.cameraFP.position.set(0,0,1);
        this.cameraPP = new THREE.OrthographicCamera( -1, 1, 1, -1, 1, 1000 );
        this.cameraPP.position.set(0,0,1);

        // Controls
        this.controls = new THREE.OrbitControls( this.cameraFP, this.renderer.domElement );
        this.controls.addEventListener(
            'change',
            ()=>this.render()
        );

        this.forwardPassRT = new THREE.WebGLRenderTarget(
            this.width,
            this.height,
            {
                minFilter: THREE.NearestFilter,
                magFilter: THREE.NearestFilter,
                wrapS:  THREE.ClampToEdgeWrapping,
                wrapT:  THREE.ClampToEdgeWrapping,
                format: THREE.RGBAFormat,
                type: THREE.FloatType,
                generateMipmaps: false
            }
        );

        this.SSAO_RT = new THREE.WebGLRenderTarget(
            this.width,
            this.height,
            {
                wrapS:  THREE.ClampToEdgeWrapping,
                wrapT:  THREE.ClampToEdgeWrapping,
                format: THREE.RGBAFormat
            }
        );

        // Main Material
        this.objectMaterial = new THREE.RawShaderMaterial( {
            vertexShader:   this.resources.forward_VS,
            fragmentShader: this.resources.forward_FS,
            side: THREE.DoubleSide
        });

        // Scene
        this.sceneFP = new THREE.Scene();

        // add dummy object for testing
        {
            const geometry = new THREE.SphereBufferGeometry( 1, 10, 10 );
            const obj = new THREE.Mesh( geometry, this.objectMaterial );
            this.sceneFP.add(obj);
        }
        {
            const geometry = new THREE.PlaneBufferGeometry( 3, 3 );
            const obj = new THREE.Mesh( geometry, this.objectMaterial );
            this.sceneFP.add(obj);
        }

        this.sceneSSAO = new THREE.Scene();
        {
            this.ssaoUniforms = {
                // TOGO
                lightPos: { type:'3f', value: [0,0,1]},

                tex: { type:'t', value: this.forwardPassRT.texture},

                uResolution: { type:'2f', value: [this.width,this.height]},
                uCamNearFar: { type:'2f', value: [this.cameraFP.near,this.cameraFP.far]},

                uJLUKRadius: { type:'f', value: 2},
                uJLUKScale: { type:'f', value: 1},
                uJLUKDiffArea: { type:'f', value: 0.4},
                uJLUKNoise: { type:'f', value: 2},
                uJLUKAOFactor: { type:'f', value: 1},
                uJLUKNormalFactor: { type:'f', value: 1},
                uJLUKLuminanceFactor: { type:'f', value: 0.1}
            };

            const quad = new THREE.Mesh(
                new THREE.PlaneBufferGeometry(2,2),
                new THREE.RawShaderMaterial({
                    uniforms: this.ssaoUniforms,
                    vertexShader:   this.resources.fullScreenQuad_VS,
                    fragmentShader: this.resources.SSAO_FS
                })
            );
            this.sceneSSAO.add(quad);
        }

        this.sceneFXAA = new THREE.Scene();
        {
            this.fxaaUniforms = {
                tDiffuse: { type:'t', value: this.SSAO_RT.texture},
                resolution: { type:'2f', value: [this.width,this.height]},
            };

            const quad = new THREE.Mesh(
                new THREE.PlaneBufferGeometry(2,2),
                new THREE.RawShaderMaterial({
                    uniforms: this.fxaaUniforms,
                    vertexShader:   this.resources.FXAA_VS,
                    fragmentShader: this.resources.FXAA_FS
                })
            );
            this.sceneFXAA.add(quad);
        }

        this.resetCamera();
    }

    setScene(vtkJson){
        console.log(vtkJson);

        // setTimeout(()=>{
            for(let i=this.sceneFP.children.length-1; i>=0; i--)
                this.sceneFP.remove(this.sceneFP.children[i]);

            var geometry = new THREE.BufferGeometry();
            geometry.setAttribute( 'position', new THREE.BufferAttribute( vtkJson.PointCoords.Values, 3 ) );

            const connectivityList = vtkJson.ConnectivityList.Values;

            const indices = new Uint32Array(connectivityList.length/4*3);
            for(let i=0,q=0; i<connectivityList.length; i+=4){
                indices[q++] = parseInt(connectivityList[i+1]);
                indices[q++] = parseInt(connectivityList[i+2]);
                indices[q++] = parseInt(connectivityList[i+3]);
            }
            geometry.setIndex( new THREE.BufferAttribute(indices,1) );

            {
                const mesh = new THREE.Mesh( geometry, this.objectMaterial );
                this.sceneFP.add(mesh);
            }

            this.resetCamera();

        // }, 200);
    }

    resetCamera(){
        const focus = this.sceneFP.children[0];

        focus.geometry.computeBoundingSphere();
        const bs = focus.geometry.boundingSphere;

        this.cameraFP.position.set(bs.center.x+1.5*bs.radius,bs.center.y,bs.center.z);

        this.controls.target.set(bs.center.x,bs.center.y,bs.center.z);
        this.controls.update();

        this.render();
        // const pos = focus.
    }

    render(){
        this.renderer.setRenderTarget(this.forwardPassRT);
        this.renderer.render( this.sceneFP, this.cameraFP);

        this.renderer.setRenderTarget(this.SSAO_RT);
        // this.renderer.setRenderTarget(null);
        this.renderer.render( this.sceneSSAO, this.cameraPP);

        this.renderer.setRenderTarget(null);
        this.renderer.render( this.sceneFXAA, this.cameraPP);
    }
}