set(SOURCES_CONVERT
  main.cpp
  DDS_Helpers.cpp
  microtar.c
  )

set(INCLUDES_CONVERT
   Compressonator.h
   DDS_Helpers.h
   microtar.h
  )

set(CONVERT convertLf)

add_executable(${CONVERT} ${SOURCES_CONVERT} ${INCLUDES_CONVERT})

target_include_directories(${CONVERT} PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories(${CONVERT} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/convert>)
target_include_directories(${CONVERT} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../Compressonator/Compressonator/Header/>)

target_link_libraries(${CONVERT} PUBLIC /home/ichlubna/Workspace/lf/render/Compressonator/Compressonator/Make/libCompressonator.a -lHalf -lpthread)
target_link_libraries(${CONVERT} PUBLIC FreeImage::FreeImage FreeImagePlus::FreeImagePlus)

