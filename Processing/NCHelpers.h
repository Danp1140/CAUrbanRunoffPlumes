#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include </usr/local/include/curl/curl.h>

#define MAX_CSV_ENTRY_LEN 128
#define GEO_TO_MEM_MAX_ERR 0.2 // generous error, due to L3SMI's resolution
				// TODO: error as a param, so we can have per-product (per-resolution)
				// error tolerance
#define GEO_TO_MEM2_STEP 1. // must be >= 1

typedef struct GeoCoord {
	float lat, lon;
} GeoCoord;

typedef struct MemCoord {
	size_t y, x;
} MemCoord;

typedef struct CSVScanData {
	char word[MAX_CSV_ENTRY_LEN];
	float min, max;
} CSVScanData;

typedef struct GeoLocNCFile {
	int fileid, geogroupid, latvarid, lonvarid, keyvarid;
	uint16_t scale; // m/px
	MemCoord bounds;
} GeoLocNCFile;

typedef struct MemGeoPair {
	MemCoord m;
	GeoCoord g;
} MemGeoPair;

#define GEO_HASH_SIZE 6250000.0 // largest we expect is 2500 * 2500
#define GEO_HASH_LAT_MAX 35.0
#define GEO_HASH_LAT_EXT 3.0 // diff between max & min
#define GEO_HASH_LON_MAX 121.0 // inverting lon to keep things positive
#define GEO_HASH_LON_EXT 3.0

const float GEO_HASH_A, GEO_HASH_B, GEO_HASH_C;

typedef struct BakedGeoLocNCFile {
	GeoLocNCFile child;
	MemGeoPair* pairs; 
} BakedGeoLocNCFile;

void initBakedFile(BakedGeoLocNCFile* f);

size_t hash(const GeoCoord* g);

void push(BakedGeoLocNCFile* f, MemGeoPair p);

MemCoord find(const BakedGeoLocNCFile* f, const GeoCoord g);

// Note: this only prints attributes for variables, not for groups
void printInfo(int id, int printattribs);

// Opens an NC file from the url with supplied queries (all encoded), checking if within the bounds
// of min and max
// Due to the NetCDF library not having good documentation, this function does not work, as
// remote requests (to valid addresses, tested w/ command line curl) fail with a 404
void* getData(const char* opendapurl, uint8_t numvars, const char** varnames, GeoCoord min, GeoCoord max);

// percent-encodes special characters for an opendap url w/ queries
// currently encodes for '?' and ','
const char* encodeURL(const char* url);

static size_t printURLData(void *data, size_t size, size_t nmemb, void* stream);

static size_t minMaxURLData(void *data, size_t size, size_t nmemb, void* stream);

int inBounds(const char* url, GeoCoord min, GeoCoord max);

float geoLen(const GeoCoord* const g);

float geoDist(const GeoCoord* const g1, const GeoCoord* const g2);

float geoDot(const GeoCoord* const g1, const GeoCoord* const g2);

GeoCoord geoSub(const GeoCoord* const lhs, const GeoCoord* const rhs);

size_t* memData(MemCoord* m);

// TODO: add out-of-range checks for memToGeo & geoToMem

GeoCoord _memToGeo(MemCoord m, const int geogroup, const int latvar, const int lonvar);

GeoCoord memToGeo(MemCoord m, const GeoLocNCFile* const f);

// TODO: phase out in favor of below version using GeoLocNCFile arg (same with memToGeo)
MemCoord _geoToMem(GeoCoord g, const int geogroup, const int latvar, const int lonvar);

MemCoord geoToMem(GeoCoord g, const GeoLocNCFile* const f);

// geoToMem2 uses a float version of MemCoord internally to support sub-pixel steps
MemCoord geoToMem2(GeoCoord g, int geogroup, int latvar, int lonvar);

char** formulateMYDATML2URLs(uint16_t year, uint8_t month, uint8_t day, size_t* numurls);

// input year (since 0 A.D.), month (1-12), day of month (1-31). it spits out day of year (1-366)
uint16_t getYearDay(uint16_t year, uint8_t month, uint8_t monthday);
