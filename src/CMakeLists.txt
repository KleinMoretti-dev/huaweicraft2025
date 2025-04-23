cmake_minimum_required(VERSION 3.8)  # 不要修改
project(CodeCraft)                   # 不要修改

set(CMAKE_CXX_STANDARD      17)      # 不要修改
set(CMAKE_C_STANDARD        11)      # 不要修改

set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/) # 不要修改
option(BUILD_SHARED_LIBS    ""  OFF) # 不要修改

aux_source_directory(./                     cur_src) # 不要修改

# 如果需要，可以使用aux_source_directory增加目录

add_executable(code_craft                   ${cur_src}) # ！！！不要修改 code_craft 名称，直接影响结果；可以根据语法在 ${cur_src} 后面追加

# 以下可以根据需要增加需要链接的库
if (NOT WIN32)
   target_link_libraries(code_craft  pthread  rt  m)
endif (NOT WIN32)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 ")