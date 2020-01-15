/// \ingroup base
/// \class ttk::WebSocketIO
/// \author Jonas Lukasczyk <jl@jluk.de>
/// \date 01.09.2019
///
/// TODO

#pragma once

// ttk common includes
#include <Debug.h>
#include <set>
// #include <chrono>
// #define ASIO_STANDALONE

#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <WebSocketIOUtils.cpp>

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::thread;

using namespace std ;

class ServerParser {
public:
    static int parse_message_type(const string& msg) {
        if (msg.rfind("code:", 0) == 0) {
            return 1 ;  // code type
        }

        if (msg == "requestData") {
            return 2 ; // request data from client
        }

        if (msg.rfind("updateUnstructuredGrid:", 0) == 0) {
            return 3; // update data from client,
        }

        if (msg.rfind("updateImageData:", 0) == 0) {
            return 4 ; // update data from client
        }

        return 0 ;  // un-known
    }

    static string combine_code(int code) {
        return "code:" + to_string(code) ;
    }

    static int parse_code(const string& code_str) {
        if (code_str.rfind("code:", 0) == 0) {
            return stoi(code_str.substr(5));
        }
        return 0 ;
    }

    static string parse_updateUnstructuredGrid(const string& msg_str) {
        if (msg_str.rfind("updateUnstructuredGrid:", 0) == 0) {
            return msg_str.substr(23);
        }
        return "" ;
    }

    static string parse_updateImageData(const string& msg_str) {
        if (msg_str.rfind("updateImageData:", 0) == 0) {
            return msg_str.substr(16);
        }
        return "" ;
    }

    // JSON
    static  string combine_object_header(map<string, string> m) {
        return "{" \
                 "\"key\": \"" + m["key"] + "\"" + ", "
               + "\"nTuples\":" + m["nTuples"] + ", "
               + "\"nComponents\":" + m["nComponents"] + ", "
               + "\"dataType\":" + m["dataType"]
               + "}" ;
    }

};

namespace ttk {

    struct WebSocketObserver {
        virtual void update(std::string name, std::string payload)=0;
    };

    class WebSocketIO : virtual public Debug {

        public:

            vector<WebSocketObserver*> observers;

            void addObserver(WebSocketObserver* observer){
                observers.push_back(observer);
            }

            int isListening() {
                return this->Server.is_listening() ;
            }

            void removeObserver(WebSocketObserver* observer){
                // find and remove from vector
                observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end()) ;
            }

            void notifyObservers(std::string name, std::string payload=""){
                //for(auto observer: this->observers)
                //    observer->update(name, payload);
                this->processClientRequest(name, payload) ;
            }

            void virtual processClientRequest(std::string name, std::string payload){};

            WebSocketIO() {
                this->setDebugMsgPrefix("WebSocketIO"); // inherited from Debug: prefix will be printed at the beginning of every msg
                this->printMsg("invoke WebSocketIO") ;

                // Set logging settings
                Server.set_error_channels(websocketpp::log::elevel::fatal);
                Server.set_access_channels(websocketpp::log::alevel::fail) ;

                Server.set_reuse_addr(true) ;

                // Initialize Asio
                Server.init_asio();

                // Set the default message handler to the echo handler
                Server.set_message_handler(bind(&WebSocketIO::on_message, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
                Server.set_open_handler(bind(&WebSocketIO::on_open, this, websocketpp::lib::placeholders::_1));
                Server.set_close_handler(bind(&WebSocketIO::on_close, this, websocketpp::lib::placeholders::_1));
            };

            void startServer(int PortNumber) {
                this->portNumber = PortNumber;

                this->printMsg("invoke startServer at port: " + to_string(this->portNumber)) ;
                this->Server.reset();
                this->Server.listen(this->portNumber);

                // Queues a connection accept operation
                this->Server.start_accept();

                // Start the Asio io_service run loop
                this->ServerThread = new thread([this]() {
                    try {
                        {
                            lock_guard<mutex> guard(this->m_mutex);
                            this->ServerThreadRunning = true;
                        }
                        Server.run();
                        lock_guard<mutex> guard(this->m_mutex);
                        this->ServerThreadRunning = false;
                    } catch(websocketpp::exception const &e) {
                        cout << "#########" << e.what() << endl;
                    }
                });
                this->ServerThread->detach();
            }

            int getPortNumber(){
                return this->portNumber;
            }

        int stopServer(){
            if(this->Server.is_listening()){
                cout<<"Stopping Server ";

                // Stopping the Websocket listener and closing outstanding connections.
                this->Server.stop_listening(this->ec);
                if (this->ec) {
                    cout << this->ec.message() << endl;
                    return 0;
                }
                // Close all existing websocket connections.
                {
                    // initiate closing
                    {
                        lock_guard<mutex> guard(this->m_mutex);

                        cout<<"closing connections"<<endl;
                        for(con_list::iterator it = this->m_connections.begin(); it != this->m_connections.end(); ++it) {
                            cout<<"c "<<endl;
                            this->Server.close(
                                    *it,
                                    websocketpp::close::status::normal,
                                    "Terminating connection ...",
                                    this->ec
                            );
                            if (this->ec) {
                                cout << this->ec.message() << endl;
                                return 0;
                            }
                        }

                        this->m_connections.clear();
                    }

                    // wait until all closed
                    size_t t = 1;
                    while(t>0){
                        lock_guard<mutex> guard(this->m_mutex);
                        t = this->m_connections.size();
                    }
                    cout<<"done waiting"<<endl;
                }
                // Stop the endpoint.
                {
                    this->Server.stop();

                    // wait until thread terminated
                    bool con = true;
                    while(con){
                        lock_guard<mutex> guard(this->m_mutex);
                        con = this->ServerThreadRunning;
                    }
                    delete this->ServerThread;
                    this->ServerThread = nullptr;
                    cout<<"done waiting for thread"<<endl;
                }

                cout<<"done stopping"<<endl;
            }
            return 1;
        }

            ~WebSocketIO() {
                this->stopServer() ;
            };

            void setHeaders(std::vector<std::map<string, string>> m) {
                this->headers = m ;
            }

            void setObjectState(int o) {
                this->object_state = o ;
            }

            void setSendingData(std::vector<void *> v) {
                this->sendingData = v ;
            }

        int send(const string& message) {
            if (m_connections.empty()) {
                return 0 ;
            }
            cout << isLittleEndian() ;
            auto it = this->m_connections.begin();

            this->Server.send(*it, message, websocketpp::frame::opcode::text );
            return 1 ;
        }

        int sendObject() {
            if (m_connections.empty()) {
                return 0 ;
            }

            auto it = this->m_connections.begin();
            if (this->object_state == 0) {
                this->Server.send(*it, ServerParser::combine_code(OBJECT_WILL_SENDING), websocketpp::frame::opcode::text) ;
                this->object_state += 1 ;
            } else {
                // cause object_state is non-negative, so it's safe to cast to unsigned value
                if ((unsigned )this->object_state > 2 * this->headers.size()) {
                    this->Server.send(*it, ServerParser::combine_code(OBJECT_WILL_FINISH), websocketpp::frame::opcode::text) ;
                    return 2 ; // finish
                } else {
                    if (this->object_state % 2 == 1) {  // sending header
                        this->Server.send(*it, ServerParser::combine_object_header(this->headers[(this->object_state - 1) / 2] ), websocketpp::frame::opcode::text) ;
                    } else {  // sending data
                        auto d = this->sendingData[(this->object_state / 2) - 1];
                        map<string, string> m = this->headers[(this->object_state / 2) - 1];

                        // https://vtk.org/doc/nightly/html/vtkType_8h_source.html
                        switch (stoi(m["dataType"])) {
                            case DATA_FLOAT_ARRAY:
                                this->Server.send(*it, d, stoi(m["nTuples"]) * stoi(m["nComponents"]) * sizeof(float), websocketpp::frame::opcode::binary);
                                break;
                            case DATA_LONG_ARRAY:
                                this->Server.send(*it, d, stoi(m["nTuples"]) * stoi(m["nComponents"]) * sizeof(long long), websocketpp::frame::opcode::binary);
                                break;
                            case DATA_UNSIGNED_ARRAY:
                                this->Server.send(*it, d, stoi(m["nTuples"]) * stoi(m["nComponents"]) * sizeof(unsigned char), websocketpp::frame::opcode::binary);
                                break;
                            case DATA_INT_ARRAY: {
                                this->Server.send(*it, d, stoi(m["nTuples"]) * stoi(m["nComponents"]) * sizeof(signed int), websocketpp::frame::opcode::binary);
                                break; }
                            case DATA_DOUBLE_ARRAY:
                                this->Server.send(*it, d, stoi(m["nTuples"]) * stoi(m["nComponents"]) * sizeof(double), websocketpp::frame::opcode::binary);
                                break;
                            case DATA_STRING_ARRAY:
                                size_t n = stoi(m["nTuples"]) * stoi(m["nComponents"]);
                                string json = "" ;
                                string * values = (string *) d ;
                                for (size_t i = 0; i < n ; i++) {
                                    if (json == "") {
                                        json += "[" ;
                                    }
                                    if (i > 0) {
                                        json += "," ;
                                    }
                                    string temp = values[i] ;
                                    temp.erase(std::remove(temp.begin(), temp.end(), '"'), temp.end());
                                    temp.erase(std::remove(temp.begin(), temp.end(), ','), temp.end());
                                    json += "\"" + temp + "\"" ;
                                }
                                json += "]" ;
                                this->Server.send(*it, json, websocketpp::frame::opcode::text);
                                break;
                        }
                    }
                }
                this->object_state += 1 ;
            }

            return 1 ;
        }

    private:
        typedef std::set<connection_hdl,std::owner_less<connection_hdl>> con_list;
        typedef websocketpp::server<websocketpp::config::asio> WSServer;
        WSServer Server;
        std::thread* ServerThread = nullptr;
        con_list m_connections;
        std::mutex m_mutex;
        websocketpp::lib::error_code ec;
        // keep the state of the object sending process
        int object_state = 0;
        bool ServerThreadRunning = false;

        std::vector<std::map<std::string, std::string>> headers ;
        std::vector<void *> sendingData ;
        int portNumber = 0;

        void on_open(websocketpp::connection_hdl hdl) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_connections.empty()) {
                m_connections.insert(hdl);  // 78: 0x7f822402ef00, 77: 0x7f822c02f310
                if (isLittleEndian()) {
                    this->Server.send(hdl, ServerParser::combine_code(LITTLE_ENDIANNESS), websocketpp::frame::opcode::text);
                } else {
                    this->Server.send(hdl, ServerParser::combine_code(BIG_ENDIANNESS), websocketpp::frame::opcode::text);
                }
            } else {
                // close this connection
                cout <<"duplicate connection" << endl;
                this->Server.send(hdl, ServerParser::combine_code(DUPLICATE), websocketpp::frame::opcode::text);
                this->Server.close(hdl,  websocketpp::close::status::normal, "Terminating connection ...", ec);
            }

            this->notifyObservers("on_open");
        }

        void on_received_object() {
            std::lock_guard<std::mutex> lock(m_mutex);
            cout << "invoke on_received_object" << endl ;
        }

        void on_close(websocketpp::connection_hdl hdl) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connections.erase(hdl);
        }

        void on_message(websocketpp::connection_hdl hdl, server::message_ptr msg) {
            // write a new message
            string pay_msg = msg->get_payload() ;
            this->printMsg("receive a message: " + pay_msg) ;
            switch (ServerParser::parse_message_type(pay_msg)) {
                case 1: { // code
                    int code = ServerParser::parse_code(pay_msg);
                    if (code != 0) {
                        if (code == OBJECT_ACK_OBJECT) {
                            sendObject();
                        } else if (code == OBJECT_ACK_FINISH) {
                            on_received_object();
                        }
                    }}
                    break ;

                case 2: // request data from client
                    this->printMsg("receive requestData") ;
                    this->notifyObservers("on_requestData");
                    break ;
                case 3: // update
                    this->notifyObservers("updateUnstructuredGrid", ServerParser::parse_updateUnstructuredGrid(pay_msg));
                    break ;
                case 4:
                    this->notifyObservers("updateImageData", ServerParser::parse_updateImageData(pay_msg));
                    break ;

                default:
                    break ;
            }

        }
    };
}
