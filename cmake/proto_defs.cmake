function(add_cc_proto_library NAME)
  set(single)
  set(multi_args PROTOS INCS DEPS)
  cmake_parse_arguments(PARSE_ARGV 1 args "" "${single}" "${multi_args}")

  protobuf_generate(
    PROTOS ${args_PROTOS}
    LANGUAGE cpp
    OUT_VAR ${NAME}_var
  )

  add_library(${NAME}
    ${${NAME}_var}
  )

  target_link_libraries(${NAME}
    PUBLIC
      ${Protobuf_LIBRARIES}
      ${args_DEPS}
  )

  target_include_directories(${NAME}
    PUBLIC
      ${Protobuf_INCLUDE_DIRS}
      ${args_INCS}
      ${CMAKE_CURRENT_BINARY_DIR}
  )
endfunction()
