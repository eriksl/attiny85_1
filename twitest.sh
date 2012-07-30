#!/bin/zsh

timeout=0.001
flushcount=10

exec 6<> /dev/ttyUSB1

while true
do
	for io in 0 1 2 3 2 1
	do
		command="6${io}"

		echo "s 04 ${command} 01 p" >&6
		echo "s 05 04 p" >&6

		for ((flush = 0; flush < flushcount; flush++))
		do
			read -t $timeout reply <&6

			[ -n "$reply" ] && echo $reply
		done

		echo "s 04 ${command} 00 p" >&6
		echo "s 05 04 p" >&6

		for ((flush = 0; flush < flushcount; flush++))
		do
			read -t $timeout reply <&6

			[ -n "$reply" ] && echo $reply
		done
	done
done
