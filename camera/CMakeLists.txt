add_library(bnb_oep_camera_target STATIC)

target_include_directories(bnb_oep_camera_target PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(bnb_oep_camera_target PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../oep)
if (WIN32)
    file(GLOB_RECURSE win_srcs
        ${CMAKE_CURRENT_LIST_DIR}/*.hpp
        ${CMAKE_CURRENT_LIST_DIR}/windows/*.hpp
        ${CMAKE_CURRENT_LIST_DIR}/windows/*.cpp
    )
    target_sources(bnb_oep_camera_target PUBLIC ${win_srcs})
else ()
    message(FATAL_ERROR "Unsupported platform for 'bnb_oep_camera_target', CMake will exit.")
endif ()
