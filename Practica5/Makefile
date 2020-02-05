obj-m =  mod5.o # mod5.c no tiene que existir
mod5-objs = modtimer.o modconfig.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean