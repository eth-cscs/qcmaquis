add_definitions(-DHAVE_ALPS_HDF5 -DDISABLE_MATRIX_ELEMENT_ITERATOR_WARNING -DALPS_DISABLE_MATRIX_ELEMENT_ITERATOR_WARNING)

set(DMRG_APP_LIBRARIES dmrg_utils ${DMRG_LIBRARIES} solver numeric_gpu)

add_executable(qrdecomp.test qrdecomp.cpp)
target_link_libraries(qrdecomp.test ${DMRG_APP_LIBRARIES})

add_test(qrdecomp qrdecomp.test)
