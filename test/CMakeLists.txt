enable_language(C)
file(GLOB files "*.cpp" "*.c")

foreach(file ${files})
    get_filename_component(binary_name ${file} NAME_WE)
    add_executable(${binary_name} ${file})

    if(file MATCHES "\\.cpp$")
        target_compile_features(${binary_name} PRIVATE cxx_std_14)
    endif()

    target_link_libraries(${binary_name} primesieve::primesieve)
    target_include_directories(${binary_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(${binary_name} PRIVATE ${PRIMESIEVE_COMPILE_DEFINITIONS})
    add_test(NAME ${binary_name} COMMAND ${binary_name})

    # Copy primesieve.dll to test directory.
    # On Windows the DLLs must be in the same directory
    # as the binaries that depend on them.
    if (WIN32 AND NOT STATICALLY_LINK_LIBPRIMESIEVE)
        add_custom_command(TARGET ${binary_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:libprimesieve>
                $<TARGET_FILE_DIR:${binary_name}>)
    endif()
endforeach()

if(BUILD_PRIMESIEVE)
    add_test(NAME maintain_load_options
             COMMAND ${CMAKE_COMMAND} -DPRIMESIEVE=$<TARGET_FILE:primesieve>
                     -DLINUX=$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/maintain_load.cmake)
    set_tests_properties(maintain_load_options PROPERTIES TIMEOUT 20)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME maintain_load_signal
                 COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/maintain_load_signal.sh
                         $<TARGET_FILE:primesieve>)
        set_tests_properties(maintain_load_signal PROPERTIES TIMEOUT 10)
    endif()
endif()
