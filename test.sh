#!/bin/bash
#args grouplistFileName Timeout AYA SendFailure runtime

bool=0
counter=1
while read line
do
	for word in $line
	do
		if [ $bool == 0 ]
		then
			bool=1
		else
			echo $word
			`./node $word $1 $counter.txt $2 $3 $4` &
			counter=$(($counter+1))
			bool=0
		fi
	done
done < $1

sleep $5
kill `ps | pgrep node | awk '{print $1}'`
counter=$(($counter-1))

while [ $counter -ne "0" ]
do
	`cat $counter.txt >> log.txt`
	`rm $counter.txt`
	counter=$(($counter-1))
done
