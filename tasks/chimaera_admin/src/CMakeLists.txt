#------------------------------------------------------------------------------
# Build CHI Admin Task Library
#------------------------------------------------------------------------------
add_chimod_library(chimaera_admin ${CMAKE_CURRENT_SOURCE_DIR}/chimaera_admin.cc)
#------------------------------------------------------------------------------
# Install CHI Admin Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        chimaera_admin
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(chimaera_admin)
endif()
