#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include </usr/local/include/curl/curl.h>

// #define VERBOSE_GEOTOMEM2

#define MAX_CSV_ENTRY_LEN 128
#define GEO_TO_MEM_MAX_ERR_ATML2 0.01
#define GEO_TO_MEM_MAX_ERR_MYD09GA 0.004
#define GEO_TO_MEM_MAX_ERR_L3SMI 0.2 
#define GEO_TO_MEM_MAX_ERR_WATER_MASK 0.0002 // theoretically could be lower
#define GEO_TO_MEM2_STEP 1. // must be >= 1
#define SORT_SCORE_DLAT 1e-4
#define GEO_TO_MEM_OPOUT 1000 // num loops before we give up ongeoToMem
#define SIMPLEX_MAX_DEPTH 100

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
	float maxgeotomemerr;
	MemCoord bounds;
	size_t latoffset, lonoffset; // used for indexing lat/lon data regardless of if they're 2d or 1d
} GeoLocNCFile;

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

// the length of g, as if it were an ordinary 2-vector (i.e., sqrt(x^2 + y^2))
float geoLen(const GeoCoord* const g);

// abs value of distance between g1 & g2 (no projection applied)
float geoDist(const GeoCoord* const g1, const GeoCoord* const g2);

// dot product, as if g1 and g2 were ordinary 2-vectors
float geoDot(const GeoCoord* const g1, const GeoCoord* const g2);

// component-wise subtraction of lhs - rhs
GeoCoord geoSub(const GeoCoord* const lhs, const GeoCoord* const rhs);

size_t* memData(MemCoord* m);

GeoCoord _memToGeo(MemCoord m, const int geogroup, const int latvar, const int lonvar);

GeoCoord memToGeo(MemCoord m, const GeoLocNCFile* const f);

GeoCoord subpixelMemToGeo(float* m, const GeoLocNCFile* const f);

// TODO: phase out in favor of below version using GeoLocNCFile arg (same with memToGeo)
MemCoord _geoToMem(GeoCoord g, const int geogroup, const int latvar, const int lonvar);

// simple gradient-descent geoToMem sampler
MemCoord geoToMem(GeoCoord g, const GeoLocNCFile* const f);

// Nelder-Mead geoToMem sampler (recommended for accuracy, speed varies by file)
MemCoord geoToMem2(GeoCoord g, const GeoLocNCFile* const f);

// recursive call, finding new Nelder-Mead simplex and returning after max error or max recursion has been reached
void simplex(MemCoord* insimp, GeoCoord g, const GeoLocNCFile* const f, float alpha, float gamma, float rho, float sigma);

// subpixel version of simplex
void simplex2(float** insimp, GeoCoord g, const GeoLocNCFile* const f, float alpha, float gamma, float rho, float sigma, size_t depth);

// ordinary subtraction, but clamps bottom end to 0 to avoid unsigned underflow
size_t sizeTypeSub(size_t lhs, size_t rhs);

// vector math op for use in simplex
MemCoord derivedVector(const GeoLocNCFile* const f, const MemCoord* const centroid, float m, const MemCoord* const v1, const MemCoord* const v2);

// subpixel version of derivedVector
void derivedVector2(float* dst, const GeoLocNCFile* const f, const float* const centroid, float m, const float* const v1, const float* const v2);

// for use in sorting simplex points by fitness in simplex
int fcomp(const void* lhs, const void* rhs);

char** formulateMYDATML2URLs(uint16_t year, uint8_t month, uint8_t day, size_t* numurls);

// input year (since 0 A.D.), month (1-12), day of month (1-31). it spits out day of year (1-366)
uint16_t getYearDay(uint16_t year, uint8_t month, uint8_t monthday);
