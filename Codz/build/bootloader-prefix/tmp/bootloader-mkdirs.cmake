# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/danym/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader"
  "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader-prefix"
  "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader-prefix/tmp"
  "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader-prefix/src"
  "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/danym/Git/Personal/ClockProject/clock_sucka_codz/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
