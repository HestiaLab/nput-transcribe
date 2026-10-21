function(nput_target_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Werror
    )
endfunction()

function(nput_target_sanitizers target)
    if(NPUT_ENABLE_ASAN)
        target_compile_options(${target} PRIVATE -fsanitize=address)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
    if(NPUT_ENABLE_UBSAN)
        target_compile_options(${target} PRIVATE
            -fsanitize=undefined
            # 让未定义行为立即失败，而不是打印后继续。
            -fno-sanitize-recover=all
        )
        target_link_options(${target} PRIVATE -fsanitize=undefined)
    endif()
endfunction()

function(nput_target_defaults target)
    target_compile_features(${target} PRIVATE c_std_11)
    nput_target_warnings(${target})
    nput_target_sanitizers(${target})
endfunction()
