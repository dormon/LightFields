cmake_minimum_required(VERSION 3.12.1)

project(LightFieldsPlayer)

SET(CMAKE_CXX_STANDARD 14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(MAJOR_VERSION 1)
set(MINOR_VERSION 0)
set(REVISION_VERSION 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(ArgumentViewer 1.0 CONFIG REQUIRED)
find_package(BasicCamera 1.0 CONFIG REQUIRED)
find_package(Simple3DApp 1.0.0 CONFIG REQUIRED)
find_package(Vars CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
#find_package(Compressonator CONFIG REQUIRED)

find_package(FreeImage REQUIRED)
find_package(FreeImagePlus REQUIRED)

set(SOURCES_RENDER
  src/render/main.cpp
  src/render/Barrier.cpp
  src/render/imguiVars.cpp
  src/render/DrawGrid.cpp
  )

set(INCLUDES_RENDER
  src/render/Barrier.h
  src/render/imguiVars.h
  src/render/DrawGrid.h
  )

set(SOURCES_CONVERT
  src/convert/main.cpp
  src/convert/DDS_Helpers.cpp
  )

set(INCLUDES_CONVERT
    src/convert/Compressonator.h
    src/convert/DDS_Helpers.h
  )

set(RENDER renderLf)
set(CONVERT convertLf)

add_executable(${RENDER} ${SOURCES_RENDER} ${INCLUDES_RENDER})
add_executable(${CONVERT} ${SOURCES_CONVERT} ${INCLUDES_CONVERT})

add_custom_target(
    run
    COMMAND ${RENDER}
  )

target_include_directories(${RENDER} PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories(${RENDER} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/render>)

target_include_directories(${CONVERT} PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories(${CONVERT} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/convert>)
target_include_directories(${CONVERT} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Compressonator/Compressonator/Header/>)

target_link_libraries(${RENDER} PUBLIC Simple3DApp::Simple3DApp BasicCamera::BasicCamera ArgumentViewer::ArgumentViewer assimp FreeImage::FreeImage FreeImagePlus::FreeImagePlus Vars::Vars -lstdc++fs)

target_link_libraries(${CONVERT} PUBLIC /home/ichlubna/Workspace/lf/render/Compressonator/Compressonator/Make/libCompressonator.a -lHalf -lpthread)
target_link_libraries(${CONVERT} PUBLIC FreeImage::FreeImage FreeImagePlus::FreeImagePlus)

function(getSharedLibraries out lib cfg)
  #message("getSharedLibraries(${lib} ${cfg})")

  if(TARGET ${lib})
    get_target_property(target_type ${lib} TYPE)
    if(${target_type} STREQUAL "INTERFACE_LIBRARY")
      #message("${lib} JE ${target_type}")
      return()
    endif()
  else()
    #message("${lib} NENI TARGET")
    return()
  endif()


  get_target_property(dll ${lib} IMPORTED_LOCATION_${cfg})
  list(APPEND dlls ${dll})

  get_target_property(interfaceLibs ${lib} INTERFACE_LINK_LIBRARIES)
  if(NOT "${interfaceLibs}" STREQUAL "interfaceLibs-NOTFOUND")
    foreach(interfaceLib ${interfaceLibs})
      getSharedLibraries(ilibs ${interfaceLib} ${cfg})
      list(APPEND dlls ${ilibs})
    endforeach()
  endif()
  list(REMOVE_DUPLICATES dlls)
  set(${out} ${dlls} PARENT_SCOPE)
endfunction()


function(getAllSharedLibraries allLibraries app cfg)
  get_target_property(libs ${app} LINK_LIBRARIES)
  foreach(lib ${libs})
    getSharedLibraries(libList ${lib} ${cfg})
    #message("${lib} ###### ${libList}")
    list(APPEND allLibs ${libList})
  endforeach()
  list(REMOVE_DUPLICATES allLibs)
  set(${allLibraries} ${allLibs} PARENT_SCOPE)
endfunction()

getAllSharedLibraries(allDebugSharedLibraries ${RENDER} DEBUG)
getAllSharedLibraries(allReleaseSharedLibraries ${RENDER} RELEASE)

#message("SharedDebug  : ${allDebugSharedLibraries}")
#message("SharedRelease: ${allReleaseSharedLibraries}")

if(MSVC)
  foreach(lib ${allDebugSharedLibraries})
    file(COPY ${lib} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
  endforeach()
  foreach(lib ${allReleaseSharedLibraries})
    file(COPY ${lib} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
  endforeach()
endif() 

install(TARGETS ${RENDER} RUNTIME DESTINATION .)
install(FILES ${allDebugSharedLibraries} DESTINATION .)
