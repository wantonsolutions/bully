#!/bin/bash
#args grouplistFileName Timeout AYA SendFailure

bool=0
while read line
do
	for word in $line
	do
		if [ $bool == 0 ]
		then
			bool=1
		else
			echo $word
			`../node $word $1 $word.txt $2 $3 $4` &
			bool=0
		fi
	done
done < $1

