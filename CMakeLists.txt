# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Cy 'kkm' K'Nelson

cmake_minimum_required(VERSION 3.10)

set(CMAKE_BUILD_TYPE_INIT "MinSizeRel")

project(tty_resize LANGUAGES C VERSION 1.3.51.1)
include(GNUInstallDirs)

add_compile_options(-Wall -Wextra)
add_executable(tty_resize tty_resize.c)
install(TARGETS tty_resize)
