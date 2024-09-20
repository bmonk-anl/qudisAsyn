#*******************************************************************************
#  Project:        quDIS Control Library
#
#  Filename:       makefile
#
#  Purpose:        GCC makefile for trivial example
#
#  Author:         N-Hands GmbH & Co KG
#*******************************************************************************
# $Id: makefile.lx,v 1.2 2019/02/06 14:50:23 trurl Exp $

example : example.o libqudis.so
	gcc -o example -L../lib example.o -lqudis

example.o : example.c
	gcc -c -g -Wall -std=gnu99 -O0 -I../inc -o example.o example.c

libqudis.so: ../lib/libqudis.so
	cp ../lib/libqudis.so .

clean:
	rm -f example example.o libqudis.so