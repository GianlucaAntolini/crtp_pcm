PROGRAMS = main \
		   monitor

FLAGS = -pthread

all: $(PROGRAMS)

main: main.c
	cc $(FLAGS) $^ -o $@

monitor: monitor.c
	cc $(FLAGS) $^ -o $@

rm:
	rm -f $(PROGRAMS)