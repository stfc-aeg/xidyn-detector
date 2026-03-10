# Install headers from the include directory
file(GLOB XIDYN_INCLUDES *.h)
INSTALL(FILES ${XIDYN_INCLUDES} DESTINATION include/xidyn)
