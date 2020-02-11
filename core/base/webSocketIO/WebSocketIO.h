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
#include <map>

#include <iostream>

#include <functional>

#if TTK_ENABLE_WEBSOCKETIO

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::thread;

#endif

namespace ttk {

    class WebSocketIO : virtual public Debug {
    public:

        static std::string combine_code(int code) {
            return "code:" + std::to_string(code) ;
        }

        static int parse_code(const std::string& code_str) {  // the code starting from 1
            if (code_str.rfind("code:", 0) == 0) {
                return stoi(code_str.substr(5));
            }
            return 0 ;
        }

        // JSON
        static std::string combine_object_header_json(std::map<std::string, std::string>& m) {
            return "{" \
                 "\"key\": \"" + m["key"] + "\"" + ", "
                   + "\"nTuples\":" + m["nTuples"] + ", "
                   + "\"nComponents\":" + m["nComponents"] + ", "
                   + "\"dataType\":" + m["dataType"]
                   + "}" ;
        }

        static std::map<std::string, std::string> combine_object_header_object(const std::string &key, int nTuples, int nComponents, int dataType) {
            std::map<std::string, std::string> ans;

            ans.insert(std::make_pair("key", key));  // "pointCoords" or "FieldData:Result", split by ":"
            ans.insert(std::make_pair("nTuples", std::to_string(nTuples)));
            ans.insert(std::make_pair("nComponents", std::to_string(nComponents)));
            ans.insert(std::make_pair("dataType", std::to_string(dataType)));

            return ans;
        }

        WebSocketIO();
        ~WebSocketIO();

        int isListening();

        int virtual processClientRequest(std::string name, std::string payload = ""){return 0;};

        int startServer(int PortNumber);
        int stopServer();

        int getPortNumber();

        int setHeaders(const std::vector<std::map<std::string, std::string>>& m);
        int setObjectState(int o);
        int setSendingData(const std::vector<void *>& v);
        int sendObject();

    private:

        #if TTK_ENABLE_WEBSOCKETIO
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

            std::vector<std::map<std::string, std::string>> headers;
            std::vector<void *> sendingData ;
            int portNumber = 0;

            int on_open(websocketpp::connection_hdl hdl);
            int on_received_object();
            int on_close(websocketpp::connection_hdl hdl);
            int on_message(websocketpp::connection_hdl hdl, server::message_ptr msg);
        #endif

    };
}
