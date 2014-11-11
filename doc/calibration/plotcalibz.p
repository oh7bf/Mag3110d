# Gnuplot script plotcalib.p to plot magnetic field
# Execute:  gnuplot plotfield.p
#
set title "Magnetic field"
set xlabel "Angle[deg]"
set ylabel "Bz[0.1uT]"

set xrange [0:380]

# field data
plot "calib.txt" using 1:4 title 'Bz'

# sinusoidal fitting function
f(x)=a*sin(b*x+c)+d

# initial values for fitting
a=200;b=0.018;c=0.1;d=800
fit f(x) "calib.txt" using 1:4 via a, b, c, d

## plot "calib.txt" using 1:4 title 'Bz', f(x)

## set term png
## set output "calibfitBz.png"
## plot "calib.txt" using 1:4, f(x)


