PWM based signal generator for Arduino

It uses a PWM frequency of 20kHz, the duty is adjusted from 0 to 80%, it cannot reach 100% since some time is needed at the end of the interruption for processing. It autoadjust custom signals that range from 0 to 100

It outputs the signals on pins A0 and A1.

You can send the following commands through Serial (9600 bauds)

0 -> it stops the emission

1 [signalType] [fr] -> starts emitting the signal type 0=square, 1=sin, 2=trian, 3=sawPositive, 4=sawNeg, 5=customDefined at the specified frequency which can have decimals. For example: 1 1 50.54 for emitting a sinusoidal of frequency 50.54Hz

2 val1 .. val256 -> defined the custom signal with 256 values ranging from 0 to 100