# Gnuplot script plotcalib.p to plot magnetic field
# Execute:  gnuplot plotfield.p
#
set title "Magnetic field"
set xlabel "Angle[deg]"
set ylabel "Bx[0.1uT]"

set xrange [0:380]

# field data
plot "calib.txt" using 1:2 title 'Bx'

# sinusoidal fitting function
f(x)=a*sin(b*x+c)+d

# initial values for fitting
a=200;b=0.018;c=0.2;d=-600
fit f(x) "calib.txt" using 1:2 via a, b, c, d

## plot "calib.txt" using 1:2 title 'Bx', f(x)

## set term png
## set output "calibfitBx.png"
## plot "calib.txt" using 1:2, f(x)


