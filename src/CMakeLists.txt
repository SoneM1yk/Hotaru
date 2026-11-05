set(HOTARU_PUBLIC_HEADERS_DIR ${CMAKE_SOURCE_DIR}/include)
set(HOTARU_PRIVATE_HEADERS_DIR ${CMAKE_SOURCE_DIR}/src)

file(GLOB_RECURSE HOTARU_PRIVATE_HEADERS 
    ${CMAKE_CURRENT_LIST_DIR}/*.h
)

file(GLOB_RECURSE HOTARU_SOURCE_FILES 
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

add_library(${PROJECT_NAME} 
    ${HOTARU_PRIVATE_HEADERS} 
    ${HOTARU_SOURCE_FILES}
)

find_package(spdlog REQUIRED)
find_package(Vulkan REQUIRED)
find_package(volk REQUIRED) 
find_package(EnTT REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${HOTARU_PUBLIC_HEADERS_DIR}
    PRIVATE ${HOTARU_PRIVATE_HEADERS_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
    Vulkan::Vulkan
    volk::volk
    EnTT::EnTT
    Eigen3::Eigen
)

if(TARGET hotaru_shaders)
    add_dependencies(${PROJECT_NAME} hotaru_shaders)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        HOTARU_DEFAULT_VERTEX_SHADER="${HOTARU_VERTEX_SHADER}"
        HOTARU_DEFAULT_FRAGMENT_SHADER="${HOTARU_FRAGMENT_SHADER}"
    )
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        HOTARU_DEFAULT_VERTEX_SHADER=""
        HOTARU_DEFAULT_FRAGMENT_SHADER=""
    )
endif()

# Compile Definitions
if(ENABLE_PROFILE) 
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE ENABLE_PROFILE)
endif()
