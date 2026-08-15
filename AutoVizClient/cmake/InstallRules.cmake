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
    if(TARGET Qt5::QXcbIntegrationPlugin AND
       TARGET Qt5::QOffscreenIntegrationPlugin AND
       TARGET Qt5::QSQLiteDriverPlugin)
        set(autoviz_qt5_plugin_files
            "$<TARGET_FILE:Qt5::QXcbIntegrationPlugin>"
            "$<TARGET_FILE:Qt5::QOffscreenIntegrationPlugin>"
            "$<TARGET_FILE:Qt5::QSQLiteDriverPlugin>"
        )
        set(autoviz_qt5_runtime_plugins ${autoviz_qt5_plugin_files})
        set(autoviz_qt5_library_dir "$<TARGET_FILE_DIR:Qt5::Core>")
    else()
        # Ubuntu's Qt5 CMake package can ship the plugins without importing
        # their targets.  Query the Qt selected by find_package(), rather than
        # assuming a system or /opt installation layout.
        get_target_property(autoviz_qt5_qmake Qt5::qmake IMPORTED_LOCATION)
        if(NOT autoviz_qt5_qmake)
            message(FATAL_ERROR "The selected Qt5 package does not provide qmake for plugin discovery")
        endif()

        execute_process(
            COMMAND "${autoviz_qt5_qmake}" -query QT_INSTALL_PLUGINS
            RESULT_VARIABLE autoviz_qt5_plugins_result
            OUTPUT_VARIABLE autoviz_qt5_plugins_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND "${autoviz_qt5_qmake}" -query QT_INSTALL_LIBS
            RESULT_VARIABLE autoviz_qt5_libs_result
            OUTPUT_VARIABLE autoviz_qt5_library_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT autoviz_qt5_plugins_result EQUAL 0 OR
           NOT autoviz_qt5_libs_result EQUAL 0)
            message(FATAL_ERROR "Unable to query plugin paths from the selected Qt5 qmake: ${autoviz_qt5_qmake}")
        endif()

        set(autoviz_qt5_plugin_files
            "${autoviz_qt5_plugins_dir}/platforms/libqxcb.so"
            "${autoviz_qt5_plugins_dir}/platforms/libqoffscreen.so"
            "${autoviz_qt5_plugins_dir}/sqldrivers/libqsqlite.so"
        )
        foreach(autoviz_qt5_plugin IN LISTS autoviz_qt5_plugin_files)
            if(NOT EXISTS "${autoviz_qt5_plugin}")
                message(FATAL_ERROR "The selected Qt5 installation is missing required plugin: ${autoviz_qt5_plugin}")
            endif()
        endforeach()
        set(autoviz_qt5_runtime_plugins ${autoviz_qt5_plugin_files})
    endif()

    list(GET autoviz_qt5_plugin_files 0 1 autoviz_qt5_platform_plugin_files)
    list(GET autoviz_qt5_plugin_files 2 autoviz_qt5_sql_plugin_file)
    install(FILES
        ${autoviz_qt5_platform_plugin_files}
        DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/platforms"
    )
    install(FILES
        "${autoviz_qt5_sql_plugin_file}"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/sqldrivers"
    )

        # Inspect the original plugin files: after they are copied below bin/,
        # their relative RPATH no longer resolves to the selected Qt tree and
        # CMake may instead find a system Qt with the same SONAME.
        install(CODE "set(autoviz_runtime_plugins [==[${autoviz_qt5_runtime_plugins}]==])\nset(autoviz_runtime_search_dirs [==[${autoviz_qt5_library_dir}]==])")
        install(CODE [=[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${CMAKE_INSTALL_PREFIX}/bin/AutoViz.bin"
            LIBRARIES ${autoviz_runtime_plugins}
            DIRECTORIES ${autoviz_runtime_search_dirs}
            RESOLVED_DEPENDENCIES_VAR autoviz_runtime_dependencies
            UNRESOLVED_DEPENDENCIES_VAR autoviz_unresolved_dependencies
        )
        foreach(autoviz_dependency IN LISTS autoviz_runtime_dependencies)
            if(autoviz_dependency MATCHES "/libQt5[^/]*\\.so" OR
               autoviz_dependency MATCHES "/libicu[^/]*\\.so" OR
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
