file(GLOB CPP_FILES
    src/*.cpp
)
add_library(vlcfg STATIC
  ${CPP_FILES}
)
target_include_directories(vlcfg PUBLIC
  include
)
