# -*- mode: makefile -*-
# 
# Makefile for compiling 'mag3110d' on Raspberry Pi. 
#
# Mon Nov  3 21:18:58 CET 2014
# Edit: 
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: mag3110d 

mag3110d: mag3110d.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

