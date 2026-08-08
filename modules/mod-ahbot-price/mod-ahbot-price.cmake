if(TARGET modules)
  # This module's own headers.
  target_include_directories(modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
endif()
