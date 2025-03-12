# A Biologically-Inspired Appearance Model for Snake Skin

## Authors

[Juan Raul Padron Griffe](https://juanraul8.github.io/),
[Diego Bielsa](https://github.com/DiegoBielsa),
[Adrian Jarabo](http://giga.cps.unizar.es/~ajarabo/),
[Adolfo Muñoz](http://webdiis.unizar.es/~amunoz/es/) <br>

## Overview

Simulating light transport in biological tissues is a longstanding challenge due to their complex multilayered structure. Among the most striking examples in nature are reptile scales, which exhibit a combination of pigmentation and photonic structures. While these optical effects have been extensively studied in biology, they remain largely overlooked in computer graphics. In this work, we propose a multilayered appearance model inspired by the anatomy of snake skin, particularly in species with highly iridescent scales. Our model represents snake skin as a two-layered reflectance function: a thin, specular top layer responsible for iridescence and a highly absorbing diffuse bottom layer that enhances the vibrancy of reflected colors. We demonstrate the versatility of our model across a range of appearances and show that it qualitatively replicates the characteristic look of iridescent snake scales.

Implementation of [A Biologically-Inspired Appearance Model for Snake Skin](https://graphics.unizar.es/projects/SnakeSkinAppearance_2023/) in Mitsuba 0.6.

## Installation

First clone the [Mitsuba](http://mitsuba-renderer.org/download.html) repository and compile it following its instructions. 

Go into the mitsuba folder:

``` cd mitsuba/ ```

Copy the config file to build:

``` cp build/config-linux-gcc.py config.py ```

You can set up a conda environment with all dependencies using the following command:

``` conda env create -f mitsuba_environment.yml ```

Activate anaconda environment for scons compilation:

``` conda activate mitsuba ```  

Compile mitsuba: 

``` scons -j8 ```

-j indicate the number of threads. The compilation usually takes around 2 minutes. For more details, you can check the -mitsuba_utils.sh- script or the Mitsuba documentation.

Then copy the following files into mitsuba folder, keeping the structure, then compile again.

``` cp -a snake_skin_plugin/include/. mitsuba/include/ ```  
``` cp -a snake_skin_plugin/src/. mitsuba/src/ ```  

Once the compilation is ready, update the paths:

``` source setpath.sh ```

## Rendering

First, download the [Mitsuba scenes](https://nas-graphics.unizar.es/s/SnakeSkinAppearance_2023) from our server.

Render the teaser scene:

``` mitsuba /scenes/teaser/teaser.xml ```

Transform the render from .exr to .png using the following command:

``` mtsutil tonemap scenes/teaser/teaser.exr ```

Reference images:

![Teaser render](https://github.com/juanraul8/SnakeSkin/blob/juanraul_dev/resources/teaser.png)

![Teaser render (grass)](https://github.com/juanraul8/SnakeSkin/blob/juanraul_dev/resources/teaser_grass.png)

Activate anaconda environment for plots and scripts:

``` conda activate raytracer2D ```  

Run the following script to create the renders of the Figure 5:

``` python ./scripts/appearance_range.py -width 1280 -height 720 -spp 1024 ```  

Run the following script to create the renders of the Figure 6:

``` python ./scripts/ablation_studies.py -width 1280 -height 720 -spp 1024 ```

## Ackowledgements

* Physically-based renderer: [Mitsuba 0.6](https://github.com/mitsuba-renderer/mitsuba)
* Modelling tool: [Blender](https://www.blender.org/)  
* Efficient integrator for layered materials: [Position-Free Monte Carlo Simulation](https://github.com/tflsguoyu/layeredbsdf)  
* Thin-film interference material: [Thin-film BSDF](https://belcour.github.io/blog/research/publication/2017/05/01/brdf-thin-film.html)  
* Initial project: [Diego Bielsa TFG](https://zaguan.unizar.es/record/126142?ln=en)

## Contact

If you have any questions, please feel free to email the authors.
