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

#include <iostream>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::thread;

using namespace std ;

// https://vtk.org/doc/nightly/html/vtkType_8h_source.html
// ============================================= Constants ==========================================================
const int DUPLICATE = 1 ;
const int BIG_ENDIANNESS = 2 ;
const int LITTLE_ENDIANNESS = 3 ;
const int OBJECT_WILL_SENDING = 5 ;
const int OBJECT_ACK_OBJECT = 6 ;
const int OBJECT_WILL_FINISH = 7 ;
const int OBJECT_ACK_FINISH = 8 ;

const int DATA_FLOAT_ARRAY = 10 ;
const int DATA_LONG_ARRAY = 8 ;
const int DATA_UNSIGNED_ARRAY = 3 ;
const int DATA_INT_ARRAY = 6 ;
const int DATA_DOUBLE_ARRAY = 11 ;
const int DATA_STRING_ARRAY = 13 ;

// ============================================ Utils ==============================================================

// borrow from https://stackoverflow.com/questions/4239993/determining-endianness-at-compile-time/4240029
bool isLittleEndian()
{
    short int number = 0x1;
    char *numPtr = (char*)&number;
    return (numPtr[0] == 1);
}

namespace ttk {

    class WebSocketIO : virtual public Debug {
    public:

        static string combine_code(int code) {
            return "code:" + to_string(code) ;
        }

        static int parse_code(const string& code_str) {  // the code starting from 1
            if (code_str.rfind("code:", 0) == 0) {
                return stoi(code_str.substr(5));
            }
            return 0 ;
        }

        // JSON
        static  string combine_object_header_json(map<string, string> m) {
            return "{" \
                 "\"key\": \"" + m["key"] + "\"" + ", "
                   + "\"nTuples\":" + m["nTuples"] + ", "
                   + "\"nComponents\":" + m["nComponents"] + ", "
                   + "\"dataType\":" + m["dataType"]
                   + "}" ;
        }

        static std::map<string, string> combine_object_header_object(const string &key, int nTuples, int nComponents, int dataType) {
            map<string, string> ans;

            ans.insert(std::make_pair("key", key));  // "pointCoords" or "FieldData:Result", split by ":"
            ans.insert(std::make_pair("nTuples", to_string(nTuples)));
            ans.insert(std::make_pair("nComponents", to_string(nComponents)));
            ans.insert(std::make_pair("dataType", to_string(dataType)));

            return ans;
        }

        int isListening() { return this->Server.is_listening() ; }

        void virtual processClientRequest(std::string name, std::string payload = ""){};

        WebSocketIO() {
            this->setDebugMsgPrefix("WebSocketIO"); // inherited from Debug: prefix will be printed at the beginning of every msg
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

            this->printMsg("###### 6: invoke startServer at port: " + to_string(this->portNumber)) ;
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
                    cout << "startServer exception: " << e.what() << endl;
                }
            });
            this->ServerThread->detach();
        }

        int getPortNumber(){ return this->portNumber; }

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

        int sendObject() {
            if (m_connections.empty()) {
                return 0 ;
            }

            auto it = this->m_connections.begin();
            if (this->object_state == 0) {
                this->Server.send(*it, WebSocketIO::combine_code(OBJECT_WILL_SENDING), websocketpp::frame::opcode::text) ;
                this->object_state += 1 ;
            } else {
                // cause object_state is non-negative, so it's safe to cast to unsigned value
                if ((unsigned )this->object_state > 2 * this->headers.size()) {
                    this->Server.send(*it, WebSocketIO::combine_code(OBJECT_WILL_FINISH), websocketpp::frame::opcode::text) ;
                    return 2 ; // finish
                } else {
                    if (this->object_state % 2 == 1) {  // sending header
                        this->Server.send(*it, WebSocketIO::combine_object_header_json(this->headers[(this->object_state - 1) / 2] ), websocketpp::frame::opcode::text) ;
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
                m_connections.insert(hdl);
                if (isLittleEndian()) {
                    this->Server.send(hdl, WebSocketIO::combine_code(LITTLE_ENDIANNESS), websocketpp::frame::opcode::text);
                    this->Server.send(hdl, WebSocketIO::combine_code(LITTLE_ENDIANNESS), websocketpp::frame::opcode::text);
                } else {
                    this->Server.send(hdl, WebSocketIO::combine_code(BIG_ENDIANNESS), websocketpp::frame::opcode::text);
                }
            } else {
                // close this connection
                cout <<"duplicate connection" << endl;
                this->Server.send(hdl, WebSocketIO::combine_code(DUPLICATE), websocketpp::frame::opcode::text);
                this->Server.close(hdl,  websocketpp::close::status::normal, "Terminating connection ...", ec);
            }

            this->processClientRequest("on_open");
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
            int code = WebSocketIO::parse_code(pay_msg) ;
            if ( code == 0 ) {
                this->processClientRequest("raw", pay_msg) ;
            } else {  // well defined code
                if (code == OBJECT_ACK_OBJECT) {
                    sendObject();
                } else if (code == OBJECT_ACK_FINISH) {
                    on_received_object();
                }
            }
        }
    };
}
