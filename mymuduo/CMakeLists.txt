#定义cmake的使用版本，要>=3.5
cmake_minimum_required(VERSION 3.5)
#定义项目名
project(mymuduo)

#设置调试信息 和 要使用的c++标准，对于比较新的编译器，就需要加上-fPIC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# mymuduo最终编译成so动态库文件，设置动态库的路径，放在当前CMakeLists文件所在目录下的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#定义要参与编译的源代码文件
aux_source_directory(. SRC_LIST) # . 代表当前目录下的所有源文件，然后放进SRC_LIST变量里

#编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})  #mymuduo是自定义的动态库名，SHARED表示生成动态库，${SRC_LIST}表示要参与编译的源文件