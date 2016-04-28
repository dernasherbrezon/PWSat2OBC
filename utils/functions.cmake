function(target_generate_hex TARGET)
  set (EXEC_OBJ ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET})
  set (HEX_OBJ ${EXEC_OBJ}.hex)

  add_custom_command(OUTPUT ${HEX_OBJ}
      COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXEC_OBJ} ${HEX_OBJ}
      DEPENDS ${TARGET}
  )

  add_custom_target (${TARGET}.hex ALL DEPENDS ${HEX_OBJ})
endfunction(target_generate_hex)

function(target_jlink_flash TARGET COMMAND_FILE)
  add_custom_target(${TARGET}.flash
    COMMAND ${JLINK} -device ${DEVICE} -ExitOnError -CommanderScript ${COMMAND_FILE}
    DEPENDS ${TARGET}.hex
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  )

endfunction(target_jlink_flash)

function(target_require_semihosting TARGET)
  get_property(flags TARGET ${TARGET} PROPERTY LINK_FLAGS)
  set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "${flags} -specs=rdimon.specs")
endfunction(target_require_semihosting)

function(target_support_semihosting TARGET)
  if(SEMIHOSTING)
    target_require_semihosting(${TARGET})
  endif()
endfunction(target_support_semihosting)

function(target_format_sources TARGET SOURCES)
    if(CLANG_FORMAT)
        add_custom_target(${NAME}.format
            COMMAND ${CLANG_FORMAT} -style=file -i ${SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Formatting files: '${SOURCES}'"
            )
    else(CLANG_FORMAT)
        add_custom_target(${NAME}.format
            COMMAND ""
            COMMENT "Clang format has not been found. Code formatting is not available. "
            )
    endif()
endfunction(target_format_sources)

