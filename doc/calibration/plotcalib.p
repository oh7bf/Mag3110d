# Gnuplot script plotcalib.p to plot magnetic field
# Execute:  gnuplot plotfield.p
#
set title "Magnetic field"
set xlabel "Angle[deg]"
set ylabel "B[0.1uT]"

set xrange [0:380]

# field data
plot "calib.txt" using 1:2 title 'Bx',\
     "calib.txt" using 1:3 title 'By',\
     "calib.txt" using 1:4 title 'Bz'

#set term png
#set output "data.png"
#plot "calib.txt" using 1:2 title 'Bx',\
#     "calib.txt" using 1:3 title 'By',\
#     "calib.txt" using 1:4 title 'Bz'



