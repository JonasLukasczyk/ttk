
// -------------------------------------------- code ------------------------------------------------------
let DUPLICATE = 1 ;
let BIG_ENDIANNESS = 2 ;
let LITTLE_ENDIANNESS = 3 ;

// object sending
let OBJECT_WILL_SENDING = 5 ;
let OBJECT_ACK_OBJECT = 6 ;
let OBJECT_WILL_FINISH = 7 ;
let OBJECT_ACK_FINISH = 8 ;
// -------------------------------------------- code ------------------------------------------------------

// data type
let DATA_FLOAT_ARRAY = 10 ;
let DATA_LONG_ARRAY = 8 ;
let DATA_UNSIGNED_ARRAY = 3 ;
let DATA_INT_ARRAY = 6 ;
let DATA_DOUBLE_ARRAY = 11 ;
let DATA_STRING_ARRAY = 13 ;

Window.ttkWebSocket = {} ;

function on_message_intercept(uuid, msg) {
    let socket = Window.ttkWebSocket[uuid].socket ;
    let code = ttkWebSocketIO.parse_code(msg.data) ;
    if (code !== 0) {
        if (code === DUPLICATE) {
            alert("the connection is existing, this connection will be closed") ;
        } else if (code === LITTLE_ENDIANNESS) {
            Window.ttkWebSocket[uuid].isLittleEndianness = true;
        } else if (code === BIG_ENDIANNESS) {
            Window.ttkWebSocket[uuid].isLittleEndianness = false ;
        } else if (code === OBJECT_WILL_SENDING) {  // send object
            Window.ttkWebSocket[uuid].objectData = {} ;
            socket.send( ttkWebSocketIO.combine_code(OBJECT_ACK_OBJECT) ) ;
        } else if (code === OBJECT_WILL_FINISH) {
            socket.send( ttkWebSocketIO.combine_code(OBJECT_ACK_FINISH) ) ;
            Window.ttkWebSocket[uuid].objectCallback(Window.ttkWebSocket[uuid].objectData) ;
        }
    }

    if (typeof msg.data === "string") {
        try {
            let data = JSON.parse(msg.data) ;
            if (data.hasOwnProperty("key")) {  // header
                if ( Window.ttkWebSocket.hasOwnProperty("debugMode") &&  Window.ttkWebSocket["debugMode"] === true) {
                    console.log("[DEBUG] received header: ", data) ;
                }
                let keys = data['key'].split(":") ;
                if (keys.length === 1) {
                    Window.ttkWebSocket[uuid].objectData[keys[0]] = null ;
                } else if (keys.length === 2) {
                    if (!Window.ttkWebSocket[uuid].objectData[keys[0]]) {
                        Window.ttkWebSocket[uuid].objectData[keys[0]] = {} ;
                        Window.ttkWebSocket[uuid].objectData[keys[0]][keys[1]] = null ;
                    }
                }
                Window.ttkWebSocket[uuid].header = data ;
                socket.send( ttkWebSocketIO.combine_code(OBJECT_ACK_OBJECT)) ;
            } else {
                if (typeof Window.ttkWebSocket[uuid].header === "object" && Window.ttkWebSocket[uuid].header) {
                    let tt = parseInt(Window.ttkWebSocket[uuid].header['dataType']);
                    let keys = Window.ttkWebSocket[uuid].header['key'].split(":") ;
                    if (tt === DATA_STRING_ARRAY) {
                        let vv = {
                            "NumberOfComponents":  Window.ttkWebSocket[uuid].header['nComponents'],
                            "Values": data,
                        } ;
                        if (keys.length === 1) {
                            vv["Name"] = keys[0] ;
                            Window.ttkWebSocket[uuid].objectData[keys[0]] = vv ;
                        } else {
                            vv["Name"] = keys[1] ;
                            Window.ttkWebSocket[uuid].objectData[keys[0]][keys[1]] = vv ;
                        }
                    }
                    socket.send( ttkWebSocketIO.combine_code(OBJECT_ACK_OBJECT) ) ;
                    Window.ttkWebSocket[uuid].header = null ;
                }
            }
        } catch (e) {

        }

    } else if (msg.data instanceof Blob) {
        const buffer = msg.data.arrayBuffer();

        if (typeof Window.ttkWebSocket[uuid].header === "object" && Window.ttkWebSocket[uuid].header) {
            let keys = Window.ttkWebSocket[uuid].header['key'].split(":") ;
            let dataType =  parseInt(Window.ttkWebSocket[uuid].header['dataType']) ;
            let vv = {
                "NumberOfComponents":  Window.ttkWebSocket[uuid].header['nComponents'],
            } ;
            buffer.then(function(dt) {
                let v = null ;
                switch ( dataType) {
                    case DATA_FLOAT_ARRAY:
                        v = new Float32Array(dt, 0, msg.data.size/4);
                        break ;
                    case DATA_LONG_ARRAY:
                        v = new BigInt64Array(dt, 0, msg.data.size/8);
                        break ;
                    case DATA_UNSIGNED_ARRAY:
                        v = new Uint8Array(dt, 0, msg.data.size) ;
                        break ;
                    case DATA_INT_ARRAY:
                        v = new Int32Array(dt, 0, msg.data.size/4);
                        break ;
                    case DATA_DOUBLE_ARRAY:
                        v =  new Float64Array(dt, 0, msg.data.size/8);
                        break ;
                    default:
                        if ( Window.ttkWebSocket.hasOwnProperty("debugMode") &&  Window.ttkWebSocket["debugMode"] === true) {
                            console.log("[DEBUG] received unknown dataType data: " + keys.join(" -> ")) ;
                        }
                        break ;
                }
                if (keys.length === 1) {
                    vv["Name"] = keys[0] ;
                    vv["Values"] = v ;
                    Window.ttkWebSocket[uuid].objectData[keys[0]] = vv;
                } else {
                    vv["Name"] = keys[1] ;
                    vv["Values"] = v ;
                    Window.ttkWebSocket[uuid].objectData[keys[0]][keys[1]] = vv;
                }
            } ) ;

            socket.send( ttkWebSocketIO.combine_code(OBJECT_ACK_OBJECT) ) ;
            Window.ttkWebSocket[uuid].header = null ;
        }

        if (typeof Window.ttkWebSocket[uuid].isLittleEndianness === "undefined") {
            Window.ttkWebSocket[uuid].isLittleEndianness = true ;
        }
    }
}

class ttkWebSocketIO {

    static create_UUID(){
        let dt = new Date().getTime();
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
            let r = (dt + Math.random() * 16) % 16 | 0;
            dt = Math.floor(dt / 16);
            return (c === 'x' ? r : (r & 0x3 | 0x8)).toString(16);
        });
    }

    constructor (port, on_open, on_error, on_message, on_close, objectCallback, ip="localhost", debugMode=false) {
        let uuid = ttkWebSocketIO.create_UUID();
        this.uuid = uuid ;
        let socket = new WebSocket('ws://'+ip+':' + port);
        socket.onopen = function() {  // add the interceptor
            return on_open() ;
        };
        socket.onerror = function() { // add the interceptor
            return on_error() ;
        };
        socket.onclose = function() { // add the interceptor
            return on_close() ;
        };

        socket.onmessage = function(msg) {
            on_message_intercept(uuid, msg) ;
            return on_message(msg) ;
        };

        if ( debugMode === true ) {
            Window.ttkWebSocket["debugMode"] = true ;
        }
        Window.ttkWebSocket[uuid] = {};
        Window.ttkWebSocket[uuid].socket = socket ;
        Window.ttkWebSocket[uuid].uuid = uuid ;
        Window.ttkWebSocket[uuid].objectCallback = objectCallback ;
    }

    static parse_code(data) {
        if (typeof data === "string" && data.startsWith("code:")) {
            return parseInt(data.substring(5));
        }
        return 0 ;
    }

    static combine_code(code) {
        return "code:" + code ;
    }

    getWindowObject () {
        return Window.ttkWebSocket[this.uuid] ;
    }

    close() {
        let o = this.getWindowObject() ;
        if (o.socket)
            o.socket.close () ;
    }

    send(data) {  // send data back to Paraview
        let o = this.getWindowObject() ;
        o.socket.send( data ) ;
    }

    sendUnstructuredGrid(pointCoords, connectivityList, pointData={}, cellData={}, fieldData={}) {  // send unstructuredGrid
        let o = this.getWindowObject() ;
        let prefix = "updateUnstructuredGrid:" ;

        let items = {
            "PointCoords": pointCoords,
            "ConnectivityList": connectivityList,
            "CellData": cellData,
            "FieldData": fieldData,
            "PointData": pointData,
        } ;
        o.socket.send( prefix + JSON.stringify(items)) ;
    }

    sendUnstructuredGridJSON(json_text) {  // send unstructuredGrid by json
        let o = this.getWindowObject() ;
        let prefix = "updateUnstructuredGrid:" ;

        try {
            JSON.parse(json_text) ;
            o.socket.send( prefix + json_text) ;
        } catch ( e ) {
            alert ("make sure the input is the json format") ;
        }
    }

    getSocketObject() {  // get raw socket connection to do cool things on your own
        let o = this.getWindowObject() ;
        return o.socket ;
    }
}