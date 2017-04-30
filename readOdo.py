from serial import Serial
from time import sleep
from struct import unpack
from math import pi


KM_FACTOR = 2.0 * pi * 24.5 / 28.0 / 100.0 / 1000.0
DELTA_FACTOR = 2.0 * pi * 24.5 / 28.0 / 100.0 / 1000.0 * 2.0 * 3600.0

def main():
	serialPort = Serial("COM4", 9600)
	while(True):
		serialPort.write('e')
		km = serialPort.read()
		km += serialPort.read()
		km += serialPort.read()
		km += serialPort.read()
		km = unpack('L',km)

		delta = serialPort.read()
		delta += serialPort.read()
		delta = unpack('H',delta)

		print 'km = %f' % (float(km[0]) * KM_FACTOR)
		print 'delta = %f' % (float(delta[0]) * DELTA_FACTOR)

		sleep(.5)


if __name__ == '__main__':
	main()
