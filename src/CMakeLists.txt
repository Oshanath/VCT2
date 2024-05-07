cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
    message("Using 64-bit glslangValidator")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message("Using 32-bit glslangValidator")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

set(VULKAN_TEMPLATE_SOURCES
	${PROJECT_SOURCE_DIR}/src/Application.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/TriangleRenderer.cpp
    ${PROJECT_SOURCE_DIR}/src/Mesh.cpp
    ${PROJECT_SOURCE_DIR}/src/Camera.cpp
    ${PROJECT_SOURCE_DIR}/src/Helper.cpp
    ${PROJECT_SOURCE_DIR}/src/ShadowMap.cpp
    ${PROJECT_SOURCE_DIR}/src/Voxelizer.cpp
    )

set(SHADER_SOURCES 
	${PROJECT_SOURCE_DIR}/src/shaders/main/main.vert
    ${PROJECT_SOURCE_DIR}/src/shaders/main/main.frag
    ${PROJECT_SOURCE_DIR}/src/shaders/shadowmap/shadowmap.vert
    ${PROJECT_SOURCE_DIR}/src/shaders/shadowmap/shadowmap.frag
    )

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(VCT2_0 ${VULKAN_TEMPLATE_SOURCES} ${SHADER_SOURCES} "main.cpp" "ShadowMap.cpp")

foreach(GLSL ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_SOURCE_DIR}/bin/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/shaders"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 -V ${GLSL} -o ${SPIRV} -gVS
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(VulkanTemplate_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(VCT2_0 VulkanTemplate_Shaders)

set_property(TARGET VCT2_0 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

target_link_libraries(VCT2_0 glfw)
target_link_libraries(VCT2_0 assimp)
target_link_libraries(VCT2_0 ${Vulkan_LIBRARY})