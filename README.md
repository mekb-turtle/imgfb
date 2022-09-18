# imgfb
Draws a [farbfeld](http://tools.suckless.org/farbfeld/) or jpeg image to the Linux framebuffer

Usage: imgfb \<image file\>\
\<image file\> : path to the image file to read (or write if reversed) or - for stdin (or stdout), can be farbfeld, jpeg or png\
--framebuffer -f : framebuffer name, defaults to fb0\
--offsetx -x --offsety -y : offset of image to draw, defaults to 0 0\
--reverse -r : reads framebuffer and writes to a file (as farbfeld only) instead, offset does not work here
