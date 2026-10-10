# Strict warnings for Rhiza's own targets. Third-party code we compile lives in
# RhizaThirdParty with warnings off. CI makes warnings fatal by configuring
# with -DCMAKE_COMPILE_WARNING_AS_ERROR=ON.
function(rhiza_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
