#include "NCHelpers.h"

int main() {
	int ncid, numdims, numvars, numattribs;

	// TODO: check nc function success & handle errors
	nc_open(
		"/Users/danp/Desktop/CAUrbanRunoffPlumes/AquaMODIS/AQUA_MODIS.20101222T211501.L2.OC.nc", 
		NC_NOWRITE, 
		&ncid);
	printInfo(ncid, 0);


	GeoCoord eg = {30, -120};
	clock_t t = clock();
	MemCoord m = geoToMem(eg, 65540, 1, 0);
	printf("took %fs\n", (float)(clock() - t) / (float)CLOCKS_PER_SEC);
	printf("{%d, %d}\n", (int)m.x, (int)m.y);

	GeoCoord g = memToGeo(m, 65540, 1, 0);
	printf("sanity check: that leads back to GC {%f, %f}\n", g.lat, g.lon);

	nc_close(ncid);
	return 0;
}
