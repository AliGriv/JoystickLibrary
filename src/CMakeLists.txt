
set(CMAKE_CXX_STANDARD 11)
find_package(Threads REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
project(joystick-lib)


file(GLOB JOYSTICK_LIBRARY_LINUX_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/*.h*
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/linux/*.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-missing-field-initializers -pedantic")
find_package(PkgConfig)
pkg_search_module(LIBEVDEV REQUIRED libevdev)
pkg_search_module(LIBUDEV REQUIRED libudev)
add_library(JoystickLibrary STATIC ${JOYSTICK_LIBRARY_LINUX_SRC})
target_link_libraries(JoystickLibrary ${LIBEVDEV_LIBRARIES} ${LIBUDEV_LIBRARIES})
target_include_directories(JoystickLibrary PUBLIC ${LIBEVDEV_INCLUDE_DIRS})
target_compile_options(JoystickLibrary PUBLIC ${LIBEVDEV_CFLAGS_OTHER})

target_link_libraries (JoystickLibrary ${CMAKE_THREAD_LIBS_INIT})
