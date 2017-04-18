#!/bin/sh

./configure -prefix  /home/leo/Qt/Qt-4.8.7-R8-opengles -opensource -confirm-license -release -shared -embedded  arm -xplatform linux-arm-gnueabi-g++ -no-qt3support -fast -no-webkit -no-largefile -qt-mouse-tslib -no-opengl -no-opengl -make  tools  -nomake  demos -nomake  examples -nomake  docs -qt-libjpeg -qt-libpng -qt-zlib -qt-sql-sqlite -qt-gfx-transformed -qt-libtiff -multimedia -no-svg -no-cups -v
