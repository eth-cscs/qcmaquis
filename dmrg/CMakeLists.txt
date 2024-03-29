
cmake_minimum_required(VERSION 3.10)

######################################################################
# Machine Config
######################################################################
if(MACHINE_CONFIG)
  if(EXISTS ${MACHINE_CONFIG})
    message(STATUS "Loading config in " ${MACHINE_CONFIG})
    include(${MACHINE_CONFIG})
  else(EXISTS ${MACHINE_CONFIG})
    message(ERROR " Machine config not found!")
  endif(EXISTS ${MACHINE_CONFIG})
endif(MACHINE_CONFIG)

if(NOT DEFAULT_BLAS_LAPACK)
    set(DEFAULT_BLAS_LAPACK alps)
endif(NOT DEFAULT_BLAS_LAPACK)

######################################################################
# Project START
######################################################################
project (MAQUIS_DMRG) 
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/config)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/opt/maquis" CACHE PATH "MAQUIS install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)


set(BUILD_SYMMETRIES "SU2U1;SU2U1PG" CACHE STRING "List of symmetry
to include in the build objects. U1, TwoU1, TwoU1PG, NU1, SU2U1, SU2U1PG and NONE are currently available.")
set(DMRG_NUMSYMM 6 CACHE STRING "Maximum number of U1 symmetries.")

option(BUILD_INTERFACE_LIBS "Build libraries for interfacing DMRG to C++ and python" ON)
option(COMPLEX "compile dmrg for complex numbers" OFF)

option(ENABLE_ALPS_MODELS "Enable binding with ALPS lattices and models" ON)
option(ENABLE_OMP "Enable OpenMP" ON)

option(ENABLE_COLLECTOR "Enable profiling through DataCollector object" OFF)

set(BENCHMARKS_DIR "../../benchmarks" CACHE PATH "Location of benchmarks directory.")
option(REGRESSION_UNIT_TESTING      "Build and run regression unit testing" OFF)
option(REGRESSION_APPLICATION_TESTS "Run application tests"                 OFF)
option(REGRESSION_PERFORMANCE_TESTS "Run application benchmarks"            OFF)

mark_as_advanced(ENABLE_COLLECTOR)

set(BLAS_LAPACK_SELECTOR "mkl_parallel" CACHE STRING "auto, manual, mkl_sequential, mkl_parallel, veclib, alps")


######################################################################
# How to build
######################################################################
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  include("config/clang_fixes.cmake")
endif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Type of build" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE "-Wno-unknown-pragmas ${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "CMAKE_CXX_FLAGS_RELEASE: Flags used by the compiler during release builds" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Wno-unknown-pragmas ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "CMAKE_CXX_FLAGS_RELWITHDEBINFO: Flags used by the compiler during Release with Debug Info builds." FORCE)
  set(CMAKE_C_FLAGS_RELEASE " ${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "CMAKE_C_FLAGS_RELEASE: Flags used by the compiler during release builds" FORCE)
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

######################################################################
# Floating types
######################################################################

if(COMPLEX)
  set(MATRIX_TYPES "matrix;cmatrix")
  message(STATUS "Enabling complex numbers.")
  list(APPEND DMRG_DEFINITIONS -DHAVE_COMPLEX)
  set(DMRG_HAS_COMPLEX TRUE)
else(COMPLEX)
  set(MATRIX_TYPES "matrix")
  set(DMRG_HAS_COMPLEX FALSE)
endif(COMPLEX)

######################################################################
# Symmetries management
######################################################################

macro(get_symm_suffix RET SYMM)
  set(${RET} ${SYMM})
  string(REGEX REPLACE "^U1$" "u1" ${RET} ${${RET}})
  string(REGEX REPLACE "^TWOU1$" "2u1" ${RET} ${${RET}})
  string(REGEX REPLACE "^TwoU1$" "2u1" ${RET} ${${RET}})
  string(REGEX REPLACE "^NU1$"  "nu1" ${RET} ${${RET}})
  string(REGEX REPLACE "^TWOU1PG$" "2u1pg" ${RET} ${${RET}})
  string(REGEX REPLACE "^TwoU1PG$" "2u1pg" ${RET} ${${RET}})
  string(REGEX REPLACE "^NONE$" "none" ${RET} ${${RET}})
#  string(REGEX REPLACE "^Z2$" "Ztwo" ${RET} ${${RET}})
  string(REGEX REPLACE "^SU2U1$" "su2u1" ${RET} ${${RET}})
  string(REGEX REPLACE "^SU2U1PG$" "su2u1pg" ${RET} ${${RET}})
  string(REGEX REPLACE "^U1DG$" "u1dg" ${RET} ${${RET}})
endmacro(get_symm_suffix)

macro(get_symm_group_name RET SYMM)
  set(${RET} ${SYMM})
  string(REGEX REPLACE "^U1$" "U1" ${RET} ${${RET}})
  string(REGEX REPLACE "^TWOU1$" "TwoU1" ${RET} ${${RET}})
  string(REGEX REPLACE "^TwoU1$" "TwoU1" ${RET} ${${RET}})
  string(REGEX REPLACE "^NU1$"  "NU1" ${RET} ${${RET}})
  string(REGEX REPLACE "^TWOU1PG$" "TwoU1PG" ${RET} ${${RET}})
  string(REGEX REPLACE "^TwoU1PG$" "TwoU1PG" ${RET} ${${RET}})
  string(REGEX REPLACE "^NONE$" "TrivialGroup" ${RET} ${${RET}})
#  string(REGEX REPLACE "^Z2$" "Ztwo" ${RET} ${${RET}})
  string(REGEX REPLACE "^SU2U1$" "SU2U1" ${RET} ${${RET}})
  string(REGEX REPLACE "^SU2U1PG$" "SU2U1PG" ${RET} ${${RET}})
  string(REGEX REPLACE "^U1DG$" "U1DG" ${RET} ${${RET}})
endmacro(get_symm_group_name)

macro(get_symm_files TYPE RET FILEBASE)
  if(NOT ${TYPE} STREQUAL "APPEND")
    set(${RET} "")
  endif(NOT ${TYPE} STREQUAL "APPEND")
  foreach(SYMM ${BUILD_SYMMETRIES})
    get_symm_suffix(SYMM_SUFFIX ${SYMM})
    string(REPLACE "{SYMM}" ${SYMM_SUFFIX} SYMM_FILE ${FILEBASE})
    list(APPEND ${RET} ${SYMM_FILE})
  endforeach(SYMM)
endmacro(get_symm_files)

macro(configure_symm_file INPUT OUTBASE VARNAME)
  foreach(SYMM ${BUILD_SYMMETRIES})
    get_symm_suffix(SYMM_SUFFIX ${SYMM})
    get_symm_group_name(${VARNAME} ${SYMM})
    string(REPLACE "{SYMM}" ${SYMM_SUFFIX} SYMM_FILE ${OUTBASE})
    configure_file(${INPUT} ${SYMM_FILE})
  endforeach(SYMM)
endmacro(configure_symm_file)

macro(get_matrix_files TYPE RET FILEBASE)
  if(NOT ${TYPE} STREQUAL "APPEND")
    set(${RET} "")
  endif(NOT ${TYPE} STREQUAL "APPEND")
  foreach(SYMM ${BUILD_SYMMETRIES})
    foreach(MAT ${MATRIX_TYPES})
      get_symm_suffix(SYMM_SUFFIX ${SYMM})
      string(REPLACE "{SYMM}" ${SYMM_SUFFIX} SYMM_FILE ${FILEBASE})
      string(REPLACE "{MATRIX}" ${MAT} SYMM_FILE ${SYMM_FILE})
      list(APPEND ${RET} ${SYMM_FILE})
    endforeach(MAT)
  endforeach(SYMM)
endmacro(get_matrix_files)

macro(configure_matrix_file INPUT OUTBASE VARNAME VARNAME2)
  foreach(SYMM ${BUILD_SYMMETRIES})
    foreach(MAT ${MATRIX_TYPES})
      get_symm_suffix(SYMM_SUFFIX ${SYMM})
      get_symm_group_name(${VARNAME2} ${SYMM})
      set(${VARNAME} ${MAT})
      string(REPLACE "{MATRIX}" ${MAT} SYMM_FILE ${OUTBASE})
      string(REPLACE "{SYMM}" ${SYMM_SUFFIX} SYMM_FILE ${SYMM_FILE})
      configure_file(${INPUT} ${SYMM_FILE})
    endforeach(MAT)
  endforeach(SYMM)
endmacro(configure_matrix_file)


# Adding symmetries definitions
foreach(SYMM ${BUILD_SYMMETRIES})
  get_symm_group_name(SYMM_NAME ${SYMM})
  message(STATUS "Enabling ${SYMM_NAME} symmetry.")
  list(APPEND DMRG_DEFINITIONS -DHAVE_${SYMM_NAME})
  set(DMRG_HAS_${SYMM_NAME} TRUE)
endforeach(SYMM)


######################################################################
# Libraries
######################################################################
# OpenMP
if(NOT OpenMP_FOUND)
  find_package(OpenMP)
endif(NOT OpenMP_FOUND)

# ALPS
if(NOT ALPS_DIR AND ALPS_ROOT_DIR)
  set(ALPS_DIR ${ALPS_ROOT_DIR}/share/alps)
endif(NOT ALPS_DIR AND ALPS_ROOT_DIR)
if(NOT ALPS_DIR AND DEFINED ENV{ALPS_ROOT})
  set(ALPS_DIR $ENV{ALPS_ROOT}/share/alps)
endif(NOT ALPS_DIR AND DEFINED ENV{ALPS_ROOT})


set(PREVENT_ALPS_COMPILERS ON)
find_package(ALPS REQUIRED NO_SYSTEM_ENVIRONMENT_PATH)

set(ALPS_LIBRARY_DIRS ${ALPS_LIBRARY_DIRS} CACHE STRING "ALPS library dirs")
set(ALPS_INCLUDE_DIRS ${ALPS_INCLUDE_DIRS} CACHE STRING "ALPS include dirs")
if(ALPS_HDF5_INCLUDE_DIR) # support for old ALPS revisions
  set(HDF5_INCLUDE_DIR ${ALPS_HDF5_INCLUDE_DIR} CACHE STRING "")
endif(ALPS_HDF5_INCLUDE_DIR)
if(ALPS_HDF5_LIBRARIES) # support for old ALPS revisions
  set(HDF5_LIBRARIES ${ALPS_HDF5_LIBRARIES} CACHE STRING "")
endif(ALPS_HDF5_LIBRARIES)

mark_as_advanced(ALPS_LIBRARY_DIRS ALPS_INCLUDE_DIRS HDF5_INCLUDE_DIR HDF5_LIBRARIES)


# This forces use of cmake's own configuration for Boost, rather than the one that Boost provides
# starting from version 1.70.0
set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost COMPONENTS date_time program_options filesystem regex system serialization thread python36)
set(Maquis_Boost_LIBRARIES Boost::regex Boost::system Boost::filesystem Boost::serialization
Boost::date_time Boost::program_options Boost::thread)


# BLAS/LAPACK selector
find_package(MAQUISLapack)
message(STATUS "Lapack include:  ${MAQUISLapack_INCLUDE_DIRS}")
message(STATUS "Lapack lib dirs: ${MAQUISLapack_LIB_DIRS}")
message(STATUS "Lapack libs:     ${MAQUISLapack_LIBRARIES}")


find_package(CUDA)

find_package(Python3 COMPONENTS Interpreter Development)

######################################################################
# Version information
######################################################################
set(DMRG_YEAR 2020)
set(DMRG_VERSION_MAJOR 0)
set(DMRG_VERSION_MINOR 6)
set(DMRG_VERSION_BUILD "")

if(NOT DMRG_VERSION_BUILD AND EXISTS ${PROJECT_SOURCE_DIR}/.svn) 
  FIND_PACKAGE(Subversion) 
  IF(Subversion_FOUND) 
    Subversion_WC_INFO(${PROJECT_SOURCE_DIR} DMRG) 
    string(REPLACE ${DMRG_WC_ROOT} "" DMRG_BRANCH ${DMRG_WC_URL})
    set(DMRG_VERSION_BUILD "r${DMRG_WC_REVISION} (${DMRG_BRANCH})")
  ENDIF(Subversion_FOUND) 
endif(NOT DMRG_VERSION_BUILD AND EXISTS ${PROJECT_SOURCE_DIR}/.svn) 

if(EXISTS ${PROJECT_SOURCE_DIR}/../.git)
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --max-count=1 HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_REVISION
            ERROR_QUIET
            )
        if(NOT ${GIT_REVISION} STREQUAL "")
            string(STRIP ${GIT_REVISION} GIT_REVISION)
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_BRANCH
            ERROR_QUIET
            )
        if(NOT ${GIT_BRANCH} STREQUAL "")
            string(STRIP ${GIT_BRANCH} GIT_BRANCH)
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 --tags
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE SVN_REV
            ERROR_QUIET
            )

        if(NOT ${SVN_REV} STREQUAL "")
            string(STRIP ${SVN_REV} SVN_REV)
        endif()

        set(DMRG_VERSION_BUILD "${GIT_REVISION} (${GIT_BRANCH}) ${SVN_REV}")
    endif() 
endif(EXISTS ${PROJECT_SOURCE_DIR}/../.git)

if(DMRG_VERSION_BUILD)
  set(DMRG_VERSION "${DMRG_VERSION_MAJOR}.${DMRG_VERSION_MINOR}-${DMRG_VERSION_BUILD}")
else(DMRG_VERSION_BUILD)
  set(DMRG_VERSION "${DMRG_VERSION_MAJOR}.${DMRG_VERSION_MINOR}")
endif(DMRG_VERSION_BUILD)
set(DMRG_VERSION_STRING "DMRG version ${DMRG_VERSION}")
MESSAGE(STATUS "DMRG version: ${DMRG_VERSION}") 


######################################################################
# Parsing user options
######################################################################

set(DMRG_INCLUDE_DIRS ${MAQUIS_Boost_INCLUDE_DIRS})
set(DMRG_LIBRARY_DIRS ${MAQUIS_Boost_LIBRARY_DIRS})
#set(DMRG_DEFINITIONS)
#set(DMRG_LIBRARIES alps wignerj ${Boost_LIBRARIES} ${HDF5_LIBRARIES} ${CUDA_LIBRARIES} )
set(DMRG_LIBRARIES alps wignerj ${Maquis_Boost_LIBRARIES} ${HDF5_LIBRARIES} ${CUDA_LIBRARIES} )
if(USE_AMBIENT)
  list(APPEND DMRG_LIBRARIES ${AMBIENT_LIBRARIES})
else()
  list(APPEND DMRG_LIBRARIES ${MAQUISLapack_LIBRARIES})
endif(USE_AMBIENT)

# OpenMP
if(ENABLE_OMP)
  if(OPENMP_FOUND)
    macro(enable_omp_if_found)
      #add_definitions(${OpenMP_C_FLAGS} ${OpenMP_CXX_FLAGS})
      add_definitions(-DMAQUIS_OPENMP)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    endmacro(enable_omp_if_found)
  else(OPENMP_FOUND)
    message("OpenMP not found")
  endif(OPENMP_FOUND)
else(ENABLE_OMP)
  macro(enable_omp_if_found)
  endmacro(enable_omp_if_found)
endif(ENABLE_OMP)


######################################################################
# Include / link directories
######################################################################

list(APPEND DMRG_INCLUDE_DIRS ${MATRICES_DIR} ${HDF5_INCLUDE_DIR} ${ALPS_INCLUDE_DIRS} ${CUDA_INCLUDE_DIRS})
list(APPEND DMRG_LIBRARY_DIRS ${ALPS_LIBRARY_DIRS})
if(NOT USE_AMBIENT)
  list(APPEND DMRG_LIBRARY_DIRS ${MAQUISLapack_LIB_DIRS})
endif(NOT USE_AMBIENT)

include_directories(${PROJECT_SOURCE_DIR}/framework ${PROJECT_BINARY_DIR}/framework ${DMRG_INCLUDE_DIRS})
link_directories(${DMRG_LIBRARY_DIRS})
add_definitions(${DMRG_DEFINITIONS})

set(DMRG_FRAMEWORK_DIR ${PROJECT_SOURCE_DIR}/framework)
if(CMAKE_EXE_LINKER_FLAGS MATCHES "^ $")
    set(CMAKE_EXE_LINKER_FLAGS ${MAQUISLapack_LINKER_FLAGS})
else(CMAKE_EXE_LINKER_FLAGS MATCHES "^ $")
    list(APPEND CMAKE_EXE_LINKER_FLAGS ${MAQUISLapack_LINKER_FLAGS})
endif(CMAKE_EXE_LINKER_FLAGS MATCHES "^ $")


######################################################################
# Configure files
######################################################################

configure_file(framework/dmrg/version.h.in ${CMAKE_BINARY_DIR}/framework/dmrg/version.h)


######################################################################
# Targets
######################################################################

# DataCollector
if(ENABLE_COLLECTOR)
  add_definitions(-DENABLE_DATACOLLECTORS)
  add_library(maquis_collector framework/utils/data_collector.cpp)
  list(APPEND DMRG_LIBRARIES maquis_collector)
endif(ENABLE_COLLECTOR)


if(USE_IMGTE_ONLY)
  add_definitions(-DIMG_ONLY)
endif(USE_IMGTE_ONLY)


# *** libraries / utilities
add_subdirectory(framework/dmrg)

# *** applications
add_subdirectory(applications)

# *** regression tests
if(REGRESSION_UNIT_TESTING)
  #add_subdirectory(tests)
  enable_testing()
  if(NOT ${CMAKE_BUILD_TYPE} MATCHES "Debug")
      message(WARNING "Tests should be checked in Debug mode!")
  endif()
  add_subdirectory(regression/tests)
  # add_subdirectory(regression/block_matrix)
  # add_subdirectory(regression/mp_tensors)
  # add_subdirectory(regression/dmrg)
  # add_subdirectory(regression/te)
endif(REGRESSION_UNIT_TESTING)

if(REGRESSION_APPLICATION_TESTS)
  enable_testing()
  add_subdirectory(regression/applications)
endif(REGRESSION_APPLICATION_TESTS)

if(REGRESSION_PERFORMANCE_TESTS)
# *** benchmark applications
  add_subdirectory(regression/benchmarks)
  #enable_testing()
  #add_subdirectory(${BENCHMARKS_DIR} benchmarks_refs)
endif(REGRESSION_PERFORMANCE_TESTS)

# *** python interface
if(BUILD_INTERFACE_LIBS)
    add_subdirectory(lib/interface)
endif(BUILD_INTERFACE_LIBS)

######################################################################
# DMRG exports
######################################################################

## For build tree
set(CONF_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/framework ${PROJECT_BINARY_DIR}/framework ${DMRG_INCLUDE_DIRS})
configure_file(MAQUIS_DMRGConfig.cmake.in ${PROJECT_BINARY_DIR}/MAQUIS_DMRGConfig.cmake @ONLY)

## For install
set(CONF_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include ${DMRG_INCLUDE_DIRS})
configure_file(MAQUIS_DMRGConfig.cmake.in ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/MAQUIS_DMRGConfig.cmake @ONLY)

install(FILES ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/MAQUIS_DMRGConfig.cmake DESTINATION share/cmake  COMPONENT dev)
install(EXPORT DMRGTargets DESTINATION share/cmake COMPONENT dev)


######################################################################
# Install
######################################################################

install(FILES ${CMAKE_BINARY_DIR}/framework/dmrg/version.h
        DESTINATION include/framework/dmrg COMPONENT headers)

#TODO: improve using GLOB
install(DIRECTORY framework/dmrg framework/ietl framework/utils
        DESTINATION include
        COMPONENT headers
        PATTERN ".svn" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE
        PATTERN "*.cpp" EXCLUDE
        PATTERN "*.C" EXCLUDE
        PATTERN "*.c" EXCLUDE
        PATTERN "*.cu" EXCLUDE
        PATTERN "*.in" EXCLUDE
        PATTERN "CMakeLists.txt" EXCLUDE
        PATTERN "Makefile*" EXCLUDE
        )

install(DIRECTORY lib/python DESTINATION lib  COMPONENT python
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN "*.py" PATTERN "*.pyc"
        PATTERN ".svn" EXCLUDE PATTERN "CVS" EXCLUDE)
