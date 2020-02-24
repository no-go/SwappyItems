#!/bin/bash

printf "\
set title outputFile
set terminal pdf
set output outputFile
set yrange[0:]
set xrange[0:]
set xlabel \"time (sec)\"
#set ylabel \"sachen\"
set boxwidth 0.2
set style fill solid 0.25 border -1
set grid

plot filename1 using 1:3 with lines title 'items', \
    filename1 using 1:5 with lines title 'queue', \
    filename1 using 1:(\$15 * 100) with lines title 'loads x 100', \
    filename1 using 1:7 with lines title 'updates', \
    filename1 using 1:13 with lines title 'error', \
    filename1 using 1:17 with lines title 'kB RAM'
" > /tmp/$0.plot

gnuplot -persist -e "outputFile='./plot.$1.pdf';filename1='../$1'" /tmp/$0.plot
