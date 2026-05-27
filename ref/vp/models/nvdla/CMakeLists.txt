# Copyright (C) 2014, GreenSocs ltd.
#
# CMakeLists.txt
#
# KONRAD Frederic <fred.konrad@greensocs.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses>.
#
# Linking GreenSocs code, statically or dynamically with other modules
# is making a combined work based on GreenSocs code. Thus, the terms and
# conditions of the GNU General Public License cover the whole
# combination.
#
# In addition, as a special exception, the copyright holders, GreenSocs
# Ltd, give you permission to combine GreenSocs code with free software
# programs or libraries that are released under the GNU LGPL, under the
# OSCI license, under the OCP TLM Kit Research License Agreement or
# under the OVP evaluation license.You may copy and distribute such a
# system following the terms of the GNU GPL and the licenses of the
# other code concerned.
#
# Note that people who make modified versions of GreenSocs code are not
# obligated to grant this special exception for their modified versions;
# it is their choice whether to do so. The GNU General Public License
# gives permission to release a modified version without this exception;
# this exception also makes it possible to release a modified version
# which carries forward this exception.

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(NVDLA)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

FIND_PACKAGE(SystemC)
if(SystemC_FOUND)
    INCLUDE_DIRECTORIES(${SystemC_INCLUDE_DIRS})
else()
    MESSAGE(FATAL_ERROR "SystemC library not found.")
endif()

FIND_PACKAGE(TLM)
if(TLM_FOUND)
    INCLUDE_DIRECTORIES(${TLM_INCLUDE_DIRS})
else()
    MESSAGE( FATAL_ERROR "TLM library not found." )
endif()

FIND_PACKAGE(GREENLIB)
if(GREENLIB_FOUND)
    INCLUDE_DIRECTORIES(${GREENLIB_INCLUDE_DIRS})
    LINK_DIRECTORIES(${GREENLIB_LIBRARY_DIRS})
else()
    MESSAGE(FATAL_ERROR "GreenLib not found.")
endif()

FIND_PACKAGE(NvdlaCmod)
if(NVDLA_CMOD_FOUND)
    INCLUDE_DIRECTORIES(${NVDLA_CMOD_INCLUDE_DIR})
    LINK_DIRECTORIES(${NVDLA_CMOD_LIBRARY_DIR})
else()
    MESSAGE(FATAL_ERROR "NVDLA CMOD library not found.")
endif()


### NVDLA CMOD ###
INSTALL(DIRECTORY ${NVDLA_CMOD_INCLUDE_DIR}/ DESTINATION include)
INSTALL(DIRECTORY ${NVDLA_CMOD_LIBRARY_DIR}/ DESTINATION lib)

set (INCLUDE_DIRS 
  ${INCLUDE_DIRS} 
  include 
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper
  ${NVDLA_HW_PREFIX}/cmod/hls/include
  ${NVDLA_HW_PREFIX}/cmod/hls/cdma_libs
  ${NVDLA_HW_PREFIX}/cmod/hls/cdp_libs
  ${NVDLA_HW_PREFIX}/cmod/hls/csc_libs
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs
  ${NVDLA_HW_PREFIX}/cmod/include
  ${NVDLA_HW_PREFIX}/cmod/include/nvdla_ness_header
  ${NVDLA_HW_PREFIX}/cmod/bdma
  ${NVDLA_HW_PREFIX}/cmod/bdma/gen
  ${NVDLA_HW_PREFIX}/cmod/cacc
  ${NVDLA_HW_PREFIX}/cmod/cacc/gen
  ${NVDLA_HW_PREFIX}/cmod/cbuf
  ${NVDLA_HW_PREFIX}/cmod/cdma
  ${NVDLA_HW_PREFIX}/cmod/cdma/gen
  ${NVDLA_HW_PREFIX}/cmod/cdp
  ${NVDLA_HW_PREFIX}/cmod/cdp/gen
  ${NVDLA_HW_PREFIX}/cmod/cmac
  ${NVDLA_HW_PREFIX}/cmod/cmac/gen
  ${NVDLA_HW_PREFIX}/cmod/csb_master
  ${NVDLA_HW_PREFIX}/cmod/csb_master/gen
  ${NVDLA_HW_PREFIX}/cmod/csc
  ${NVDLA_HW_PREFIX}/cmod/csc/gen
  ${NVDLA_HW_PREFIX}/cmod/cvif
  ${NVDLA_HW_PREFIX}/cmod/cvif/gen
  ${NVDLA_HW_PREFIX}/cmod/glb
  ${NVDLA_HW_PREFIX}/cmod/glb/gen
  ${NVDLA_HW_PREFIX}/cmod/mcif
  ${NVDLA_HW_PREFIX}/cmod/mcif/gen
  ${NVDLA_HW_PREFIX}/cmod/pdp
  ${NVDLA_HW_PREFIX}/cmod/pdp/gen
  ${NVDLA_HW_PREFIX}/cmod/rubik
  ${NVDLA_HW_PREFIX}/cmod/rubik/gen
  ${NVDLA_HW_PREFIX}/cmod/sdp
  ${NVDLA_HW_PREFIX}/cmod/sdp/gen
  ${NVDLA_HW_PREFIX}/cmod/nvdla_top
  ${NVDLA_HW_PREFIX}/cmod/nvdla_core
  ${NVDLA_HW_PREFIX}/cmod/nvdla_clibs
  ${NVDLA_HW_PREFIX}/cmod/nvdla_payload
  ${NVDLA_HW_PREFIX}/cmod/nvdla_top/cmod/include
  ${NVDLA_HW_PREFIX}/outdir/${NVDLA_HW_PROJECT}/spec/defs
  ${NVDLA_HW_PREFIX}/outdir/${NVDLA_HW_PROJECT}/spec/manual
  )

INCLUDE_DIRECTORIES(${INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSC_INCLUDE_DYNAMIC_PROCESSES -Wall -Werror -fpermissive -Wno-deprecated-declarations -Wno-deprecated -Wno-register -Wno-overloaded-virtual -Wno-unknown-pragmas -Wno-unused-local-typedefs -Wno-unused-but-set-variable -Wno-narrowing -Wno-error")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSC_INCLUDE_DYNAMIC_PROCESSES -Wall -Werror -fpermissive -Wno-deprecated-declarations -Wno-deprecated -Wno-register -Wno-overloaded-virtual -Wno-unknown-pragmas -Wno-unused-local-typedefs -Wno-unused-but-set-variable -Wno-narrowing -Wno-error -DDEBUG_LOG=1")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DSC_INCLUDE_DYNAMIC_PROCESSES -Wall -Werror -fpermissive -Wno-deprecated-declarations -Wno-deprecated -Wno-register -Wno-overloaded-virtual -Wno-unknown-pragmas -Wno-unused-local-typedefs -Wno-unused-but-set-variable -Wno-narrowing -Wno-error")

set(NVDLA_ADAPTOR_FILES 
  src/adaptors/gp2str.cpp
  src/adaptors/gsgp2tlm.cpp
  src/adaptors/tlm2gsgp.cpp
  src/adaptors/irq_adaptor.cpp
  )

set(NVDLA_CMOD_COMPAT_FILES
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/cdma_hls_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/cdp_hls_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/cdp_icvt_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/cdp_ocvt_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/csc_hls_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/pdp_hls_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls_wrapper/sdp_hls_wrapper.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/cdma_libs/cdma_cvt.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/cdp_libs/cdp_icvt.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/cdp_libs/cdp_ocvt.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/cdp_libs/cdp_sq.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/csc_libs/csc_cvt.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_c.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_x.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_y.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_y_core.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_y_cvt.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_y_idx.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/sdp/sdp_y_inp.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp16_max.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp16_min.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp16_to_fp17.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp16_to_fp32.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp17_add.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp17_mul.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp17_to_fp16.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp17_to_fp32.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp32_add.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp32_mul.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp32_sub.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/fp32_to_fp17.cpp
  ${NVDLA_HW_PREFIX}/cmod/hls/vlibs/uint16_to_fp17.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_clibs/NvdlaPacker.cpp
  ${NVDLA_HW_PREFIX}/cmod/mcif/gen/NV_NVDLA_mcif.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_core/NV_NVDLA_core.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_core/NvdlaCoreDummy.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_top/NV_nvdla.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_top/NvdlaAxiAdaptor.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_top/NvdlaCsbAdaptor.cpp
  ${NVDLA_HW_PREFIX}/cmod/nvdla_top/NvdlaTopDummy.cpp
  ${NVDLA_HW_PREFIX}/cmod/pdp/gen/pdp_rdma_reg_model.cpp
  ${NVDLA_HW_PREFIX}/cmod/pdp/gen/pdp_reg_model.cpp
  ${NVDLA_HW_PREFIX}/cmod/pdp/NV_NVDLA_pdp.cpp
  ${NVDLA_HW_PREFIX}/cmod/rubik/gen/rbk_reg_model.cpp
  ${NVDLA_HW_PREFIX}/cmod/rubik/NV_NVDLA_rbk.cpp
  ${NVDLA_HW_PREFIX}/cmod/sdp/gen/sdp_rdma_reg_model.cpp
  ${NVDLA_HW_PREFIX}/cmod/sdp/gen/sdp_reg_model.cpp
  ${NVDLA_HW_PREFIX}/cmod/sdp/NV_NVDLA_sdp.cpp
  )

if(MINGW)
    add_library(nvdla STATIC src/nvdla_top.cpp
      ${NVDLA_ADAPTOR_FILES}
      ${NVDLA_CMOD_COMPAT_FILES}
      )
else()
    add_library(nvdla SHARED src/nvdla_top.cpp
      ${NVDLA_ADAPTOR_FILES}
      ${NVDLA_CMOD_COMPAT_FILES}
      )
endif()  

TARGET_LINK_LIBRARIES(nvdla ${GREENLIB_LIBRARIES} ${NVDLA_CMOD_LIBRARY})

# Installation paths
INSTALL(DIRECTORY include/nvdla
        DESTINATION include
)

INSTALL(TARGETS nvdla DESTINATION lib)
