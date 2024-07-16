#include "NCHelpers.h"

int main() {
	const char* aquamodiscloudurl = "https://ladsweb.modaps.eosdis.nasa.gov/opendap/hyrax/RemoteResources/laads/allData/61/MYDATML2/2002/185/MYDATML2.A2002185.0000.061.2018003215057.hdf";
	const char* queries[3] = {"Latitude", "Longitude", "Cloud_Mask"};
	GeoCoord min = {0, 0}, max = {90, 90};
	// getData(aquamodiscloudurl, 3, &queries[0], min, max);
	size_t numurls;
	char** urls = formulateMYDATML2URLs(2003, 11, 7, &numurls);
	clock_t boundscheckstart = clock();
	for (size_t i = 0; i < numurls; i++) {
		inBounds(urls[i], min, max);
	}
	printf("day of bounds checking took %fs\n", (float)(clock() - boundscheckstart) / (float)CLOCKS_PER_SEC);
	// inBounds(aquamodiscloudurl, min, max);
	return 0;
}
