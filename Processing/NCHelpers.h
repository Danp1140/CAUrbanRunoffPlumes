#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include </usr/local/include/curl/curl.h>

#define MAX_CSV_ENTRY_LEN 128
#define GEO_TO_MEM_MAX_ERR 1e-2
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

GeoCoord memToGeo(MemCoord m, int geogroup, int latvar, int lonvar);

MemCoord geoToMem(GeoCoord g, int geogroup, int latvar, int lonvar);

// geoToMem2 uses a float version of MemCoord internally to support sub-pixel steps
MemCoord geoToMem2(GeoCoord g, int geogroup, int latvar, int lonvar);

char** formulateMYDATML2URLs(uint16_t year, uint8_t month, uint8_t day, size_t* numurls);

// input year (since 0 A.D.), month (1-12), day of month (1-31). it spits out day of year (1-366)
uint16_t getYearDay(uint16_t year, uint8_t month, uint8_t monthday);
