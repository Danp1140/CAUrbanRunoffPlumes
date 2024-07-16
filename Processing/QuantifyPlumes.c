#include "NCHelpers.h"

int main() {
	int ncid, numdims, numvars, numattribs;

	// TODO: check nc function success & handle errors
	nc_open(
		"/Users/danp/Desktop/CAUrbanRunoffPlumes/AquaMODIS/AQUA_MODIS.20101222T211501.L2.OC.nc", 
		NC_NOWRITE, 
		&ncid);
	printInfo(ncid, 0);


	GeoCoord eg = {-120, 30};
	MemCoord m = geoToMem(eg, 65540, 0, 1);
	printf("{%d, %d}\n", (int)m.x, (int)m.y);

	nc_close(ncid);
	return 0;
}
