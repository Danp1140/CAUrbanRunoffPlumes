#include "NCHelpers.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#define STUDY_AREA_RADIUS 0.1666666667
#define CUTOFF_RASTER_BBOX_MARGIN 0.01

#define DATE_PATH_FORMAT "%u/%u/%u"

#define L3SMI_FILEPATH_PREFIX "/Users/danp/Desktop/CAUrbanRunoffPlumes/SatelliteData/AquaMODIS/L3SMI/"
#define L3SMI_GEOGROUP_ID -1 // my L3SMI files have no groups, so geogroup is just the file id
#define L3SMI_LAT_ID 0
#define L3SMI_LON_ID 1

#define MYD09GA_FILEPATH_PREFIX "/Users/danp/Desktop/CAUrbanRunoffPlumes/SatelliteData/AquaMODIS/MYD09GA/"
#define MYD09GA_GEOGROUP_ID -1
#define MYD09GA_LAT_ID 0
#define MYD09GA_LON_ID 1

int* openNCFile(const char* prefix, uint16_t year, uint8_t month, uint8_t day, uint8_t* numfiles);

/*
 * Returns a heap-allocated array of memory coordinates at which the variable at varid
 * within the file/group at file is greater than cutoff. Note that current implementation
 * only works for float variables, as it calls nc_get_var1_float. Calculates this within a
 * circle with the given radius (in º) and center.
 */
MemCoord* calcCutoffRaster(
	const GeoLocNCFile* const file, 
	const int vargroup,
	const int varid, 
	float cutoff, 
	const GeoCoord* const center, 
	float radius,
	size_t* numpixels);

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("Wrong number of arguments (%d)\nCorrect usage: ./QuantifyPlumes <year> <month> <day>\n", argc);
		return 0;
	}

	const GeoCoord lariver = {33.755, -118.185},
			sgriver = {33.74, -118.115},
			sariver = {33.63, -117.96},
			bcreek = {33.96, -118.46};

	uint8_t numl3smifiles, nummyd09gafiles;
	int* l3smifiles = openNCFile(
			L3SMI_FILEPATH_PREFIX, 
			atoi(argv[1]), 
			atoi(argv[2]), 
			atoi(argv[3]), 
			&numl3smifiles),
		* myd09gafiles = openNCFile(
			MYD09GA_FILEPATH_PREFIX,
			atoi(argv[1]), 
			atoi(argv[2]), 
			atoi(argv[3]), 
			&nummyd09gafiles);

	GeoLocNCFile gll3smifiles[numl3smifiles];
	for (uint8_t i = 0; i < numl3smifiles; i++) {
		printInfo(l3smifiles[i], 0);
		gll3smifiles[i] = (GeoLocNCFile){l3smifiles[i], l3smifiles[i], L3SMI_LAT_ID, L3SMI_LON_ID};
	}
	GeoLocNCFile glmyd09gafiles[nummyd09gafiles];
	for (uint8_t i = 0; i < nummyd09gafiles; i++) {
		printInfo(myd09gafiles[i], 0);
		glmyd09gafiles[i] = (GeoLocNCFile){myd09gafiles[i], myd09gafiles[i], MYD09GA_LAT_ID, MYD09GA_LON_ID};
	}

	/*
	 * L3SMI files are gridded in such a way that lat & lon only have one dimension, presumably b/c 
	 * pixels are projected to have linearly indep lat/lon
	 */

	// MemCoord m = geoToMem((GeoCoord){33, -120}, &gll3smifiles[0]);

	size_t n;
	MemCoord* larivercr = calcCutoffRaster(&gll3smifiles[0], gll3smifiles[0].geogroupid, L3SMI_LON_ID, -119.5, &lariver, STUDY_AREA_RADIUS, &n);

	if (larivercr) free(larivercr);

	for (uint8_t i = 0; i < numl3smifiles; i++) {
		nc_close(l3smifiles[i]);
	}
	free(l3smifiles);
	for (uint8_t i = 0; i < nummyd09gafiles; i++) {
		nc_close(myd09gafiles[i]);
	}
	free(myd09gafiles);

	return 0;
}

int* openNCFile(const char* prefix, uint16_t year, uint8_t month, uint8_t day, uint8_t* numfiles) {
	char template[strlen(prefix) + strlen(DATE_PATH_FORMAT)];
	strcpy(&template[0], prefix);
	strcpy(&template[strlen(prefix)], DATE_PATH_FORMAT);
	// allocs dir of max length, some chars may not be used due to 1 digit days and months
	char dirpath[strlen(prefix) + 5 + 3 + 3];
	sprintf(dirpath, template, year, month, day);

	char* dirptr[2] = {&dirpath[0], NULL};
	FTS* dir = fts_open(dirptr, FTS_LOGICAL, 0);
	if (!dir) {
		printf("Dir didnt open (%s)\n", dirptr[0]);
		return NULL;
	}
	FTSENT* file;
	char** filepaths = (char**)malloc(1 * sizeof(char*));
	while ((file = fts_read(dir))) {
		if (file->fts_errno != 0) {
			if (file->fts_errno == 2) {
				printf("No such filepath (%s)\n", dirptr[0]);
			}
			printf("Unknown dir open error code %d (opening %s)\n", file->fts_errno, dirptr[0]);
		}
		else if (file->fts_level == 1) {
			if (*numfiles) { // realloc if this isn't the first file
				filepaths = (char**)realloc(filepaths, (*numfiles + 1) * sizeof(char*));
			}
			filepaths[*numfiles] = (char*)malloc(strlen(file->fts_accpath));
			strcpy(filepaths[*numfiles], file->fts_accpath);
			(*numfiles)++;
		}
	}
	fts_close(dir);

	int result;
	int* ids = (int*)malloc(*numfiles * sizeof(int));
	for (uint8_t i = 0; i < *numfiles; i++) {
		result = nc_open(filepaths[i], NC_NOWRITE, &ids[i]);
		if (result != NC_NOERR) {
			if (result == 2) printf("No such file (%s)\n", filepaths[i]);
			else printf("Unknown file open error code %d (opening %s)\n", result, filepaths[i]);
		}
		free(filepaths[i]);
	}
	free(filepaths);

	return ids;
}

MemCoord* calcCutoffRaster(
	const GeoLocNCFile* const file, 
	const int vargroup,
	const int varid, 
	float cutoff, 
	const GeoCoord* const center, 
	float radius,
	size_t* numpixels) {
	*numpixels = 0; 
	// to minimize geoToMem calls and allow iterating through memory, we'll iterate through
	// a square guarenteed to be larger than the study area, and only consider a pixel if
	// it's within the radius
	GeoCoord upperleft = *center;
	upperleft.lat += radius + CUTOFF_RASTER_BBOX_MARGIN;
	upperleft.lon -= radius + CUTOFF_RASTER_BBOX_MARGIN;
	GeoCoord lowerright = *center;
	lowerright.lat -= radius + CUTOFF_RASTER_BBOX_MARGIN;
	lowerright.lon += radius + CUTOFF_RASTER_BBOX_MARGIN;
	MemCoord extent = geoToMem(lowerright, file);
	MemCoord origin = geoToMem(upperleft, file);

	const size_t maxarea = (origin.x - extent.x) * (origin.y - extent.y);
	printf("max area %zu\n", maxarea);
	MemCoord* result = (MemCoord*)malloc(sizeof(MemCoord) * maxarea);

	// printf("o: {%zu, %zu}, e: {%zu, %zu}\n", temporigin.x, temporigin.y, extent.x, extent.y);
	GeoCoord gtemp;
	float datatemp;
	MemCoord current;
	for (current.y = origin.y; current.y > extent.y; current.y--) {
		for (current.x = origin.x; current.x > extent.x; current.x--) {
			gtemp = memToGeo(current, file);
			if (geoDist(&gtemp, center) < radius) {
				nc_get_var1_float(vargroup, varid, memData(&current), &datatemp);
				if (datatemp > cutoff) {
					printf("pixel found w/ value %f\n", datatemp);
					result[*numpixels] = current;
					(*numpixels)++;
					// ideally this check would never be needed, but its not much overhead
					if (*numpixels == maxarea) { 
						printf("numpixels %zu larger than areasize %zu\n", *numpixels, maxarea);
						return result;
					}
				}
			}
		}
	}
	if (*numpixels == 0) {
		free(result);
		return NULL;
	}
	result = realloc(result, *numpixels);
	return result;
}
