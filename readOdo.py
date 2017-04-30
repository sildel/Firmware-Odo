from serial import Serial
from time import sleep
from struct import unpack

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

		print 'km = %d' % km
		print 'delta = %d' % delta

		sleep(.5)


if __name__ == '__main__':
	main()
