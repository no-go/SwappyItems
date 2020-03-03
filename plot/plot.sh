#!/bin/bash

printf "\
set title outputFile
set terminal pdf linewidth 1 size 27cm,18cm
set output outputFile
set yrange[0:600]
set xrange[0:700000]
set xlabel \"time (ms)\"
#set ylabel \"sachen\"
set boxwidth 0.2
set style fill solid 0.25 border -1
set key left top
set grid

plot filename1 using 1:(\$3/1000) with lines title '1000 items', \
    filename1 using 1:(\$5/1000) with lines title '1000 queue size', \
    filename1 using 1:(\$7/1000) with lines title '1000 updates', \
    filename1 using 1:(\$9/1000) with lines title '1000 deletes', \
    filename1 using 1:(\$15/1000) with lines title '1000 key not found in file', \
    filename1 using 1:(\$17/10) with lines title '1/10 swap operations', \
    filename1 using 1:(\$19/100) with lines title '1/100 file loads', \
    filename1 using 1:(\$21/1000) with lines title 'MB RAM usage', \
    filename1 using 1:(\$23/1000) with lines title 'MB file usage'
" > /tmp/$0.plot

gnuplot -persist -e "outputFile='./plot.$1.pdf';filename1='$1'" /tmp/$0.plot
