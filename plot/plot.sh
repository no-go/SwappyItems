#!/bin/bash

printf "\
set title outputFile
set terminal pdf linewidth 1 size 27cm,18cm
set output outputFile
set yrange[0:2400]
set xrange[0:1000000]
set xlabel \"time (ms)\"
#set ylabel \"sachen\"
set boxwidth 0.2
set style fill solid 0.25 border -1
set grid

plot filename1 using 1:(\$3/1000) with lines title 'k items', \
    filename1 using 1:(\$5/1000) with lines title 'k queue', \
    filename1 using 1:(\$7/1000) with lines title 'k updates', \
    filename1 using 1:(\$15/1000) with lines title 'k error', \
    filename1 using 1:(\$17/10) with lines title '1/10 swaps', \
    filename1 using 1:19 with lines title '1/10 loads', \
    filename1 using 1:(\$21/1000) with lines title 'MB RAM', \
    filename1 using 1:(\$9/1000) with lines title 'k deletes'
" > /tmp/$0.plot

gnuplot -persist -e "outputFile='./plot.$1.pdf';filename1='$1'" /tmp/$0.plot
