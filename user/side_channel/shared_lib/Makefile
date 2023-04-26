
victim=victim
lib_name=nvleak_example
lib_file=${pmem_path}/lib${lib_name}.so
pmem_path=/mnt/pmem/

all: ${lib_file} ${victim}

${lib_file}: ${lib_name}.c
	gcc -c -Wall  -fpic $<
	gcc -shared -o lib${lib_name}.so ${lib_name}.o
	mv lib${lib_name}.so $@

${victim}: ${victim}.c ${lib_file}
	gcc -L${pmem_path} -Wall -O0 -o $@ $< -l${lib_name}
