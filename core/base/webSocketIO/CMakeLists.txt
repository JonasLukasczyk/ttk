ttk_add_base_library(webSocketIO
  HEADERS
    WebSocketIO.h
    WebSocketIO.cpp
  LINK
    triangulation
    boost_system
)