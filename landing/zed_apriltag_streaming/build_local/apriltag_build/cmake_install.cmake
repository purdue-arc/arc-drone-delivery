# Install script for directory: /home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/apriltag.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/apriltag_math.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/apriltag_pose.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/debug_print.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/doubles.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/doubles_floats_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/floats.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/g2d.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/getopt.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/homography.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/image_types.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/image_u8.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/image_u8_parallel.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/image_u8x3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/image_u8x4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/matd.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/math_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/pam.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/pjpeg.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/pnm.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/postscript_utils.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/pthreads_cross.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/string_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/svd22.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/time_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/timeprofile.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/unionfind.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/workerpool.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/zarray.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/zhash.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag/common" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/common/zmaxheap.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tag16h5.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tag25h9.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tag36h10.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tag36h11.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tagCircle21h7.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tagCircle49h12.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tagCustom48h12.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tagStandard41h12.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/apriltag" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build/_deps/apriltag-src/tagStandard52h13.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so.3.4.5"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so.3"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/libapriltag.so.3.4.5"
    "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/libapriltag.so.3"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so.3.4.5"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so.3"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/libapriltag.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libapriltag.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake/apriltagTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake/apriltagTargets.cmake"
         "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/CMakeFiles/Export/lib/apriltag/cmake/apriltagTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake/apriltagTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake/apriltagTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/CMakeFiles/Export/lib/apriltag/cmake/apriltagTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/CMakeFiles/Export/lib/apriltag/cmake/apriltagTargets-release.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/apriltag/cmake" TYPE FILE FILES
    "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/generated/apriltagConfig.cmake"
    "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/generated/apriltagConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/apriltag.pc")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/usr/local/lib/python3.10/site-packages" TYPE MODULE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/apriltag.cpython-310-x86_64-linux-gnu.so")
  if(EXISTS "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so"
         OLD_RPATH "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/usr/local/lib/python3.10/site-packages/apriltag.cpython-310-x86_64-linux-gnu.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/opencv_demo")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo"
         OLD_RPATH "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/opencv_demo")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build/apriltag_demo")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo"
         OLD_RPATH "/home/raghav/Documents/arc-drone-delivery/zed_apriltag_streaming/build_local/apriltag_build:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/apriltag_demo")
    endif()
  endif()
endif()

