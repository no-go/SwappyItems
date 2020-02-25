#!/bin/bash

printf "\
set title outputFile
set terminal pdf linewidth 1 size 20cm,18cm
set output outputFile
set yrange[0:2400]
set xrange[0:2000]
set xlabel \"time (sec)\"
#set ylabel \"sachen\"
set boxwidth 0.2
set style fill solid 0.25 border -1
set grid

plot filename1 using 1:(\$3/1000) with lines title 'k items', \
    filename1 using 1:(\$5/1000) with lines title 'k queue', \
    filename1 using 1:(\$7/1000) with lines title 'k updates', \
    filename1 using 1:(\$13/1000) with lines title 'k error', \
    filename1 using 1:15 with lines title 'swaps', \
    filename1 using 1:17 with lines title 'loads', \
    filename1 using 1:(\$19/1000) with lines title 'MB RAM'
" > /tmp/$0.plot

gnuplot -persist -e "outputFile='./plot.$1.pdf';filename1='$1'" /tmp/$0.plot
