# Scripts for "A Biologically-Inspired Appearance Model for Snake Skin"

# ------------------------Mitsuba Installation-------------------------------------------------------------

# download the source files
git clone https://github.com/mitsuba-renderer/mitsuba

# go into the mitsuba folder
cd mitsuba

# copy the config file to build
cp build/config-linux-gcc.py config.py

# install all the necessary libraries
sudo apt-get install build-essential scons git libpng-dev libjpeg-dev libilmbase-dev libxerces-c-dev libboost-all-dev libopenexr-dev libglewmx-dev libxxf86vm-dev libeigen3-dev libfftw3-dev

# install collada support
sudo apt-get install libcollada-dom-dev

# if you want to use the GUI install QT5
sudo apt-get install qt5-default libqt5opengl5-dev libqt5xmlpatterns5-dev

# copy Snake skin plugin 
cp -a include/. mitsuba/include/
cp -a src/. mitsuba/src/

# -------------------------Compilation Issues-----------------------------------------------------------

# NB: If you have issues with the compilation, you can probably fix it by including the CXX flag '-fPIC' in the config.py file

# OpenGL: /usr/bin/ld: cannot find -lGL
apt install libgl1-mesa-glx --reinstall
ln -s /usr/lib/x86_64-linux-gnu/libGL.so.1 /usr/lib/x86_64-linux-gnu/libGL.so

#No package 'QtCore' found
cd /usr/lib/x86_64-linux-gnu/pkgconfig

sudo ln -s ./Qt5Core.pc ./QtCore.pc
sudo ln -s ./Qt5Gui.pc ./QtGui.pc
sudo ln -s ./Qt5Widgets.pc ./QtWidgets.pc
sudo ln -s ./Qt5OpenGL.pc ./QtOpenGL.pc
sudo ln -s ./Qt5Network.pc ./QtNetwork.pc
sudo ln -s ./Qt5Xml.pc ./QtXml.pc
sudo ln -s ./Qt5XmlPatterns.pc ./QtXmlPatterns.pc

# If you have issue with the python version and scons version
alias scons2="/usr/bin/env python2 $(which scons)"

# ------------------------Mitsuba Rendering-----------------------------------------------------------

# Python 2.7 environment for scons compilation
conda activate mitsuba

# start compiling and wait for "scons:Done building targets." --> around 2 mins
# QT_SELECT variable is only necessary if you want to use Mitsuba with the GUI
QT_SELECT=qt5 scons -j8

# update the paths
source setpath.sh

# render teaser scene
mitsuba /scenes/teaser/teaser.xml

# Obj to serialized
mtsimport ../scenes/feather_ball/geometry/feather_ball.obj feather_ball.xml

# EXR --> RGBE
mtsutil tonemap /scenes/teaser/teaser.xml

# ------------------------- Final Figures ---------------------------------------------------------------------

# Python environment for scripts and plots
conda activate raytracer2D

# Ablation studies for fiber BCSDF latlong  
time python ./scripts/measurements/ablation_fiber.py -i ./scenes/measurements/our_measurements_masking_fiber.xml -of ./scripts/measurements/fiber_ablation/ -n 1000 -t latlong