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
				command="8${io}"

				echo "s 04 ${command} 01 p" >&6
				flush
				echo "r 00 p" >&6
				flush
				echo "w ${command} 00 p" >&6
				flush
				echo "r 00 p" >&6
				flush
			done
		done
	;;
	2)
		echo "s 04 p" >&6
		flush

		while true
		do
			for ((pwm = 1; pwm < 1024; pwm += 50))
			do
				printf "w 80 %04x p\n" $[((pwm + 0) & 1023)] >&6
				flush

				printf "w 81 %04x p\n" $[((1024 - pwm) & 1023)] >&6
				flush

				printf "w 82 %04x p\n" $[((512 - pwm) & 1023)] >&6
				flush

				usleep 20000
			done
		done
	;;
esac
