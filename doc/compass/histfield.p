#!/usr/bin/gnuplot
# Gnuplot script histfield.p to plot magnetic field
# Execute:  gnuplot histfield.p
#
set title "Magnetic field"
set xlabel "B[0.1uT]"

set style fill solid 0.5

binwidth = 1
binstart = 500
bin(x,width)=width*floor(x/width)

plot 'northcalib.txt' using (bin($3,binwidth)):(1.0) lc 1 smooth freq\
 with boxes title 'Bx',\
     '' using (bin($4,binwidth)):(1.0) lc 2 smooth freq with boxes title 'By',\
     '' using (bin($5,binwidth)):(1.0) lc 3 smooth freq with boxes title 'Bz'

#plot 'northcalib.txt' using (bin($6,binwidth)):(1.0) lc 1 smooth freq\
# with boxes

set term png
set output "northcalib.png"
plot 'northcalib.txt' using (bin($3,binwidth)):(1.0) lc 1 smooth freq\
 with boxes title 'Bx',\
     '' using (bin($4,binwidth)):(1.0) lc 2 smooth freq with boxes title 'By',\
     '' using (bin($5,binwidth)):(1.0) lc 3 smooth freq with boxes title 'Bz'



