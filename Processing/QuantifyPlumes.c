#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>

// Note: this only prints attributes for variables, not for groups
void printInfo(int id, int printattribs) {
	char name[NC_MAX_NAME];

	int numgroups;
	nc_inq_grps(id, &numgroups, NULL);
	int groupids[numgroups];
	nc_inq_grps(id, NULL, &groupids[0]);
	if (numgroups) printf("%d groups:\n", numgroups);
	for (int i = 0; i < numgroups; i++) {
		size_t nmelen;
		nc_inq_grpname(groupids[i], &name[0]);
		printf("@%d | %s\n", groupids[i], name);
		printInfo(groupids[i], printattribs);
	}

	int numdims, numvars, numattribs;
	nc_inq(id, &numdims, &numvars, &numattribs, NULL);
	
	if (numdims) printf("%d Dimensions:\n", numdims);
	int dimids[numdims];
	nc_inq_dimids(id, NULL, &dimids[0], 0);
	for (int i = 0; i < numdims; i++) {
		nc_inq_dimname(id, dimids[i], &name[0]);
		printf("\t@%d | %s\n", dimids[i], name);
	}

	if (numvars) printf("%d Variables:\n", numvars);
	int varids[numvars];
	nc_inq_varids(id, NULL, &varids[0]);
	for (int i = 0; i < numvars; i++) {
		nc_inq_varname(id, varids[i], &name[0]);
		printf("\t@%d | %s\n", varids[i], name);

		if (printattribs) {
			int numvarattribs;
			nc_inq_varnatts(id, varids[i], &numvarattribs);
			if (numvarattribs) printf("\t%d Attributes:\n", numvarattribs);
			for (int j = 0; j < numvarattribs; j++) {
				nc_inq_attname(id, varids[i], j, &name[0]);
				printf("\t\t@%d | %s\n", j, name);
			}
		}
	}

	printf("\n");
}

int main() {
	int ncid, numdims, numvars, numattribs;

	// TODO: check nc function success & handle errors
	nc_open(
		"/Users/danp/Desktop/CAUrbanRunoffPlumes/AquaMODIS/AQUA_MODIS.20101222T211501.L2.OC.nc", 
		NC_NOWRITE, 
		&ncid);
	
	printInfo(ncid, 0);

	nc_close(ncid);
	return 0;
}
