global_compile_options()

# API
set(API_DIR ../odalpapi)
file(GLOB API_HEADERS ${API_DIR}/*.h ${API_DIR}/threads/*.h)
file(GLOB API_SOURCES ${API_DIR}/*.cpp ${API_DIR}/threads/*.cpp)

# Odalaunch
set(LAUNCH_DIR res src)
file(GLOB LAUNCH_HEADERS res/*.h src/*.h)
file(GLOB LAUNCH_SOURCES src/*.cpp)

# Build Resource Directory
# This file is generated by the build process.
set(XRCRES_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)
set(XRCRES_HEADER ${XRCRES_DIR}/xrc_resource.h)

# Odalaunch definitions
include_directories(${API_DIR} ${LAUNCH_DIR} ${XRCRES_DIR})

# wxWidgets configuration
find_package(wxWidgets)
if(wxWidgets_FOUND)
  include( "${wxWidgets_USE_FILE}" )
else()
  if(wxWidgets_ROOT_DIR)
    message(WARNING "wxWidgets root directory exists but no compiled libraries were found, odalaunch target not generated.")
  else()
    message(WARNING "wxWidgets not found, odalaunch target not generated.")
  endif()
endif()

# Odalaunch target
if(wxWidgets_FOUND)
  add_custom_command(
    OUTPUT ${XRCRES_HEADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/res
    COMMAND ${wxWidgets_wxrc_EXECUTABLE} -c -o ${XRCRES_HEADER} ${CMAKE_CURRENT_SOURCE_DIR}/res/*.xrc)
  add_executable(odalaunch MACOSX_BUNDLE WIN32
    ${API_SOURCES} ${API_HEADERS}
    ${LAUNCH_SOURCES} ${LAUNCH_HEADERS}
    ${XRCRES_HEADER})
  target_link_libraries(odalaunch ${wxWidgets_LIBRARIES})

  if(WIN32)
    target_link_libraries(odamex ws2_32)
  endif()

  set(MACOSX_BUNDLE_BUNDLE_NAME OdaLaunch)
  set(MACOSX_BUNDLE_INFO_STRING "ODAMEX and its likeness © ${PROJECT_COPYRIGHT} Odamex team.")
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
  set(MACOSX_BUNDLE_ICON_FILE odalaunch.icns)

  if(APPLE)
    add_custom_command(
      TARGET odalaunch
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/odalaunch.app/Contents/Resources
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/media/${MACOSX_BUNDLE_ICON_FILE} ${CMAKE_CURRENT_BINARY_DIR}/odalaunch.app/Contents/Resources/
    )

    set_directory_properties(
      PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
      "${CMAKE_CURRENT_BINARY_DIR}/odalaunch.app/Contents/Resources/${MACOSX_BUNDLE_ICON_FILE}"
    )
  elseif(WIN32)
    install(TARGETS odalaunch
      RUNTIME DESTINATION .
      COMPONENT odalaunch)
  else()
    install(TARGETS odalaunch
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT odalaunch)
  endif()

endif()
