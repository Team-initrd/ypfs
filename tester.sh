#!/bin/bash
# tests YPFS copy times
# format: ./tester.sh <src> <dest> <num>

#copy first then rename

if [[ -z "$1" || -z "$2" || -z "$3" ]]
then
 exit 1
fi

src=$1
dest=$2
num=$3

i=0
total=0

while [ "$i" -lt "$num" ]
do
 strt=`expr "\`date +%N\`" + 0`
 cp $src $dest$i
 end=`expr "\`date +%N\`" + 0`
 #let "strt /= 1000000"
 #let "end /= 1000000"
 let "total += end - start"
 #echo $total
 let "i += 1"
done

echo "total: $total"
let "avg = total / num"
echo "average: $avg"

exit 0
