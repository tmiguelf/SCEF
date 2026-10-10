cmake_minimum_required(VERSION 4.2.3)
project(scef)
enable_language(CXX)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS 
	"./src/*.cpp"
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

target_link_libraries(${PROJECT_NAME} PUBLIC CoreLib)

quick_default_post_config()

