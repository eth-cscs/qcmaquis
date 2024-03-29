
#add_definitions(-DHAVE_ALPS_HDF5 -DDISABLE_MATRIX_ELEMENT_ITERATOR_WARNING -DALPS_DISABLE_MATRIX_ELEMENT_ITERATOR_WARNING)
enable_omp_if_found()
include_directories(. ${CMAKE_CURRENT_BINARY_DIR})

# *** Targets
add_library(maquis SHARED export_collections.cpp maquis.cpp)

target_link_libraries(maquis optimizer dmrg_models dmrg_utils wignerj alps ${Maquis_Boost_LIBRARIES} Boost::python36 ${Python3_LIBRARIES})

target_include_directories(maquis PUBLIC ${Python3_INCLUDE_DIRS})
set_property(TARGET maquis PROPERTY POSITION_INDEPENDENT_CODE TRUE)

add_library(cpp_maquis SHARED cpp_maquis.cpp)
target_link_libraries(cpp_maquis optimizer dmrg_models dmrg_utils wignerj alps ${Maquis_Boost_LIBRARIES})
set_property(TARGET cpp_maquis PROPERTY POSITION_INDEPENDENT_CODE TRUE)

add_library(dmrgbox SHARED export_collections.cpp dmrgbox.cpp)
target_link_libraries(dmrgbox cpp_maquis optimizer dmrg_models dmrg_utils wignerj alps ${Maquis_Boost_LIBRARIES} Boost::python36 ${Python3_LIBRARIES})
#target_link_libraries(dmrgbox cpp_maquis Boost::python36 ${Python3_LIBRARIES})
target_include_directories(dmrgbox PUBLIC ${Python3_INCLUDE_DIRS})
set_property(TARGET dmrgbox PROPERTY POSITION_INDEPENDENT_CODE TRUE)

# *** Install
install(TARGETS maquis COMPONENT libraries DESTINATION lib/python/maquis)
install(TARGETS cpp_maquis COMPONENT libraries DESTINATION lib)
