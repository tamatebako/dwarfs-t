# Ensure mistletoe Python package is installed for manpage generation
# This module checks if mistletoe is available and installs it if needed

if(NOT PYTHON3_EXECUTABLE)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    set(PYTHON3_EXECUTABLE "${Python3_EXECUTABLE}")
  endif()
endif()

if(PYTHON3_EXECUTABLE)
  # Check if mistletoe is installed
  execute_process(
    COMMAND "${PYTHON3_EXECUTABLE}" -c "import mistletoe"
    RESULT_VARIABLE MISTLETOE_CHECK_RESULT
    OUTPUT_QUIET ERROR_QUIET
  )

  if(NOT MISTLETOE_CHECK_RESULT EQUAL 0)
    message(STATUS "mistletoe not found, attempting to install...")

    # Try to install mistletoe using pip
    execute_process(
      COMMAND "${PYTHON3_EXECUTABLE}" -m pip install --user mistletoe
      RESULT_VARIABLE MISTLETOE_INSTALL_RESULT
      OUTPUT_VARIABLE MISTLETOE_INSTALL_OUTPUT
      ERROR_VARIABLE MISTLETOE_INSTALL_ERROR
    )

    if(NOT MISTLETOE_INSTALL_RESULT EQUAL 0)
      # If pip module doesn't exist, try using system pip
      execute_process(
        COMMAND pip3 install --user mistletoe
        RESULT_VARIABLE MISTLETOE_INSTALL_RESULT2
        OUTPUT_QUIET ERROR_QUIET
      )

      if(NOT MISTLETOE_INSTALL_RESULT2 EQUAL 0)
        message(WARNING "Failed to install mistletoe. Manpage generation may fail.")
        message(WARNING "Please install manually: pip3 install mistletoe")
      else()
        message(STATUS "mistletoe installed successfully via pip3")
      endif()
    else()
      message(STATUS "mistletoe installed successfully")
    endif()

    # Verify installation
    execute_process(
      COMMAND "${PYTHON3_EXECUTABLE}" -c "import mistletoe"
      RESULT_VARIABLE MISTLETOE_VERIFY_RESULT
      OUTPUT_QUIET ERROR_QUIET
    )

    if(MISTLETOE_VERIFY_RESULT EQUAL 0)
      message(STATUS "✓ mistletoe is now available")
    else()
      message(WARNING "mistletoe installation verification failed")
    endif()
  else()
    message(STATUS "✓ mistletoe is available")
  endif()
else()
  message(WARNING "Python3 not found, cannot ensure mistletoe availability")
endif()