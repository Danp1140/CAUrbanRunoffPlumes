#include "NCHelpers.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#define STUDY_AREA_RADIUS 12500 // in meters 
#define CUTOFF_RASTER_BBOX_MARGIN 1000 // in meters

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

#define ATML2_DETERMINED_BIT 0x01
#define ATML2_UNCERTAIN_BIT 0x04 // note that both cloudy & uncertain have uncertain bit set

#define OUTPUT_FILEPATH "/Users/danp/Desktop/CAUrbanRunoffPlumes/Processing/Data/"

typedef struct OutputRow {
	uint16_t year;
	uint8_t month, day;
	float plumearea[1], avgintensity[1];
} OutputRow;

typedef enum DataSet {
	MYDATML2,
	L3SMI,
	MYD09GA
} DataSet;

const GeoCoord lariver = {33.755, -118.185},
			sgriver = {33.74, -118.115},
			sariver = {33.63, -117.96},
			bcreek = {33.96, -118.46};

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

void printBox(const char* m);

void writeData(FILE* f, const OutputRow* r);

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("Wrong number of arguments (%d)\nCorrect usage: ./QuantifyPlumes <year> <month> <day>\n", argc);
		return 0;
	}

	uint16_t year = atoi(argv[1]);
	uint8_t month = atoi(argv[2]),
		day = atoi(argv[3]);
	printf("––– QuantifyPlumes %d/%d/%d –––\n", month, day, year);

	uint8_t numatml2files, numl3smifiles, nummyd09gafiles;
	printf("Input filepaths:\n");
	int* myd09gafiles = openNCFile(
			MYD09GA_FILEPATH_PREFIX,
			atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 
			&nummyd09gafiles),
		* atml2files = openNCFile(
			MYDATML2_FILEPATH_PREFIX,
			atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), 
			&numatml2files);

	GeoLocNCFile glmyd09gafiles[nummyd09gafiles];
	initGLFiles(&glmyd09gafiles[0], &myd09gafiles[0], nummyd09gafiles, MYD09GA);
	for (uint8_t i = 0; i < nummyd09gafiles; i++) {
	}
	GeoLocNCFile glatml2files[numatml2files];
	initGLFiles(&glatml2files[0], &atml2files[0], numatml2files, MYDATML2);

	FILE* lariverout = fopen(OUTPUT_FILEPATH "LARiver.csv", "a");

	size_t n;
	short cutoff = 3000;
	OutputRow rowtemp = {year, month, day, 0, 0};
	for (uint8_t i = 0; i < nummyd09gafiles; i++) {
		printf("LA plume cutoff for MYD09GA file %d\n", i);
		MemCoord* larivercr = calcCutoffRaster(&glmyd09gafiles[i], &glatml2files[0], numatml2files, (void*)(&cutoff), &lariver, STUDY_AREA_RADIUS, &n);
		writeData(lariverout, &rowtemp);
		if (n > 0) {
			char* msg;
			asprintf(&msg, "MYD09GA LA plume mask %zu pixels large", n);
			printBox(msg);
			free(msg);
			writeData(lariverout, &rowtemp);
		}
		if (larivercr) free(larivercr);
	}

	fclose(lariverout);

	for (uint8_t i = 0; i < numatml2files; i++) {
		nc_close(atml2files[i]);
	}
	free(atml2files);
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
	*numfiles = 0;
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
			printf("[%d]: %s\n", *numfiles, file->fts_name);
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

void initGLFiles(GeoLocNCFile* dst, int* files, const uint8_t numfiles, DataSet d) {
	for (uint8_t i = 0; i < numfiles; i++) {
		switch (d) {
			case MYDATML2:
				dst[i] = (GeoLocNCFile){files[i], files[i], MYDATML2_LAT_ID, MYDATML2_LON_ID, MYDATML2_CLOUDMASK_ID, 1000, GEO_TO_MEM_MAX_ERR_ATML2, {0, 0}};
				break;
			case L3SMI:
				dst[i] = (GeoLocNCFile){files[i], files[i], L3SMI_LAT_ID, L3SMI_LON_ID, 2, 4638, GEO_TO_MEM_MAX_ERR_L3SMI, {0, 0}};
				break;
			case MYD09GA:
				dst[i] = (GeoLocNCFile){files[i], files[i], MYD09GA_LAT_ID, MYD09GA_LON_ID, MYD09GA_RRS_ID, 500, GEO_TO_MEM_MAX_ERR_MYD09GA, {0, 0}};
				break;
		}
		int dimids[2];
		nc_inq_vardimid(dst[i].fileid, dst[i].latvarid, &dimids[0]); // presuming lat & lon both have 2 dimensions and they're the same
									     // if we need to run this on L3SMI, gonna need to do some checking
		nc_inq_dimlen(dst[i].fileid, dimids[0], &dst[i].bounds.y);
		nc_inq_dimlen(dst[i].fileid, dimids[1], &dst[i].bounds.x);
	}
}

void initCloudMaps(BakedGeoLocNCFile* dst, GeoLocNCFile* files, const uint8_t numfiles) {
	MemCoord m;
	GeoCoord g, g2;
	float s;
	for (uint8_t i = 0; i < numfiles; i++) {
		initBakedFile(&dst[i], files[i].bounds.x * files[i].bounds.y);
		for (m.y = 0; m.y < files[i].bounds.y; m.y++) {
			for (m.x = 0; m.x < files[i].bounds.x; m.x++) {
				g = memToGeo(m, &files[i]);
				s = sortScore(&g);
				push(&dst[i], (MemGeoPair){m, g});
				if (s != 0.0) {
					g2 = find(dst[i].pairs, dst[i].numpairs, g)->g;
					if (g.lat != g2.lat || g.lon != g2.lon) printf("\033[101;97m");
					printf("{%f, %f} transformed to {%f, %f}", g.lat, g.lon, g2.lat, g2.lon);
					if (g.lat != g2.lat || g.lon != g2.lon) printf("\033[0m");
					printf("\n");
				}
			}
		}
		shrinkBakedFile(&dst[i]);
	}
	printf("done\n");
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

	MemCoord memcenter = geoToMem(*center, file);
	if (memcenter.x == -1u || memcenter.y == -1u) {
		printf("center outside of image\n");
		return NULL;
	}
	const size_t pixelradius = STUDY_AREA_RADIUS / file->scale;
	const size_t pixelmargin = CUTOFF_RASTER_BBOX_MARGIN / file->scale;
	MemCoord extent = {
		memcenter.y + pixelradius + pixelmargin, 
		memcenter.x + pixelradius + pixelmargin
	};
	MemCoord origin = {
		memcenter.y - pixelradius - pixelmargin, 
		memcenter.x - pixelradius - pixelmargin
	};
	if (extent.x > file->bounds.x) extent.x = file->bounds.x - 1;
	if (extent.y > file->bounds.y) extent.y = file->bounds.y - 1;
	if (origin.x < 0) origin.x = 0; 
	if (origin.y < 0) origin.y = 0; 

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
	size_t totalinradius = 0,
	       occludedbycloud = 0,
	       belowcutoff = 0,
	       notwater = 0,
	       rasterized = 0;
	for (current.y = origin.y; current.y < extent.y; current.y++) {
		for (current.x = origin.x; current.x < extent.x; current.x++) {
			gtemp = memToGeo(current, file);
			if (sqrt(pow(current.x - memcenter.x, 2) + pow(current.y - memcenter.y, 2)) < pixelradius) {
				totalinradius++;
				clear = 0;
				for (cloudcounter = 0; cloudcounter < numcloudfiles; cloudcounter++) {
					gtemp = memToGeo(current, file);
					cloudmcoord.m = geoToMem(gtemp, &clouds[cloudcounter]);
					if (cloudmcoord.m.x == -1u || cloudmcoord.m.y == -1u) {
						continue;
					}
					// TODO: Cloud_Mask is actually an NC_INT for some reason
					nc_get_var1_ubyte(
						clouds[cloudcounter].fileid, 
						clouds[cloudcounter].keyvarid, 
						&cloudmcoord.byte, 
						&cloudbyte);
					// if determined and either cloudy or uncertain, don't store
					if (cloudbyte & (ATML2_DETERMINED_BIT | ATML2_UNCERTAIN_BIT)) clear = 1;
					else clear = 0;
				}
				if (clear == 0) {
					occludedbycloud++;
					continue;
				}
				store = 0;
				if (type == NC_FLOAT) {
					nc_get_var1_float(
						file->fileid, 
						file->keyvarid, 
						memData(&current), 
						(float*)(&datatemp[0]));
					if (*((float*)(&datatemp[0])) > (*((float*)(cutoff)))) store = 1;
				} else if (type == NC_SHORT) {
					nc_get_var1_short(
						file->fileid, 
						file->keyvarid, 
						memData(&current), 
						(short*)(&datatemp[0]));
					if (*((short*)(&datatemp[0])) > (*((short*)(cutoff)))) store = 1;
				}
				if (store) {
					result[*numpixels] = current;
					(*numpixels)++;
					// ideally this check would never be needed, but its not much overhead
					if (*numpixels == maxarea) { 
						printf("numpixels %zu larger than areasize %zu\n", *numpixels, maxarea);
						return result;
					}
					rasterized++;
				}
				else belowcutoff++;
			}
		}
	}

	printf("Stats:\n"
			"\tPixels in radius: %zu\n"
			"\t%% rasterized: %f\n"
			"\t%% occluded by clouds: %f\n"
			"\t%% below cutoff: %f\n", 
			totalinradius, 
			(float)rasterized / (float)totalinradius,
			(float)occludedbycloud / (float)totalinradius,
			(float)belowcutoff / (float)totalinradius);

	if (*numpixels == 0) {
		free(result);
		printf("plume mask empty\n");
		return NULL;
	}
	result = realloc(result, sizeof(MemCoord) * *numpixels);
	
	// troubleshooting filewrite, should not write every analysis file i dont think
	size_t len;
	nc_inq_path(file->fileid, &len, NULL);
	char path[len];
	nc_inq_path(file->fileid, NULL, path);
	// this is a really janky way to do this but since this is just a debugging measure its whatever
	char* template;
	if (center == &lariver) {
		template = "/Users/danp/Desktop/CAUrbanRunoffPlumes/Processing/%s/lariver_%s";
	} else if (center == &sgriver) {
		template = "/Users/danp/Desktop/CAUrbanRunoffPlumes/Processing/%s/sgriver_%s";
	} else if (center == &sariver) {
		template = "/Users/danp/Desktop/CAUrbanRunoffPlumes/Processing/%s/sariver_%s";
	} else if (center == &bcreek) {
		template = "/Users/danp/Desktop/CAUrbanRunoffPlumes/Processing/%s/bcreek_%s";
	} else printf("unknown center addr in cutoff raster\n");
	char* filepath;
	char* suffix = strrchr(path, '/') + 1;
	char* infix = strchr(path + 1, '/');
	infix = strchr(infix + 1, '/');
	infix = strchr(infix + 1, '/');
	infix = strchr(infix + 1, '/');
	infix = strchr(infix + 1, '/');
	infix = strchr(infix + 1, '/');
	infix = strchr(infix + 1, '/') + 1;
	*strrchr(infix, '/') = '\0';
	asprintf(&filepath, template, infix, suffix);
	// until i get off this plane and figure out how to mkdir in c
	fclose(fopen(filepath, "w"));
	printf("%s\n", filepath);
	writePlumeRaster(result, *numpixels, origin, extent, &filepath[0], file);
	free(filepath);

	return result;
}

void writePlumeRaster(
	MemCoord *const raster, 
	const size_t numpixels, 
	MemCoord origin,
	const MemCoord extent,
	const char* filepath,
	const GeoLocNCFile *const src) {
	struct stat s;
	char daydirpath[strlen(filepath)];
	strcpy(daydirpath, filepath);
	*strrchr(daydirpath, '/') = '\0';
	char monthdirpath[strlen(daydirpath)];
	strcpy(monthdirpath, daydirpath);
	*strrchr(monthdirpath, '/') = '\0';
	char yeardirpath[strlen(monthdirpath)];
	strcpy(yeardirpath, monthdirpath);
	*strrchr(yeardirpath, '/') = '\0';
	if (stat(yeardirpath, &s) == -1) mkdir(yeardirpath, S_IRWXU);
	if (stat(monthdirpath, &s) == -1) mkdir(monthdirpath, S_IRWXU);
	if (stat(daydirpath, &s) == -1) mkdir(daydirpath, S_IRWXU);

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

void printBox(const char* m) {
	size_t mlen = strlen(m);
	char hline[5 + mlen];
	memset(&hline[0], '-', 4 + mlen);
	hline[4 + mlen] = '\0';
	printf("%s\n", hline);
	printf("| %s |\n", m);
	printf("%s\n", hline);
}

void writeData(FILE* f, const OutputRow* r) {
	char* temp = "%02d/%02d/%04d,%f,%f\n";
	char* dst;
	asprintf(&dst, temp, 
			r->month, r->day, r->year, 
			r->plumearea[0], r->avgintensity[0]);
	fwrite(dst, 1, strlen(dst), f);
	free(dst);
}
