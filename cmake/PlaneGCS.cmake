# Unmodified pinned solver files copied into a standalone layout. No FreeCAD UI.
set(GCS_ROOT "${CMAKE_BINARY_DIR}/planegcs")
file(MAKE_DIRECTORY "${GCS_ROOT}/src/planegcs" "${GCS_ROOT}/Base")
file(GLOB GCS_FILES "${DEPS}/planegcs/*")
foreach(file IN LISTS GCS_FILES)
  configure_file("${file}" "${GCS_ROOT}/src/planegcs/" COPYONLY)
endforeach()
file(WRITE "${GCS_ROOT}/SketcherGlobal.h" "#pragma once\n#define SketcherExport\n")
file(WRITE "${GCS_ROOT}/FCConfig.h" "#pragma once\n")
file(WRITE "${GCS_ROOT}/boost_graph_adjacency_list.hpp" "#pragma once\n#include <boost/graph/adjacency_list.hpp>\n")
configure_file("${CMAKE_SOURCE_DIR}/tests/support/Console.h" "${GCS_ROOT}/Base/Console.h" COPYONLY)
add_library(cad_planegcs STATIC
  "${GCS_ROOT}/src/planegcs/GCS.cpp" "${GCS_ROOT}/src/planegcs/Constraints.cpp"
  "${GCS_ROOT}/src/planegcs/Geo.cpp" "${GCS_ROOT}/src/planegcs/SubSystem.cpp"
  "${GCS_ROOT}/src/planegcs/qp_eq.cpp")
target_include_directories(cad_planegcs SYSTEM PUBLIC "${GCS_ROOT}/src/planegcs" "${DEPS}/boost" PRIVATE "${GCS_ROOT}")
target_link_libraries(cad_planegcs PUBLIC cad_eigen)
if(NOT MSVC)
  target_compile_options(cad_planegcs PRIVATE -O1)
endif()
