set(HOTARU_PUBLIC_HEADERS_DIR ${CMAKE_SOURCE_DIR}/include)

set(HOTARU_PRIVATE_HEADERS 
)

set(HOTARU_SOURCE_FILES 
    ${CMAKE_CURRENT_LIST_DIR}/engine.cpp
)

add_library(${PROJECT_NAME} 
    ${HOTARU_PRIVATE_HEADERS} 
    ${HOTARU_SOURCE_FILES}
)

find_package(spdlog REQUIRED)

target_include_directories(${PROJECT_NAME} 
    PUBLIC ${HOTARU_PUBLIC_HEADERS_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
)

