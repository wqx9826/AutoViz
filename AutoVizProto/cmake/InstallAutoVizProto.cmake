# Install the SDK and export AutoVizProto::AutoVizProto for find_package().

include(CMakePackageConfigHelpers)

install(
    TARGETS autoviz_proto
    EXPORT AutoVizProtoTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    DIRECTORY "${AUTOVIZ_PROTO_GENERATED_DIR}/autoviz/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/autoviz"
    FILES_MATCHING PATTERN "*.h"
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/proto/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AutoVizProto/proto"
    FILES_MATCHING PATTERN "*.proto"
)

set(AUTOVIZ_PROTO_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/AutoVizProto"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AutoVizProtoConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfig.cmake"
    INSTALL_DESTINATION "${AUTOVIZ_PROTO_CMAKE_INSTALL_DIR}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfigVersion.cmake"
    DESTINATION "${AUTOVIZ_PROTO_CMAKE_INSTALL_DIR}"
)
install(
    EXPORT AutoVizProtoTargets
    FILE AutoVizProtoTargets.cmake
    NAMESPACE AutoVizProto::
    DESTINATION "${AUTOVIZ_PROTO_CMAKE_INSTALL_DIR}"
)
