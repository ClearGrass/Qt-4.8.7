#!/bin/sh

./configure -prefix  /home/leo/Qt/Qt-4.8.7-R8 -opensource -confirm-license -release -shared -embedded  arm -xplatform linux-arm-gnueabi-g++ -no-qt3support -fast -no-largefile -qt-mouse-tslib -no-opengl -no-openvg -make  tools  -nomake  demos -nomake  examples -nomake  docs -qt-libjpeg -qt-libpng -qt-zlib -qt-sql-sqlite -qt-gfx-transformed -qt-libtiff -multimedia -no-svg -no-cups -v -I ~/myGit/tina/build_dir/target-arm_cortex-a8+neon_uClibc-0.9.33.2_eabi/openssl-1.0.2d/include -L ~/myGit/tina/build_dir/target-arm_cortex-a8+neon_uClibc-0.9.33.2_eabi/openssl-1.0.2d
