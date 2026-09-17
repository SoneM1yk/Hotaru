set(HOTARU_PUBLIC_HEADERS_DIR ${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE HOTARU_PRIVATE_HEADERS "${CMAKE_CURRENT_LIST_DIR}/*.h")
file(GLOB_RECURSE HOTARU_SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(${PROJECT_NAME} 
    ${HOTARU_PRIVATE_HEADERS} 
    ${HOTARU_SOURCE_FILES}
)

find_package(spdlog REQUIRED)
find_package(Vulkan REQUIRED)
find_package(EnTT REQUIRED)

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${HOTARU_PUBLIC_HEADERS_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
    Vulkan::Vulkan
    EnTT::EnTT
)

