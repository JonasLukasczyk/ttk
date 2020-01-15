ttk_add_base_library(webSocketIO
  SOURCES
    WebSocketIOUtils.cpp
  HEADERS
    WebSocketIO.h
  LINK
    triangulation
    boost_system
    #boost_filesystem
)

target_include_directories(webSocketIO PUBLIC ${WEBSOCKETPP_INCLUDE_DIR})