#!/usr/bin/perl -w
#
# Read mag3110d.log and extract magnetic field data from it.
# Usage: getfield.pl < file
#

use strict;

my $line="";

print "time                 Bx     By     Bz   B\n";

$line=<>;
while($line) 
{
    printf "%4d-%02d-%02d %02d:%02d:%02d   %-+5d %-+5d %-+5d %-5d\n",$1,$2,$3,$4,$5,$6,$7,$8,$9,$10 if($line =~ /(\d{4})-(\d{2})-(\d{2})\s(\d{2}):(\d{2}):(\d{2})\s+Bx=([+-]?\d{1,5})\s+By=([+-]?\d{1,5})\s+Bz=([+-]?\d{1,5})\s+B=(\d{1,5}).*/);

    $line=<>;
}


