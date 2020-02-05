#!/bin/bash
# Este script se limita a eliminar y mostrar los primeros 1000 numeros en la entrada /proc/modlist

lista="/proc/modlist"
tiempo=0.3

for (( i = 0; $i < 1000; ++i )); do
	echo remove $i > $lista
	cat $lista
	sleep $tiempo
done