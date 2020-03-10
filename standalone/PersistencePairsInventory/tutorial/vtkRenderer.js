'use strict'; /* globals $ THREE dat*/
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
        this.renderer.setClearColor('#fff',0);
        this.container.append(this.renderer.domElement);

        // Cameras
        this.cameraFP = new THREE.PerspectiveCamera(45, this.width/this.height, 0.001, 6);
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

        this.Scale = 64;

        // add dummy object for testing
        // {
        //     const geometry = new THREE.SphereBufferGeometry( 1, 10, 10 );
        //     const obj = new THREE.Mesh( geometry, this.objectMaterial );
        //     this.sceneFP.add(obj);
        // }
        // {
        //     const geometry = new THREE.PlaneBufferGeometry( 3, 3 );
        //     const obj = new THREE.Mesh( geometry, this.objectMaterial );
        //     this.sceneFP.add(obj);
        // }

        this.sceneSSAO = new THREE.Scene();
        {
            this.ssaoUniforms = {
                // TOGO
                lightPos: { type:'3f', value: [1,0,0]},

                tex: { type:'t', value: this.forwardPassRT.texture},

                uResolution: { type:'2f', value: [this.width,this.height]},
                uCamNearFar: { type:'2f', value: [this.cameraFP.near,this.cameraFP.far]},

                uJLUKRadius: { type:'f', value: 5},
                uJLUKScale: { type:'f', value: 1},
                uJLUKDiffArea: { type:'f', value: 0.5},
                uJLUKNoise: { type:'f', value: 1},
                uJLUKAOFactor: { type:'f', value: 1},
                uJLUKNormalFactor: { type:'f', value: 10},
                uJLUKLuminanceFactor: { type:'f', value: 0.01}
            };

            {
                this.gui = new dat.GUI({ autoPlace: false });
                this.container.append(this.gui.domElement);

                this.gui.add(this,'Scale',1,256,1)
                    .onChange( ()=>this.scaleScene() );
                this.gui.add(this.ssaoUniforms.uJLUKRadius,'value',0.1,100,1)
                    .name( 'Radius' )
                    .onChange( ()=>this.render() );
                this.gui.add(this.ssaoUniforms.uJLUKScale,'value',0.1,4,0.1)
                    .name( 'Falloff' )
                    .onChange( ()=>this.render() );
                this.gui.add(this.ssaoUniforms.uJLUKDiffArea,'value',0.1,2,0.1)
                    .name( 'DiffArea' )
                    .onChange( ()=>this.render() );
                this.gui.add(this.ssaoUniforms.uJLUKAOFactor,'value',0.1,2,0.1)
                    .name( 'AO' )
                    .onChange( ()=>this.render() );
                this.gui.add(this.ssaoUniforms.uJLUKNormalFactor,'value',0.1,100,0.1)
                    .name( 'Normals' )
                    .onChange( ()=>this.render() );
                this.gui.add(this.ssaoUniforms.uJLUKLuminanceFactor,'value',0.01,10,0.01)
                    .name( 'Luminance' )
                    .onChange( ()=>this.render() );

                this.gui.close();
            }

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

        this.scaleScene();
    }

    resetCamera(){
        this.cameraFP.position.set(
            1.2,
            1.2,
            1.2
        );

        this.controls.target.set(
            0.5,
            0.5,
            0.5
        );
        this.controls.update();

        this.render();
    }

    setScene(vtkJson){
        console.log("vtkRender:", vtkJson);

        for(let i=this.sceneFP.children.length-1; i>=0; i--)
            this.sceneFP.remove(this.sceneFP.children[i]);

        if (! vtkJson.hasOwnProperty("PointCoords")) {
            const empty = new Int32Array(0);
            vtkJson.PointCoords = { "Values": empty };
            vtkJson.ConnectivityList = { "Values": empty};
        }

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

        this.scaleScene();
    }

    scaleScene(){
        const scale = 1/Math.max(this.Scale,Math.max(this.Scale,this.Scale));
        for(let i=this.sceneFP.children.length-1; i>=0; i--){
            this.sceneFP.children[i].position.set( -scale*this.Scale/2,-scale*this.Scale/2,-scale*this.Scale/2 );
            this.sceneFP.children[i].scale.set( scale,scale,scale );
        }

        this.render();
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