include(GNUInstallDirs)

if(WIN32)
    install(TARGETS AutoViz
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
else()
    install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")\nfile(COPY_FILE \"${CMAKE_CURRENT_BINARY_DIR}/AutoViz\" \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/AutoViz.bin\" ONLY_IF_DIFFERENT)\nfile(CHMOD \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/AutoViz.bin\" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)")
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/AutoViz.sh"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        RENAME AutoViz
    )
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/configs/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/configs"
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/scripts/qt.conf"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(WIN32)
    if(NOT TARGET Qt6::QWindowsIntegrationPlugin OR
       NOT TARGET Qt6::QSQLiteDriverPlugin)
        install(CODE [=[
            message(FATAL_ERROR
                "The selected Qt6 package does not export the Windows/qsqlite plugin targets")
        ]=])
    else()
        install(FILES
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/platforms"
        )
        install(FILES
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/sqldrivers"
        )

        install(CODE "set(autoviz_runtime_search_dirs [==[$<TARGET_FILE_DIR:Qt6::Core>;$<TARGET_FILE_DIR:protobuf::libprotobuf>]==])")
        install(CODE [=[
        if(POLICY CMP0207)
            cmake_policy(SET CMP0207 NEW)
        endif()
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${CMAKE_INSTALL_PREFIX}/bin/AutoViz.exe"
            RESOLVED_DEPENDENCIES_VAR autoviz_runtime_dependencies
            UNRESOLVED_DEPENDENCIES_VAR autoviz_unresolved_dependencies
            DIRECTORIES ${autoviz_runtime_search_dirs}
            PRE_EXCLUDE_REGEXES
                "^api-ms-win-"
                "^ext-ms-win-"
            POST_EXCLUDE_REGEXES
                "^[A-Za-z]:/Windows/"
                "^api-ms-win-"
                "^ext-ms-win-"
                ".*/(advapi32|authz|d3d11|dbghelp|dnsapi|dwmapi|dwrite|dxgi|gdi32|iphlpapi|kernel32|mpr|msvcrt|netapi32|ntdll|ole32|secur32|shell32|user32|userenv|uxtheme|version|winhttp|winmm|ws2_32)\\.dll$"
        )
        if(autoviz_unresolved_dependencies)
            message(FATAL_ERROR
                "AutoViz package has unresolved DLL dependencies: ${autoviz_unresolved_dependencies}")
        endif()
        file(INSTALL
            DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
            TYPE SHARED_LIBRARY
            FILES ${autoviz_runtime_dependencies}
        )
        ]=])
    endif()
else()
    if(NOT TARGET Qt5::QXcbIntegrationPlugin OR
       NOT TARGET Qt5::QOffscreenIntegrationPlugin OR
       NOT TARGET Qt5::QSQLiteDriverPlugin)
        install(CODE [=[
            message(FATAL_ERROR
                "The selected Qt5 package does not export the xcb/offscreen/qsqlite plugin targets")
        ]=])
    else()
        install(FILES
            "$<TARGET_FILE:Qt5::QXcbIntegrationPlugin>"
            "$<TARGET_FILE:Qt5::QOffscreenIntegrationPlugin>"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/platforms"
        )
        install(FILES
            "$<TARGET_FILE:Qt5::QSQLiteDriverPlugin>"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/sqldrivers"
        )

        install(CODE [=[
        set(autoviz_runtime_plugins
            "${CMAKE_INSTALL_PREFIX}/bin/plugins/platforms/libqxcb.so"
            "${CMAKE_INSTALL_PREFIX}/bin/plugins/platforms/libqoffscreen.so"
            "${CMAKE_INSTALL_PREFIX}/bin/plugins/sqldrivers/libqsqlite.so"
        )
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${CMAKE_INSTALL_PREFIX}/bin/AutoViz.bin"
            LIBRARIES ${autoviz_runtime_plugins}
            RESOLVED_DEPENDENCIES_VAR autoviz_runtime_dependencies
            UNRESOLVED_DEPENDENCIES_VAR autoviz_unresolved_dependencies
        )
        foreach(autoviz_dependency IN LISTS autoviz_runtime_dependencies)
            if(autoviz_dependency MATCHES "/libQt5[^/]*\\.so" OR
               autoviz_dependency MATCHES "/libprotobuf[^/]*\\.so")
                file(REAL_PATH "${autoviz_dependency}" autoviz_dependency_real)
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
                    TYPE SHARED_LIBRARY
                    FILES "${autoviz_dependency_real}"
                )
                get_filename_component(autoviz_dependency_soname "${autoviz_dependency}" NAME)
                get_filename_component(autoviz_dependency_realname "${autoviz_dependency_real}" NAME)
                if(NOT autoviz_dependency_soname STREQUAL autoviz_dependency_realname)
                    file(CREATE_LINK "${autoviz_dependency_realname}"
                        "${CMAKE_INSTALL_PREFIX}/lib/${autoviz_dependency_soname}"
                        SYMBOLIC COPY_ON_ERROR)
                endif()
            endif()
        endforeach()
        ]=])
    endif()
endif()
