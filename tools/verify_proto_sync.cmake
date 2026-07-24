cmake_minimum_required(VERSION 3.16)

get_filename_component(AUTOVIZ_REPOSITORY_ROOT
                       "${CMAKE_CURRENT_LIST_DIR}/.."
                       ABSOLUTE)
set(AUTOVIZ_CLIENT_PROTO_ROOT
    "${AUTOVIZ_REPOSITORY_ROOT}/AutoVizClient/proto")
set(AUTOVIZ_SERVER_PROTO_ROOT
    "${AUTOVIZ_REPOSITORY_ROOT}/AutoVizServer/src/autoviz_server/proto")

file(GLOB_RECURSE AUTOVIZ_CLIENT_PROTO_FILES
     RELATIVE "${AUTOVIZ_CLIENT_PROTO_ROOT}"
     "${AUTOVIZ_CLIENT_PROTO_ROOT}/*.proto")
file(GLOB_RECURSE AUTOVIZ_SERVER_PROTO_FILES
     RELATIVE "${AUTOVIZ_SERVER_PROTO_ROOT}"
     "${AUTOVIZ_SERVER_PROTO_ROOT}/*.proto")
list(SORT AUTOVIZ_CLIENT_PROTO_FILES)
list(SORT AUTOVIZ_SERVER_PROTO_FILES)

if(NOT AUTOVIZ_CLIENT_PROTO_FILES STREQUAL AUTOVIZ_SERVER_PROTO_FILES)
    message(FATAL_ERROR
            "Client/Server proto file sets differ.\n"
            "Client: ${AUTOVIZ_CLIENT_PROTO_FILES}\n"
            "Server: ${AUTOVIZ_SERVER_PROTO_FILES}")
endif()

foreach(proto_file IN LISTS AUTOVIZ_CLIENT_PROTO_FILES)
    file(SHA256
         "${AUTOVIZ_CLIENT_PROTO_ROOT}/${proto_file}"
         client_hash)
    file(SHA256
         "${AUTOVIZ_SERVER_PROTO_ROOT}/${proto_file}"
         server_hash)
    if(NOT client_hash STREQUAL server_hash)
        message(FATAL_ERROR
                "Client/Server proto mismatch: ${proto_file}")
    endif()
endforeach()

list(LENGTH AUTOVIZ_CLIENT_PROTO_FILES proto_file_count)
message(STATUS
        "AutoViz Client/Server proto schemas are identical (${proto_file_count} files)")
