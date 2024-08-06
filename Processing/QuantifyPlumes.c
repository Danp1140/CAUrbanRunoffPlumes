#include "NCHelpers.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

// #define STUDY_AREA_RADIUS 0.1666666667
#define STUDY_AREA_RADIUS 0.333333333333
#define CUTOFF_RASTER_BBOX_MARGIN 0.01

#define DATE_PATH_FORMAT "%u/%u/%u"

#define MYDATML2_FILEPATH_PREFIX "/Users/danp/Desktop/CAUrbanRunoffPlumes/SatelliteData/AquaMODIS/MYDATML2/"
#define MYDATML2_GEOGROUP_ID -1
#define MYDATML2_LAT_ID 1
#define MYDATML2_LON_ID 2
#define MYDATML2_CLOUDMASK_ID 0

#define L3SMI_FILEPATH_PREFIX "/Users/danp/Desktop/CAUrbanRunoffPlumes/SatelliteData/AquaMODIS/L3SMI/"
#define L3SMI_GEOGROUP_ID -1 // my L3SMI files have no groups, so geogroup is just the file id
#define L3SMI_LAT_ID 0
#define L3SMI_LON_ID 1

#define MYD09GA_FILEPATH_PREFIX "/Users/danp/Desktop/CAUrbanRunoffPlumes/SatelliteData/AquaMODIS/MYD09GA/"
#define MYD09GA_GEOGROUP_ID -1
#define MYD09GA_LAT_ID 1
#define MYD09GA_LON_ID 2
#define MYD09GA_RRS_ID 0

typedef enum DataSet {
	MYDATML2,
	L3SMI,
	MYD09GA
} DataSet;

int* openNCFile(const char* prefix, uint16_t year, uint8_t month, uint8_t day, uint8_t* numfiles);

void initGLFiles(GeoLocNCFile* dst, int* files, const uint8_t numfiles, DataSet d);

void initCloudMaps(BakedGeoLocNCFile* dst, GeoLocNCFile* files, const uint8_t numfiles);

/*
 * Returns a heap-allocated array of memory coordinates at which the variable at varid
 * within the file/group at file is greater than cutoff. Note that current implementation
 * only works for float variables, as it calls nc_get_var1_float. Calculates this within a
 * circle with the given radius (in º) and center.
 * 
 * TODO: remove radius arg, we're just gonna use the macro for every dataset (as writePlumeRaster does)
 */
MemCoord* calcCutoffRaster(
	const GeoLocNCFile* const file, 
	const GeoLocNCFile* const clouds, 
	const size_t numcloudfiles,
	const void* cutoff,  // to allow for different var types. assumed to be the exact size of the type of var @ varid
	const GeoCoord* const center, 
	const float radius,
	size_t* numpixels);

void writePlumeRaster(
	MemCoord *const raster, 
	const size_t numpixels, 
	MemCoord origin,
	const MemCoord extent,
	const char* filepath,
	const GeoLocNCFile *const src); 

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("Wrong number of arguments (%d)\nCorrect usage: ./QuantifyPlumes <year> <month> <day>\n", argc);
		return 0;
	}

	const GeoCoord lariver = {33.755, -118.185},
			sgriver = {33.74, -118.115},
			sariver = {33.63, -117.96},
			bcreek = {33.96, -118.46};

	uint8_t numatml2files, numl3smifiles, nummyd09gafiles;
	int* l3smifiles = openNCFile(
			L3SMI_FILEPATH_PREFIX, 
			atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 
			&numl3smifiles),
		* myd09gafiles = openNCFile(
			MYD09GA_FILEPATH_PREFIX,
			atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 
			&nummyd09gafiles),
		* atml2files = openNCFile(
			MYDATML2_FILEPATH_PREFIX,
			atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 
			&numatml2files);


	GeoLocNCFile gll3smifiles[numl3smifiles];
	for (uint8_t i = 0; i < numl3smifiles; i++) {
		printInfo(l3smifiles[i], 0);
		gll3smifiles[i] = (GeoLocNCFile){l3smifiles[i], l3smifiles[i], L3SMI_LAT_ID, L3SMI_LON_ID};
	}
	GeoLocNCFile glmyd09gafiles[nummyd09gafiles];
	initGLFiles(&glmyd09gafiles[0], &myd09gafiles[0], nummyd09gafiles, MYD09GA);
	for (uint8_t i = 0; i < nummyd09gafiles; i++) {
		printInfo(myd09gafiles[i], 0);
	}
	GeoLocNCFile glatml2files[numatml2files];
	initGLFiles(&glatml2files[0], &atml2files[0], numatml2files, MYDATML2);

	BakedGeoLocNCFile cloudmaps[1];
	initCloudMaps(&cloudmaps[0], &glatml2files[0], 1);

	printInfo(atml2files[0], 0);


	size_t n;
	for (uint8_t i = 0; i < nummyd09gafiles; i++) {
		short cutoff = 5000;
		MemCoord* larivercr = calcCutoffRaster(&glmyd09gafiles[i], &glatml2files[0], numatml2files, (void*)(&cutoff), &lariver, STUDY_AREA_RADIUS, &n);
		printf("plume mask %zu pixels large\n", n);
		if (larivercr) free(larivercr);
	}


	for (uint8_t i = 0; i < numatml2files; i++) {
		nc_close(atml2files[i]);
	}
	free(atml2files);
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
		printf("%s\n", filepaths[i]);
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

void initGLFiles(GeoLocNCFile* dst, int* files, const uint8_t numfiles, DataSet d) {
	for (uint8_t i = 0; i < numfiles; i++) {
		switch (d) {
			case MYDATML2:
				dst[i] = (GeoLocNCFile){files[i], files[i], MYDATML2_LAT_ID, MYDATML2_LON_ID, MYDATML2_CLOUDMASK_ID, 1000, {0, 0}};
				break;
			case L3SMI:
				dst[i] = (GeoLocNCFile){files[i], files[i], L3SMI_LAT_ID, L3SMI_LON_ID, 2, 4638, {0, 0}};
				break;
			case MYD09GA:
				dst[i] = (GeoLocNCFile){files[i], files[i], MYD09GA_LAT_ID, MYD09GA_LON_ID, MYD09GA_RRS_ID, 500, {0, 0}};
				break;
		}
		int dimids[2];
		nc_inq_vardimid(dst[i].fileid, dst[i].latvarid, &dimids[0]); // presuming lat & lon both have 2 dimensions and they're the same
									     // if we need to run this on L3SMI, gonna need to do some checking
		nc_inq_dimlen(dst[i].fileid, dimids[0], &dst[i].bounds.x);
		nc_inq_dimlen(dst[i].fileid, dimids[1], &dst[i].bounds.y);
	}
}

void initCloudMaps(BakedGeoLocNCFile* dst, GeoLocNCFile* files, const uint8_t numfiles) {
	MemCoord m;
	GeoCoord g;
	for (uint8_t i = 0; i < numfiles; i++) {
		initBakedFile(&dst[i]);
		for (m.y = 0; m.y < files[i].bounds.y; m.y++) {
			for (m.x = 0; m.x < files[i].bounds.x; m.x++) {
				g = memToGeo(m, &files[i]);
				printf("hash: %zu\n", hash(&g));
			}
		}
	}
}

MemCoord* calcCutoffRaster(
	const GeoLocNCFile* const file, 
	const GeoLocNCFile* const clouds, 
	const size_t numcloudfiles,
	const void* cutoff, 
	const GeoCoord* const center, 
	const float radius,
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

	/*
	 * Given a pixel size (in m^2), we can actually calculate a radius in pixels (meters) rather than lat/lon (deg)
	 */

	GeoCoord lowerbound = memToGeo((MemCoord){0, 0}, file), 
		 upperbound = memToGeo((MemCoord){file->bounds.y - 1, file->bounds.x - 1}, file);
	if (lowerbound.lat > upperbound.lat) {
		float temp = lowerbound.lat;
		lowerbound.lat = upperbound.lat;
		upperbound.lat = temp;
	}
	if (lowerbound.lon > upperbound.lon) {
		float temp = lowerbound.lon;
		lowerbound.lon = upperbound.lon;
		upperbound.lon = temp;
	}
	if (upperleft.lat > upperbound.lat) upperleft.lat = upperbound.lat;
	if (upperleft.lon < lowerbound.lon) upperleft.lon = lowerbound.lon;
	if (upperleft.lat < lowerbound.lat) upperleft.lat = lowerbound.lat;
	if (upperleft.lon > upperbound.lon) upperleft.lon = upperbound.lon;
	if (lowerright.lat < lowerbound.lat) lowerright.lat = lowerbound.lat;
	if (lowerright.lon > upperbound.lon) lowerright.lon = upperbound.lon;
	if (lowerright.lat > upperbound.lat) lowerright.lat = upperbound.lat;
	if (lowerright.lon < lowerbound.lon) lowerright.lon = lowerbound.lon;
	if (upperleft.lat == lowerright.lat || upperleft.lon == lowerright.lon) {
		printf("study radius completely outside satellite image\n");
		return NULL;
	}

	MemCoord extent = geoToMem(lowerright, file);
	MemCoord origin = geoToMem(upperleft, file);
	if (extent.x == -1u || origin.y == -1u) { // out-of-range extent or origin
						  // technically this doesn't make full use of our data, we can
						  // come back to this later, instead locking to min/max
		printf("study radius completely outside satellite image\n");
		return NULL;
	}

	// we do a little swapping to make sure they're where we expect them to be in memory
	// since they're definitely extreme bounds, we're really just making sure we can
	// iterate predicatably
	if (extent.x < origin.x) {
		size_t temp = extent.x;
		extent.x = origin.x;
		origin.x = temp;
	}
	if (extent.y < origin.y) {
		size_t temp = extent.y;
		extent.y = origin.y;
		origin.y = temp;
	}
	size_t maxarea = (extent.x - origin.x) * (extent.y - origin.y);
	printf("max area %zu\n", maxarea);
	MemCoord* result = (MemCoord*)malloc(sizeof(MemCoord) * maxarea);

	GeoCoord gtemp;
	MemCoord current, mtemp;
	nc_type type;
	nc_inq_vartype(file->fileid, file->keyvarid, &type);
	int store;
	char datatemp[4]; // facilitates moving up to 32 bytes of data
	unsigned char cloudbyte;
	size_t cloudcounter;
	int clear;
	struct CloudMemCoord {size_t byte; MemCoord m;} cloudmcoord;
	cloudmcoord.byte = 0;
	for (current.y = origin.y; current.y < extent.y; current.y++) {
		for (current.x = origin.x; current.x < extent.x; current.x++) {
			clear = 0;
			for (cloudcounter = 0; cloudcounter < numcloudfiles; cloudcounter++) {
				gtemp = memToGeo(current, file);
				// cloudmcoord.m = geoToMem(gtemp, &clouds[cloudcounter]);
				cloudmcoord.m = (MemCoord){0, 0}; 
				if (cloudmcoord.m.x == -1u || cloudmcoord.m.y == -1u) {
					continue;
				}
				nc_get_var1_ubyte(clouds[cloudcounter].fileid, clouds[cloudcounter].keyvarid, &cloudmcoord.byte, &cloudbyte);
				/*
				printf("%zu: %c%c%c\n", cloudcounter,
						cloudbyte & 0b0100 ? '1' : '0',
						cloudbyte & 0b0010 ? '1' : '0',
						cloudbyte & 0b0001 ? '1' : '0');
						*/
				// TODO: replace with bitmask macros
				// second check is true if probably, or certainly clear, but not if cloudy or uncertain
				// uncertain is actually pretty common 
				if (cloudbyte & 0b00000001 && cloudbyte & 0b00000100) {
					clear = 1;
				}
				else clear = 0;
			}
			if (clear == 0) {
				// printf("pixel obstructed by cloud\n");
				continue;
			}
			gtemp = memToGeo(current, file);
			if (geoDist(&gtemp, center) < radius) {
				store = 0;
				if (type == NC_FLOAT) {
					nc_get_var1_float(file->fileid, file->keyvarid, memData(&current), (float*)(&datatemp[0]));
					if (*((float*)(&datatemp[0])) > (*((float*)(cutoff)))) {
						store = 1;
						// printf("pixel found at {%zu, %zu} w/ value %f\n", current.x, current.y, *((float*)datatemp));
					}
				} else if (type == NC_SHORT) {
					nc_get_var1_short(file->fileid, file->keyvarid, memData(&current), (short*)(&datatemp[0]));
					if (*((short*)(&datatemp[0])) > (*((short*)(cutoff)))) {
						store = 1;
						// printf("pixel found at {%zu, %zu} w/ value %d\n", current.x, current.y, *((short*)datatemp));
					}
				}
				if (store) {
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
	result = realloc(result, sizeof(MemCoord) * *numpixels);

	// troubleshooting filewrite, should not write every analysis file i dont think
	char* template = "/Users/danp/Desktop/CAUrbanRunoffPlumes/Processing/test%06d.nc4";
	char filepath[strlen(template) + 2];
	sprintf(&filepath[0], template, file->fileid);
	writePlumeRaster(result, *numpixels, origin, extent, &filepath[0], file);

	return result;
}

void writePlumeRaster(
	MemCoord *const raster, 
	const size_t numpixels, 
	MemCoord origin,
	const MemCoord extent,
	const char* filepath,
	const GeoLocNCFile *const src) {
	GeoLocNCFile result;
	int dimids[2]; // dimids stored {y, x}, a la MemCoord
	nc_create(filepath, NC_CLOBBER | NC_NETCDF4, &result.fileid);
	// TODO: switch out xwidth & ywidth for bounds member of GLNCF
	size_t xwidth = extent.x - origin.x,
		ywidth = extent.y - origin.y;
	result.geogroupid = result.fileid;
	nc_def_dim(result.fileid, "x", xwidth, &dimids[1]);
	nc_def_dim(result.fileid, "y", ywidth, &dimids[0]);
	result.bounds = (MemCoord){ywidth, xwidth};
	nc_def_var(result.fileid, "lat", NC_FLOAT, 2, &dimids[0], &result.latvarid);
	nc_def_var(result.fileid, "lon", NC_FLOAT, 2, &dimids[0], &result.lonvarid);
	int varid;
	nc_def_var(result.fileid, "var", NC_SHORT, 2, &dimids[0], &varid);
	double scaleattval = -1;
	nc_get_att_double(src->fileid, 0, "scale_factor", &scaleattval);
	if (scaleattval != -1) {
		printf("scale att set\n");
		nc_put_att_double(result.fileid, varid, "scale_factor", NC_DOUBLE, 1, &scaleattval);
	}

	float* latdst = (float*)malloc(xwidth * ywidth * sizeof(float)), 
		* londst = (float*)malloc(xwidth * ywidth * sizeof(float));
	MemCoord count = {ywidth, xwidth};
	nc_get_vara_float(src->geogroupid, src->latvarid, memData(&origin), memData(&count), latdst);
	nc_get_vara_float(src->geogroupid, src->lonvarid, memData(&origin), memData(&count), londst);
	MemCoord temporigin = (MemCoord){0, 0};
	nc_put_vara_float(result.geogroupid, result.latvarid, memData(&temporigin), memData(&count), latdst);
	nc_put_vara_float(result.geogroupid, result.lonvarid, memData(&temporigin), memData(&count), londst);
	short* data = (short*)malloc(result.bounds.x * result.bounds.y * sizeof(short));
	memset(data, 0, result.bounds.x * result.bounds.y * sizeof(short));
	nc_put_var_short(result.fileid, varid, data);
	short temp;
	MemCoord c;
	for (size_t i = 0; i < numpixels; i++) {
		// TODO: don't hardcode src varid (same above)
		nc_get_var1_short(src->fileid, 0, memData(&raster[i]), &temp);
		c = (MemCoord){raster[i].y - origin.y, raster[i].x - origin.x};
		nc_put_var1_short(result.fileid, varid, memData(&c), &temp);
		// printf("pixel at {%zu, %zu} written as %d\n", c.x, c.y, temp);
	}
	free(latdst);
	free(londst);

	nc_close(result.fileid);
}

