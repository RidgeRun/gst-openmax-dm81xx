This is a read-me file for gst-openmax plugins
-------------------------------------------------

Note:
	The gst-openmax package on git@github.com:RidgeRun/gst-openmax-dm81xx.git
	repository is based on TI's gst-openmax and modified by RigeRun. This
	package is always updated according with the latest changes included in
	the TI package.
	
/***************/

Contents
1. Summary
2. What is gst-openmax?
3. Documentation
4. Installing gst-openmax plugins.
5. Building gst-openmax plugins
6. Compiling gst-openmax plugins
7. Change Log
8. If something goes wrong


1. Summary
========================================
  These are the release notes for gst-openmax plugins.  Read them carefully, 
  as they tell you what this is all about, explain how to build, compile
  and install the gst-openmax plugins, and what to do if something goes 
  wrong. 


2. What is gst-openmax?
========================================
  
  GstOpenMAX is a GStreamer plug-in that allows communication with OpenMAX
  IL components allowing you to take advantage of OpenMAX and all
  functions included in a multimedia framework as Gstreamer
  
  
  OpenMAX IL is an industry standard that provides an abstraction layer 
  for computer graphics, video, and sound routines.
  

3. Documentation
========================================

  RidgeRun gst-openmax package follows the same structure of TI gst-openmax
  package. The difference is on additional features that RidgeRun adds on 
  this package. Features that are included in this package are neccesary 
  for meet RidgeRun needs.
  
  General information:
	
	http://www.freedesktop.org/wiki/GstOpenMAX
	
	
4. Installation gst-openmax plugins
========================================
  
  First, you need to download or clone gst-openmax package, specifically 
  the RidgeRun's repository located in github
  
  Clone it with the following command:
	git clone git://github.com/RidgeRun/gst-openmax-dm81xx.git  
   
  or download on:
     https://github.com/RidgeRun/gst-openmax-dm81xx


5. Building gst-openmax plugins
========================================
 
  The build system used by the RidgeRun's version of gst-openmax is based
  on a shared library called libOMX, which is located in its own github 
  repository under the name libOMX_Core-DM81xx
  
  Clone repository with the following command:
	git clone git://github.com/RidgeRun/libOMX_Core-DM81xx.git
	
  or download:
	https://github.com/RidgeRun/libOMX_Core-DM81xx.git
  
  After it, you need to run the following commands on gst-openmax 
  directory:
  
	./autogen.sh
	./configure
	
  Also, you need to apply patches that are on patches directory. These 
  patches are modifications on GStreamer API. 
  
  In patches directory, you can find a README file that contains information 
  about functionality of patches as well as the GStreamer's version where 
  you need to apply them.
  

6. Compiling gst-openmax plugins
========================================

  First, you need to compile gst-openmax with:
	 make

  and then, in order to install them please run:
	 make install
  

7. Change Log
========================================
 
  This section shows and explain differences between RidgeRun's gst-openmax 
  package and TI's gst-openmax package.
 
  - Modifying build system, new build system is based in the use of a shared 
  library called libOmx.
  
  - Adding support for omx_camera.
  
  - Adding support for omx_tvp.
  
  - Adding support for decoder, 60fps by default.
 

8. If something goes wrong
========================================
  
  Write us at:
				inquires@ridgerun.com

