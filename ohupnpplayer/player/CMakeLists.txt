cmake_minimum_required(VERSION 3.15...4.1)

option(DUMMY_RENDERER "Enable dummy renderer support" ON)
option(MPD_RENDERER "Enable MPD renderer" OFF)

project(
        ohupnpplayer
        VERSION 1.0
        LANGUAGES CXX)


add_subdirectory(platinum)

set(PLAYER_SOURCES
        OHUPnPPlayer.cpp
        control/MyOHPlaylist.cpp
        control/MyUPnPRenderer.cpp


)

set(SOURCES
        ${PLAYER_SOURCES}
)

include_directories(common control renderer)




# Adding something we can run - Output name matches target name
add_executable(${PROJECT_NAME} ${SOURCES})


target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${MPD_RENDERER}>:MPD_RENDERER=1>
)
target_sources(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${MPD_RENDERER}>:${CMAKE_CURRENT_SOURCE_DIR}/renderer/MPDRenderer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/renderer/MpdController.cpp>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${DUMMY_RENDERER}>:DUMMY_RENDERER=1>
)
target_sources(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${DUMMY_RENDERER}>:${CMAKE_CURRENT_SOURCE_DIR}/renderer/DummyRenderer.cpp>
)


# Make sure you link your targets with this command. It can also link libraries and
# even flags, so linking a target that does not exist will not give a configure-time error.
target_link_libraries(${PROJECT_NAME} PRIVATE platinum-upnp)

find_package(PkgConfig REQUIRED)

if(MPD_RENDERER)
  set_target_properties(${PROJECT_NAME} PROPERTIES
          CXX_STANDARD 17
          CXX_EXTENSIONS OFF
  )

  pkg_check_modules(MPDClient REQUIRED IMPORTED_TARGET libmpdclient)
  target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::MPDClient)
endif()
