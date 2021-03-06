# Straight from the SWIG manual:
# http://www.swig.org/Doc1.3/Introduction.html#Introduction%5Fbuild%5Fsystem

FIND_PACKAGE(SWIG)
# If we can't find SWIG, print a message saying so and don't add projects
IF(NOT SWIG_FOUND)
  MESSAGE(STATUS "SWIG not found, not building cross language libraries!")
  RETURN()
ENDIF()
MESSAGE(STATUS "Using SWIG!")
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(PythonLibs REQUIRED)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(CMAKE_SWIG_FLAGS "-c++")

SET_SOURCE_FILES_PROPERTIES(omron.i PROPERTIES CPLUSPLUS ON)
SET_SOURCE_FILES_PROPERTIES(omron.i PROPERTIES SWIG_FLAGS "-c++")

# Python
SWIG_ADD_MODULE(omron python omron.i Omron.cxx)
SWIG_LINK_LIBRARIES(omron ${PYTHON_LIBRARIES} ${libomron_LIBRARY} ${LIBOMRON_REQUIRED_LIBS})
INSTALL_PROGRAMS(/python/omron omron.py ../lib/_omron.so)
