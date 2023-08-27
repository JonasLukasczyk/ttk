ttk_add_base_library(cinemaWriter
  SOURCES
    PyCinemaWriter.cxx
  HEADERS
    PyCinemaWriter.h
  DEPENDS
    triangulation
)

if(TTK_ENABLE_HDF5)
  target_compile_definitions(cinemaWriter PRIVATE TTK_ENABLE_HDF5)
  target_link_libraries(cinemaWriter PRIVATE ${HDF5_LIBRARIES} ${HDF5_CXX_LIBRARIES})
endif()
