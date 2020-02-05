#!/bin/bash
# Este script se limita a escribir los primeros 1000 numeros en la entrada /proc/modlist

lista="/proc/modlist"
tiempo=0.3

for (( i = 0; $i < 1000; ++i )); do
	echo add $i > $lista
	sleep $tiempo
done
