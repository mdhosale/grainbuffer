grainbuffer~
============

grainbuffer~ is a MSP object that exploits the buffer~ object built into the Max/MSP environment.

grainbuffer~ is a flexible tool that allows independent control over the buffer and the grain creation. For example, users can set the rate at which the grains read from the buffer independent of their dispersion or duration. Also, loop points in the buffer can be set, allowing the user to decide which portion of the buffer will be granulated. In addition, the buffer read point can be randomized. This randomization produces various effects ranging from a 'blurring' effect to that of total randomness of grain sources.

The lastest version of grainbuffer~ is 64-bit and now features multichannel output with up to 32 channels of spatialized grains.

All parameters can be randomized. Randomizable features include the frequency, amplitude, pan position, duration and dispersion (distance between grain start times). The randomness of each of these parameters is set by a range indicating an upper and a lower limit. If the limits are set the same then the grainbuffer~ will only generate single value for that parameter.

Finally, the user can select the type of envelope. Envelope types include, sine, linear, exponential, trapezoid, parabolic, percussive, evissucrep (backwards percussive), and, of course, random, which chooses a new grain envelope randomly for each individual grain.
