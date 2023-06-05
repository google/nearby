all:
	gcc -DuECC_ENABLE_VLI_API=1 -DuECC_VLI_NATIVE_LITTLE_ENDIAN=1 -DuECC_SUPPORTS_secp160r1=1 -c -o uECC.o uECC.c
	ar rcs libmicro-ecc.a uECC.o
	cp libmicro-ecc.a ../../../out/gLinux/third_party/micro-ecc
 clean:
	rm -f libmicro-ecc.a