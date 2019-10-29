grainbuffer~
============

grainbuffer~ is a MSP object that exploits the buffer~ object built into the Max/MSP environment.

grainbuffer~ is a flexible tool that allows independent control over the buffer and the grain creation. For example, users can set the rate at which the grains read from the buffer independent of their dispersion or duration. Also, loop points in the buffer can be set, allowing the user to decide which portion of the buffer will be granulated. In addition, the buffer read point can be randomized. This randomization produces various effects ranging from a 'blurring' effect to that of total randomness of grain sources.

The lastest version of grainbuffer~ is 64-bit and now features multichannel output with up to 32 channels of spatialized grains.

All parameters can be randomized. Randomizable features include the frequency, amplitude, pan position, duration and dispersion (distance between grain start times). The randomness of each of these parameters is set by a range indicating an upper and a lower limit. If the limits are set the same then the grainbuffer~ will only generate single value for that parameter.

Finally, the user can select the type of envelope. Envelope types include, sine, linear, exponential, trapezoid, parabolic, percussive, evissucrep (backwards percussive), and, of course, random, which chooses a new grain envelope randomly for each individual grain.

//////////////
..::Change log::..

1/1/2014
Updated grainbuffer~ from original 32-bit version (Max 4.5 & Max 5) to 64-bit (Max 6) version.
Source has been migrated to Git Hub.

New features include:
- Multi-channel expansion: option for 1 - 32 channels of spatialized grains, which is specified after the buffer~ name. Uses a modified equal power pan logic, which may be suitable for ring-like speaker layouts. For multi-dimensional spatializtion (azimuth and elevation) 1 channel is recommended with an external spatialization scheme (such as ambisonics). Note on output channels: Default (no argument for output channels) is Stereo Max 4 Live Compatiblity mode, which is the same as if the argument provided = 2. This object will crash in M4L if a another value is provided for this argument. 

- Everything is updated to 64-bit resolution.

- The arbitrary limit on the grain pool was increased to allow for higher resolution grain densities. This coincides with processor performance improvements since the original creation of this object.

-----------------
10/1/2014

- updated mxe64 object and project files. Has been tested successfully on Windows 7.1 64-bit.
