'use strict'; /* globals $ THREE */
class vtkRenderer{
    constructor(containerID, width, height){
        // Parameters
        this.containerID = containerID;
        this.container = $('#'+this.containerID);
        this.width = width;
        this.height = height;

        // Renderer
        this.renderer = new THREE.WebGLRenderer({
            antialias: true
        });
        this.renderer.setSize(this.width, this.height);
        this.renderer.setClearColor('#fff',1);
        this.container.append(this.renderer.domElement);

        // Camera
        // this.camera = new THREE.OrthographicCamera( -1, 1, 1, -1, 1, 1000 );
        this.camera = new THREE.PerspectiveCamera(90, 1, 0.0001, 10000);
        this.camera.position.set(0,0,1);

        this.controls = new THREE.OrbitControls( this.camera, this.renderer.domElement );
        this.controls.addEventListener(
            'change',
            ()=>this.render()
        );

        // Scene
        this.scene = new THREE.Scene();

        var geometry = new THREE.PlaneBufferGeometry( 1, 1 );
        var material = new THREE.MeshBasicMaterial( {color: 0xffff00, side: THREE.DoubleSide} );
        var plane = new THREE.Mesh( geometry, material );
        this.scene.add(plane);

        this.render();
    }


    setScene(vtkJson){
        console.log(vtkJson);

        // setTimeout(()=>{
            for(let i=this.scene.children.length-1; i>=0; i--)
                this.scene.remove(this.scene.children[i]);

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
                const material = new THREE.MeshBasicMaterial( { color: 0xff0000, side: THREE.DoubleSide } );
                const mesh = new THREE.Mesh( geometry, material );
                this.scene.add(mesh);
            }
            {
                const material = new THREE.MeshBasicMaterial( { color: 0x00000, side: THREE.DoubleSide, wireframe:true } );
                const mesh = new THREE.Mesh( geometry, material );
                this.scene.add(mesh);
            }

            this.resetCamera();

        // }, 200);
    }

    resetCamera(){
        const focus = this.scene.children[0];

        focus.geometry.computeBoundingSphere();
        const bs = focus.geometry.boundingSphere;

        this.camera.position.set(bs.center.x+1.5*bs.radius,bs.center.y,bs.center.z);

        this.controls.target.set(bs.center.x,bs.center.y,bs.center.z);

        this.controls.update();

        this.render();
        // const pos = focus.
    }

    render(){
        this.renderer.render(this.scene, this.camera);
    }
}