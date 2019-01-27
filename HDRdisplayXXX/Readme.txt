HDRDisplay Quick Start guide

HDRDisplay is a simple SDK app for the purposes of testing and displaying HDR
images, It works with or without an HDR display. For more detailed information
please see the included pdf.

Command line arguments

HDRDisplay [options] <image list>

The image list can contain a collection of exr or hdr files. All will be
placed in the list of available images in the app.

options

  -w [number] - specify the window width
  -h [number] - specify the window height
  -fullscreen - run in fullscreen exclusive mode
  -display [number] - select the display device on the primary adapter
  -hdr - start the app with the TV in HDR mode (requires fullscreen)

Keys

  <escape> - exit the app
  <tab> - toggle UI display

== Application control panels ==

The two primary panels are expended by default
Setting panel

When starting the app, the setting panel is displayed at the upper left.
This panel has options for selecting the image displayed, selecting the
tone map operator, and controlling  splitscreen mode.

Tone map panel

This panel will change based on the tone mapper selected in the settings
panel. The name is always based on the selected tone mapper. In some cases,
the tone mapper may have no settings, so this panel may completely disappear.

Splitscreen Panel

This panel controls a special tone mapper that is applied to half of the
screen when the split screen operation is enabled. The default is an LDR
ACES tone map, and the controls for the tone map itself are similar to the
standard ACES tone map panel described later.

The splitscreen panel has one special option. IT allows you to recalibrate the
brightness of the splitscreen region to emulate a monitor being set to
different brightness levels. This really only works well with an HDR display.
The setting is named "Post Scale", and it is at the bottom of the panel. By
default, the SDR output sets the image at the sRGB refence 80 nits. Many
monitors can display up to 350 nits when set to maximum brightness. Setting
this parameter to 4.0 makes it as though you have a 320 nit monitor with
brightness at full. Setting it to 12.5 on an HDR TV demonstrates the result
of just stretching SDR content to the full HDR range.

Note that it may be useful to set this scale parameter less than 1.0 to
emulate the standard brightness of an SDR monitor when testing EDR.

Test Pattern Parameters

This panel allows you to select a test pattern and tweak brightness. It
is mostly there for when someone fails to provide an image on the command
line. The test pattern is always available under the images list.

Pre-Transform Color

This panel controls a shader pass that transforms the image before the
tone mapper. The first several options are fairly standard with full-frame,
linear filtered, and zoom.

The second block of options handles exposure and contrast. The exposure
setting is in photographic stops. Plus one stop means scaling the input image
by 2.0. The app presently lacks an autoexposure option, so exposure needs to
be applied beforehand, or adjusted in the application. The expansion power
option is a contrast adjustment. This allows you to stretch an image with low
dynamic range to a wider range. Values greater than 1.0 will expand, values
less than 1.0 will compress.

Finally, there are several color grading controls. They can be turned on and
off, as well as viewed on only half the screen. You probably should ignore the
IPT version for now. The RGB version has a bunch of tools to emulate a look
that an art team many have created via a LUT.

HDR Settings

This panel allows you to toggle the HDR enable on an HDR device. It also has
metadata settings, but those can be safely ignored at the moment. Remember that
the display will only support HDR when in fullscreen exclusive mode.

== ACES Tone Map Options ==

First, there are several preset buttons. The HDR and HDR sharpened buttons
really only make sense on HDR displays. The brighter regions of the screen
will just clip to white if you are running on an SDR display. The default
tone map setting is actually the HDR sharpened version, so when running on
an SDR display, you will see clipping.

  HDR Preset - ACES standard for 1000 nits
  HDR Sharpened - ACES 1000 nit sharpened to accentuate highlights
  SDR - standard setting
  EDR - setting appropriate for high-spec monitors
  EDR Extreme - setting for special display device

The EDR setting is useful for testing HDR content on a high-quality SDR monitor
that can generate a bright signal. To use this mode, run the app in fullscreen
and turn the brightness to the maximum. The image should appear fairly similar
to the standard SDR image with a normal brightness, but highlights should now
appear better, and colors should be somewhat richer. (note there are better
EDR settings than this)

=ACES settings=

Color Space - The color space to restrict the content to
  Generally, DCI-P3 for HDR and sRGB for SDR.

EOTF - this is the "gamma" function selector
  scRGB - nide we use for HDR TV output
  sRGB - the mode for the SDR transforms (packs whole range into 0-1)
  PQ - special output mode for special HDR displays

Tone Map Luminance - whether to use constant color tone mapping
  The tone mapper normally desaturates close to extreme highlights this reduces
  that effect

Saturation level - degree to use luminance or RGB

Cat D60 to D65 - conversion of white point
  Generally ignore this

Desaturate - enable/disable step in ACES path
  Generally, this is set to true only for SDR tone mappers

Alter Surround - adjust for the brightness of the viewing environment
  Tone mapper is designd for a reference dark environment
  Turning this on reduces output contrast, but compensates for viewer adjusting
  to brighter lighting environments

Surround Gamma - power used for the alteration of surround
  0.9811 is the standard for a dim environment
  0.8 seems reaosnable for a bright environment
  The lower you go, the more contrast you are losing, but you are compensating
  for a reduction in contrast sensitivity by the viewer

=Tone Curve Settings=

Tone Curve - selectable list of ODT curves for reference output brightness
  Adjustable ones obey the other parameters listed

Max Stops - input data level resulting in saturation at max brightness
  10 - 1000 nit default
  6.5 - SDR default

Max Level - Output of peak luminance in nits
  -1 - use default for curve
  1000 - 1000 nit default
  48 - SDR default (based on cinema, but everything is normalized to 0-1)

Middle Gray Scale - Scale factor applied to output middle gray level
  This parameter allows for the creation of EDR type tone mappers


== ACES LUT Tone map setting ==

This is essentially the same as the ACES settings, except that the tone map is
baked into a 3D texture. The range is compressed via a shaper function
(presently log), and it relies on linear filtering between sample positions.

== Linear Tone Map ==

There is nothing that interesting here. This is just a way to visualize the
image with no tone mapping. You do have a scale control and output color space
controls.

== Display Range == 

This option is particularly interesting for the purposes of understanding the
range and exposure level of an image. The visualization is based on
photographic stops. Cyan is placed at middle gray, and each 2 stops of change
transitions to a pure warmer or cooler color. Blue is 2 stops dimmer than
middle gray, and green is 2 stops brighter. Yellow, red, magenta, and white
continue the progression. Therefor, white means something is 10 stops above
middle gray, and is 1024 times the luminance of the middle gray.

You can use this for adjusting the exposure on an image. Generally, you will
target a scene to be fairly balanced with a lot of cyan and green. For some
things it may be desirable to go for low or high key, which will bias this in
one direction or the other.

== Reinhard ==

This demonstrates a standard Reinhard operator. That operator works in a 
paramtric 0-1 space. By setting the EOTF mode to scRGB, you allow it to
expand to the maximum range of an HDR display. The display level controls the
scale used here. 

This is mostly to demonstrate what Reinhard does on an HDR displacy, and how it
compares with filmic tone mappers.




