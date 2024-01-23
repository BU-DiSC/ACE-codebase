#!/bin/sh

b=20000
f="testfile"
x=2000000
e=50
s=90
d=10
r=1
w=4
k=8

# workload 1
for a in 1 2 3 4 5
do
	./buffermanager -b $b -f $f -x $x -e $e -s $s -d $d -r $r -w $w -k $k -a $a -W 3 >> workload1.txt
	sleep 5
done

sleep 10
# workload 2
e=10

for a in 1 2 3 4 5
do
	./buffermanager -b $b -f $f -x $x -e $e -s $s -d $d -r $r -w $w -k $k -a $a -W 3 >> workload2.txt
	sleep 5
done

sleep 10
# workload 3
e=90

for a in 1 2 3 4 5
do
	./buffermanager -b $b -f $f -x $x -e $e -s $s -d $d -r $r -w $w -k $k -a $a -W 3 >> workload3.txt
	sleep 5
done

sleep 10
# workload 4
e=50
s=50
d=50

for a in 1 2 3 4 5
do
	./buffermanager -b $b -f $f -x $x -e $e -s $s -d $d -r $r -w $w -k $k -a $a -W 3 >> workload4.txt
	sleep 5
done
