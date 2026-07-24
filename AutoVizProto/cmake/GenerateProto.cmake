# Generate autoviz/*.pb.h and autoviz/*.pb.cc in the build directory.

set(AUTOVIZ_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(AUTOVIZ_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AUTOVIZ_PROTO_FILES
    autoviz/common.proto
    autoviz/vehicle.proto
    autoviz/planning.proto
    autoviz/perception.proto
    autoviz/control.proto
    autoviz/runtime.proto
    autoviz/transport.proto
)

set(AUTOVIZ_PROTO_GENERATED_SOURCES)
set(AUTOVIZ_PROTO_GENERATED_HEADERS)
set(AUTOVIZ_PROTO_ABSOLUTE_FILES)

foreach(proto_file IN LISTS AUTOVIZ_PROTO_FILES)
    string(REPLACE ".proto" ".pb.cc" generated_source "${proto_file}")
    string(REPLACE ".proto" ".pb.h" generated_header "${proto_file}")

    list(APPEND AUTOVIZ_PROTO_GENERATED_SOURCES
         "${AUTOVIZ_PROTO_GENERATED_DIR}/${generated_source}")
    list(APPEND AUTOVIZ_PROTO_GENERATED_HEADERS
         "${AUTOVIZ_PROTO_GENERATED_DIR}/${generated_header}")
    list(APPEND AUTOVIZ_PROTO_ABSOLUTE_FILES
         "${AUTOVIZ_PROTO_ROOT}/${proto_file}")
endforeach()

add_custom_command(
    OUTPUT
        ${AUTOVIZ_PROTO_GENERATED_SOURCES}
        ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOVIZ_PROTO_GENERATED_DIR}"
    COMMAND protobuf::protoc
            --cpp_out=${AUTOVIZ_PROTO_GENERATED_DIR}
            --proto_path=${AUTOVIZ_PROTO_ROOT}
            ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    DEPENDS ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    COMMENT "Generating AutoViz protobuf sources"
    VERBATIM
)
