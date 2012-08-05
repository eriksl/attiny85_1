#!/bin/zsh

timeout=0.001
flushcount=2

testcase=2

exec 6<> /dev/ttyUSB1

function flush ()
{
	for ((flush = 0; flush < flushcount; flush++))
	do
		read -A -t $timeout reply <&6

		if [ -n "${reply[*]}" ]
		then
			if [ ${reply[1]} != "FD" ]
			then
				echo "${reply[*]}"
			fi
		fi
	done
}

function hex ()
{

}

case $testcase in
	1)
		while true
		do
			for io in 0 1 2 3 2 1
			do
				command="6${io}"

				echo "s 04 ${command} 01 p" >&6
				flush
				echo "s 05 04 p" >&6
				flush
				echo "s 04 ${command} 00 p" >&6
				flush
				echo "s 05 04 p" >&6
				flush
			done
		done
	;;
	2)
		echo "s 04 p" >&6
		flush

		while true
		do
			for ((pwm = 1; pwm < 128; pwm += 5))
			do
				printf "w 80 %02x p\n" $[((pwm + 0) % 128) / 1] >&6
				flush

				printf "w 81 %02x p\n" $[((pwm + 64) % 128) / 1] >&6
				flush

				printf "w 82 %02x p\n" $[((192 - pwm) % 128) / 1] >&6
				flush

				printf "w 83 %02x p\n" $[((128 - pwm) % 128) / 1] >&6
				flush

				usleep 20000
			done
		done
	;;
esac
