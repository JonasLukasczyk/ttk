'use strict'; /* globals $ THREE dat*/

const HD = false;


class vtkRenderer{
    constructor(containerID, width, height){
        // Parameters

        if(HD){
            width *= 4;
            height *= 4;
        }

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
            alpha: true,
            preserveDrawingBuffer: true
        });
        this.renderer.setSize(this.width, this.height);
        this.renderer.setClearColor('#fff',0);
        this.container.append(this.renderer.domElement);

        if(HD){
            $(this.renderer.domElement).css('width','700px');
            $(this.renderer.domElement).css('height','700px');
        }

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

        // this.scale = 64;
        this.scale = 255;
        // this.scale = 128;

        // add dummy object for testing
        {
            if(this.scale===64){
                const geometry = new THREE.CylinderBufferGeometry( 32, 32, 5, 80 );
                // const geometry = new THREE.SphereBufferGeometry( 64, 10, 10 );
                geometry.translate(32,52,32);
                // const geometry = new THREE.CylinderBufferGeometry( 1, 1, 1 );

                const nVertices = geometry.attributes.position.array.length;
                const colors = new Uint8Array( nVertices*3 );
                for(let i=0; i<nVertices*3; i++)
                    colors[i] = 150;
                geometry.setAttribute( 'color', new THREE.BufferAttribute( colors, 3, true ) );

                const obj = new THREE.Mesh( geometry, this.objectMaterial );
                this.sceneFP.add(obj);
            } else if (this.scale===255) {
                const geometry = new THREE.BoxBufferGeometry( 256, 256, 256 );
                geometry.translate(128,128,128);

                const nVertices = geometry.attributes.position.array.length;
                const colors = new Uint8Array( nVertices*3 );
                for(let i=0; i<nVertices*3; i++)
                    colors[i] = 240;
                geometry.setAttribute( 'color', new THREE.BufferAttribute( colors, 3, true ) );

                const mat = this.objectMaterial.clone();
                mat.side = THREE.BackSide;
                // // // mat.wireframe = true;
                // // // mat.wireframeLineWidth = 2;

                // var edges = new THREE.EdgesGeometry( geometry );

                const obj = new THREE.Mesh( geometry, mat );
                this.sceneFP.add(obj);
            } else if (this.scale===128) {
                const geometry = new THREE.BoxBufferGeometry( 128, 128, 256 );
                geometry.translate(64,64,128);

                const nVertices = geometry.attributes.position.array.length;
                const colors = new Uint8Array( nVertices*3 );
                for(let i=0; i<nVertices*3; i++)
                    colors[i] = 240;
                geometry.setAttribute( 'color', new THREE.BufferAttribute( colors, 3, true ) );

                const mat = this.objectMaterial.clone();
                mat.side = THREE.BackSide;
                // // // mat.wireframe = true;
                // // // mat.wireframeLineWidth = 2;

                // var edges = new THREE.EdgesGeometry( geometry );

                const obj = new THREE.Mesh( geometry, mat );
                this.sceneFP.add(obj);
            }
        }
        // add dummy object for testing
        // {
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
                uJLUKNoise: { type:'f', value: 0.5},
                uJLUKAOFactor: { type:'f', value: 1},
                uJLUKNormalFactor: { type:'f', value: 10},
                uJLUKLuminanceFactor: { type:'f', value: 0.01}
            };

            {
                this.gui = new dat.GUI({ autoPlace: false });
                this.container.append(this.gui.domElement);

                this.gui.add(this,'scale',1,256,1)
                    .onChange( ()=>this.scaleScene() );
                this.gui.add(this.ssaoUniforms.uJLUKRadius,'value',0.1,100,1)
                    .name( 'Radius' )
                    .onChange( ()=>this.render() );
                this.gui.add(this.ssaoUniforms.uJLUKNoise,'value',0,2,0.1)
                    .name( 'Noise' )
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

        // hidden "Open Controls" in Render View
        $('#RendererContainer .dg.main').hide() ;
        this.resetCamera();
    }

    resetCamera(){

        if(this.scale===128){
            this.cameraFP.position.set(
                2.8,
                0,
                0.5
            );

            this.controls.target.set(
                0,
                0,
                0.5
            );
        } else {
            this.cameraFP.position.set(
                1.5*1,
                1.5*0.05,
                0
            );

            this.controls.target.set(
                0,
                0.05,
                0
            );
        }
        this.controls.update();

        this.render();
    }

    setScene(vtkJson){
        console.log("vtkRender received the data",vtkJson) ;
        for(let i=this.sceneFP.children.length-1; i>=1; i--)
            this.sceneFP.remove(this.sceneFP.children[i]);

        var geometry = new THREE.BufferGeometry();
        geometry.setAttribute( 'position', new THREE.BufferAttribute(
          Float32Array.from(vtkJson.points.coordinates.data), 3 )
        );

        if(this.scale===64){
            const scaleHalf = this.scale/2;
            geometry.translate(-scaleHalf,-scaleHalf,-scaleHalf);
            geometry.rotateX(-Math.PI/2);
            geometry.translate(scaleHalf,scaleHalf,scaleHalf);
        }
        if(this.scale===128){
            const scaleHalf = this.scale/2;
            geometry.translate(-scaleHalf,-scaleHalf,-scaleHalf);
            geometry.rotateX(-Math.PI/2);
            geometry.translate(scaleHalf,scaleHalf,3*scaleHalf);
        }

        const connectivityList = vtkJson.cells.connectivityArray.data;

        // const indices = new Uint32Array(connectivityList.length/4*3);
        // for(let i=0,q=0; i<connectivityList.length; i+=4){
        //     indices[q++] = parseInt(connectivityList[i+1]);
        //     indices[q++] = parseInt(connectivityList[i+2]);
        //     indices[q++] = parseInt(connectivityList[i+3]);
        // }
        const indices = new Uint32Array(connectivityList.length);
        for(let i=0; i<connectivityList.length; i++)
          indices[i] = Number(connectivityList[i]);
        geometry.setIndex( new THREE.BufferAttribute(indices,1));

        console.log(vtkJson);

        if(vtkJson.hasOwnProperty("pointData") && vtkJson.pointData.hasOwnProperty('RegionId')){
            const regionIds = vtkJson.pointData.RegionId.data;

            const colorMap = [
                228,26,28,
                55,126,184,
                77,175,74,
                152,78,163,
                255,127,0,
                255,255,51,
                166,86,40,
                247,129,191
            ];
            // const colorMap = [
            //     166,206,227,
            //     31,120,180,
            //     178,223,138,
            //     51,160,44,
            //     251,154,153,
            //     227,26,28,
            //     253,191,111,
            //     255,127,0
            // ];
            const nVertices = regionIds.length;
            const colors = new Uint8Array( nVertices*3  );
            for(let i=0,q=0; i<nVertices; i++){
                let index = 3*(regionIds[i]%8);
                colors[q++] = colorMap[index++];
                colors[q++] = colorMap[index++];
                colors[q++] = colorMap[index++];
            }
            geometry.setAttribute( 'color', new THREE.BufferAttribute( colors, 3, true ) );
        }

        {
            const mesh = new THREE.Mesh( geometry, this.objectMaterial );

            // const edgeMat = this.objectMaterial.clone();
            // edgeMat.wireframe = true;
            // // console.log(edgeMat);
            // edgeMat.wireframeLinewidth = 2;
            // const cg2 = geometry.clone();
            // const nVertices = vtkJson.PointCoords.Values.length;
            // const colors = new Uint8Array( nVertices*3  );
            // for(let i=0,q=0; i<nVertices; i++){
            //     colors[q++] = 30;
            //     colors[q++] = 30;
            //     colors[q++] = 30;
            // }
            // cg2.setAttribute( 'color', new THREE.BufferAttribute( colors, 3, true ) );
            // const mesh2 = new THREE.Mesh( cg2, edgeMat );
            // this.sceneFP.add(mesh2);

            this.sceneFP.add(mesh);
        }

        this.scaleScene();
    }

    scaleScene(){
        const scale = 1/Math.max(this.scale,Math.max(this.scale,this.scale));
        for(let i=this.sceneFP.children.length-1; i>=0; i--){
            this.sceneFP.children[i].position.set( -scale*this.scale/2,-scale*this.scale/2,-scale*this.scale/2 );
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

        $("#RendererContainer").loading("stop") ;
    }
}
