cmake_minimum_required( VERSION 3.8 )
project ("MCGILL")

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/libs)
set(ASSET_DIR ${CMAKE_SOURCE_DIR}/assets)
set(SHDR_DIR ${SRC_DIR}/shaders)
set(AUDIO_DIR ${SRC_DIR}/audio)

file(GLOB SOURCES 
	"${SRC_DIR}/*.cpp" 
	"${SRC_DIR}/*.h" 
	"${LIB_DIR}/ImGui/*.cpp"
	"${LIB_DIR}/ImGui/*.h"
	"${LIB_DIR}/ImGui/backends/*.h"
	"${LIB_DIR}/ImGui/backends/*.cpp"
	"${LIB_DIR}/STB_IMAGE/stb_image.h"
	"${LIB_DIR}/OpenGL/src/glad.c"
)

message(STATUS "Found source files: ${SOURCES}")

add_library(GLFW STATIC IMPORTED)
set_target_properties(GLFW PROPERTIES IMPORTED_LOCATION  "${LIB_DIR}/glfw-3.4/lib-vc-2022/glfw3.lib" )
target_include_directories(GLFW INTERFACE "${LIB_DIR}/glfw-3.4/include")

add_library(GLAD "${LIB_DIR}/OpenGL/src/glad.c")
target_include_directories(GLAD PUBLIC "${LIB_DIR}/OpenGL/include")

add_executable (MCGILL ${SOURCES})

target_include_directories(MCGILL PRIVATE
	"${SRC_DIR}/include"
    "${LIB_DIR}/ImGui"         
    "${LIB_DIR}/ImGui/backends"
	"${LIB_DIR}/STB_IMAGE"
)

target_link_libraries(MCGILL PUBLIC GLAD)
target_link_libraries(MCGILL PRIVATE GLFW)

# Windows-specific: Link winmm for audio
if(WIN32)
    target_link_libraries(MCGILL PRIVATE winmm)
endif()

# Copy required files to build directory
add_custom_command(
    TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
         "${LIB_DIR}/glfw-3.4/lib-vc-2022/glfw3.dll" ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHDR_DIR} "${CMAKE_BINARY_DIR}/shaders"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${ASSET_DIR} "${CMAKE_BINARY_DIR}/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${AUDIO_DIR} "${CMAKE_BINARY_DIR}/audio"
    COMMENT "Copying runtime files (shaders, assets, audio) to build directory"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MCGILL PROPERTY CXX_STANDARD 20)
endif()