echo "Building NCHelpers"
gcc NCHelpers.c -o build/NCHelpers.o -c -g

echo "Building GetData"
gcc GetData.c -o build/GetData.o -c -g
gcc -o GetData build/NCHelpers.o build/GetData.o -l netcdf -L/usr/local/lib -lcurl -g

echo "Building QuantifyPlumes"
gcc QuantifyPlumes.c -o build/QuantifyPlumes.o -c -g
gcc -o QuantifyPlumes build/NCHelpers.o build/QuantifyPlumes.o -l netcdf -L/usr/local/lib -lcurl -g
