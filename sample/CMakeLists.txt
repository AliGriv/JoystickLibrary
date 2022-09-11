# CMakeLists.txt for jstester sample application

add_executable (jstester jstester.cpp ServiceTester.h)
target_include_directories (jstester PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries (jstester LINK_PUBLIC JoystickLibrary)