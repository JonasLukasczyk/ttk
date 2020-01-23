ttk_add_base_library(webSocketIO
  HEADERS
    WebSocketIO.h
  LINK
    triangulation
    boost_system
)

target_include_directories(webSocketIO PUBLIC ${WEBSOCKETPP_INCLUDE_DIR})