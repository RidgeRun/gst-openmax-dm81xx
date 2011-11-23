#!/usr/bin/env python
# -*- Mode: Python -*-
# vi:si:et:sw=4:sts=4:ts=4
# ChangeLog:
# 2008-02-20 - Initial Version


import os
import gc
import sys
import time
import threading

import gobject

import gst
import gst.interfaces

loop = gobject.MainLoop()


# TODO add vidbin for video capture later
pipeline = gst.parse_launch('''
omx_camera name=cam vstab=false vnf=off output-buffers=6 image-output-buffers=1
  cam.src ! ( queue ! video/x-raw-yuv-strided, format=(fourcc)UYVY, width=640, height=480, framerate=30/1, buffer-count-requested=6 ! v4l2sink sync=false )
  cam.imgsrc ! ( name=imgbin queue ! image/jpeg, width=640, height=480 ! multifilesink location=test_imagecapture_%02d.jpg )
''')

i = 0
def on_timeout():
  global i
  if i == 0:
    print "switching to image mode"
    cam.set_property('mode', 'image')
    pipeline.add(imgbin)
    #cam.link_pads('imgsrc', imgbin, None)
    gobject.timeout_add(10000, on_timeout)
  elif i == 1:
    print "switching to preview mode"
    cam.set_property('mode', 'preview')
    gobject.timeout_add(10000, on_timeout)
  elif i == 2:
    print "finishing up"
    pipeline.set_state(gst.STATE_NULL)
  i = i + 1
  return False


def on_message(bus, message):
  global pipeline
  t = message.type
  if t == gst.MESSAGE_ERROR:
    err, debug = message.parse_error()
    print "Error: %s" % err, debug
  elif t == gst.MESSAGE_EOS:
    print "eos"
  elif t == gst.MESSAGE_STATE_CHANGED:
    oldstate, newstate, pending = message.parse_state_changed()
    elem = message.src
    print "State Changed: %s: %s --> %s" % (elem, oldstate.value_name, newstate.value_name)
    if elem == pipeline:
      if newstate == gst.STATE_PLAYING:
        print "State Change complete.. triggering next step"
        gobject.timeout_add(10000, on_timeout)


bus = pipeline.get_bus()
bus.enable_sync_message_emission()
bus.add_signal_watch()
bus.connect('message', on_message)


cam = None
imgbin = None
vidbin = None

for elem in pipeline:
  name = elem.get_name()
  if name.startswith('cam'):
    cam = elem
  elif name.startswith('imgbin'):
    imgbin = elem
  elif name.startswith('vidbin'):
    vidbin = elem

cam.set_property('mode', 'preview')
pipeline.remove(imgbin)

print "setting state to playing"
ret = pipeline.set_state(gst.STATE_PLAYING)
print "setting pipeline to PLAYING: %s" % ret.value_name

ret = imgbin.set_state(gst.STATE_PLAYING)
print "setting imgbin to PLAYING: %s" % ret.value_name

loop.run()

