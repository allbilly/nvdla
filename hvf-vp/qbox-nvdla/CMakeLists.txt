gs_create_dymod(Nvdla)

set(NVDLA_HW_DIR "${CMAKE_CURRENT_LIST_DIR}/../../ref/hw" CACHE PATH
    "NVDLA hardware source tree")
set(NVDLA_PROJECT "nv_full" CACHE STRING "NVDLA C-model project")
set(NVDLA_CMOD_DIR "${NVDLA_HW_DIR}/outdir/${NVDLA_PROJECT}/cmod")

target_include_directories(Nvdla PRIVATE
    "${NVDLA_CMOD_DIR}/release/include"
    "${NVDLA_HW_DIR}/cmod/include"
)

target_link_libraries(Nvdla PRIVATE
    "${NVDLA_CMOD_DIR}/release/lib/libnvdla_cmod.dylib"
)

set_target_properties(Nvdla PROPERTIES
    BUILD_RPATH "${NVDLA_CMOD_DIR}/release/lib"
)
