function(nput_target_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
    )
endfunction()

function(nput_target_sanitizers target)
    if(NPUT_ENABLE_ASAN)
        target_compile_options(${target} PRIVATE -fsanitize=address)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
endfunction()

function(nput_target_defaults target)
    target_compile_features(${target} PRIVATE c_std_11)
    nput_target_warnings(${target})
    nput_target_sanitizers(${target})
endfunction()
