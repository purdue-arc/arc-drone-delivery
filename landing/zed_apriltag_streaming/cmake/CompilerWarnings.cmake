function(zas_enable_warnings TARGET_NAME WARN_AS_ERROR)
  # Apply a consistent set of warning flags to the specified target.
  #
  # Example usage in CMakeLists.txt:
  #   include(cmake/CompilerWarnings.cmake)
  #   add_executable(myapp ...)
  #   zas_enable_warnings(myapp ${ZAS_WARNINGS_AS_ERRORS})
  if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
    if (WARN_AS_ERROR)
      target_compile_options(${TARGET_NAME} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${TARGET_NAME} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wsign-conversion
      -Wnon-virtual-dtor -Wold-style-cast
      -Woverloaded-virtual -Wnull-dereference
    )
    if (WARN_AS_ERROR)
      target_compile_options(${TARGET_NAME} PRIVATE -Werror)
    endif()
  endif()
endfunction()
