# Gnuplot script plotfield.p to plot magnetic field
# Execute:  gnuplot plotfield.p
#
set title "Magnetic field"
set xdata time
set timefmt "%Y-%m-%d %H:%M:%S"
set format x "%m-%d\n%H:%M"
set xlabel "Date"
set ylabel "B[0.1uT]"

# set correct time range here, otherwise nothing is plotted
set xrange ["2014-11-10 20:50":"2014-11-10 21:20"]

# field data
plot "data.txt" using 1:3 title 'Bx'

#set term png
#set output "data.png"
#plot "data.txt" using 1:3 title 'Bx'



