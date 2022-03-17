cmake_minimum_required(VERSION 3.5)

project(vistd LANGUAGES CXX)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(Boost_USE_STATIC_LIBS  ON)

include_directories(../p2db)
link_directories(../p2db/build)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED log log_setup filesystem thread program_options)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

#set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_BUILD_RPATH TRUE)

if (UNIX AND NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wall -Wextra")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -march=nocona -fstack-protector-strong --param ssp-buffer-size=4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -DNDEBUG -march=nocona -fstack-protector-strong --param ssp-buffer-size=4")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")
endif()

add_executable(vistd
  main.cpp
  config.cpp
  technic_info.cpp
)

target_link_libraries(vistd
  ssl
  z
  crypto
  curl
  expat
  parser2db
  Threads::Threads
  ${Boost_LOG_LIBRARY}
  ${Boost_LOG_SETUP_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY})

target_compile_definitions(vistd PUBLIC WITH_OPENSSL)
