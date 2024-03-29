add_definitions(-DHAVE_ALPS_HDF5 -DDISABLE_MATRIX_ELEMENT_ITERATOR_WARNING -DALPS_DISABLE_MATRIX_ELEMENT_ITERATOR_WARNING)

set(DMRG_APP_LIBRARIES dmrg_utils solver numeric_gpu ${DMRG_LIBRARIES})

if(USE_AMBIENT)
  compile_with_ambient()
endif(USE_AMBIENT)


macro(create_tools_symm_target TNAME SYMM CPPFILE LIBS)
  get_symm_group_name(SYMM_NAME ${SYMM})
  if(DMRG_HAS_${SYMM_NAME})
    add_executable(${TNAME} ${CPPFILE})
    target_link_libraries(${TNAME} ${LIBS})
    set_target_properties(${TNAME} PROPERTIES COMPILE_DEFINITIONS "USE_${SYMM}")
    install(TARGETS ${TNAME} RUNTIME DESTINATION bin COMPONENT applications)
  endif(DMRG_HAS_${SYMM_NAME})
endmacro(create_tools_symm_target)


create_tools_symm_target("mps_overlap_u1"       "U1"     "mps_overlap.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_overlap_2u1"      "TWOU1"  "mps_overlap.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_overlap_2u1pg"    "TWOU1PG"  "mps_overlap.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_overlap_su2u1"    "SU2U1"  "mps_overlap.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_overlap_su2u1pg"  "SU2U1PG"  "mps_overlap.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_overlap_none" "NONE"   "mps_overlap.cpp"  "${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mps_reflect_u1"   "U1"     "mps_reflect.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_reflect_2u1"  "TWOU1"  "mps_reflect.cpp"  "${DMRG_APP_LIBRARIES}")
# create_tools_symm_target("mps_reflect_none" "NONE"   "mps_reflect.cpp"  "${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mps_join_u1"   "U1"     "mps_join.cpp"  "${DMRG_APP_LIBRARIES}")
# create_tools_symm_target("mps_join_2u1"  "TWOU1"  "mps_join.cpp"  "${DMRG_APP_LIBRARIES}")
# create_tools_symm_target("mps_join_none" "NONE"   "mps_join.cpp"  "${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mps_stats_u1"      "U1"      "mps_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_stats_2u1"     "TWOU1"   "mps_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_stats_2u1pg"   "TWOU1PG" "mps_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_stats_u1dg"    "U1DG"    "mps_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_stats_su2u1"   "SU2U1"   "mps_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_stats_su2u1pg" "SU2U1PG" "mps_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mpo_stats_u1"      "U1"      "mpo_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mpo_stats_2u1"     "TWOU1"   "mpo_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mpo_stats_2u1pg"   "TWOU1PG" "mpo_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mpo_stats_u1dg"    "U1DG"    "mpo_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mpo_stats_su2u1"   "SU2U1"   "mpo_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mpo_stats_su2u1pg" "SU2U1PG" "mpo_stats.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

# create_tools_symm_target("visual_mpo_u1"      "U1"      "visual_mpo.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
# create_tools_symm_target("visual_mpo_2u1"     "TWOU1"   "visual_mpo.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mps_upgrade_u1"   "U1"     "mps_upgrade.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_upgrade_2u1"  "TWOU1"  "mps_upgrade.cpp"  "${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps_upgrade_none" "NONE"   "mps_upgrade.cpp"  "${DMRG_APP_LIBRARIES}")

create_tools_symm_target("nrdmexpect_2u1pg"   "TWOU1PG" "nrdmexpect.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("nrdmexpect_su2u1pg" "SU2U1PG" "nrdmexpect.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mps2ci_2u1"           "TWOU1"   "mps2ci.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
create_tools_symm_target("mps2ci_2u1pg"         "TWOU1PG" "mps2ci.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

create_tools_symm_target("deas"             "TWOU1"   "deas.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

create_tools_symm_target("mult_test_2u1pg"   "TWOU1PG" "mult_test.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")

if(DMRG_HAS_TwoU1)
create_tools_symm_target("mps_transform"    "SU2U1"   "mps_transform.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
endif(DMRG_HAS_TwoU1)
if(DMRG_HAS_TwoU1PG)
create_tools_symm_target("mps_transform_pg" "SU2U1PG" "mps_transform.cpp"  "dmrg_models;${DMRG_APP_LIBRARIES}")
endif(DMRG_HAS_TwoU1PG)


#create_tools_symm_target("shtm_su2u1"   "SU2U1"   "shtm.cpp"  "${DMRG_APP_LIBRARIES}")
#create_tools_symm_target("shtm_su2u1pg" "SU2U1PG" "shtm.cpp"  "${DMRG_APP_LIBRARIES}")
