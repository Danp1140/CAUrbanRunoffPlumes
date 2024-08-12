#include "NCHelpers.h"

float sortScore(const GeoCoord* g) {
	// naive approach, unsure if this works
	if (g->lat > 35.0 || g->lat < 32.0
		|| g->lon > -116.0 || g->lon < -121.0) return 0.0;
	return (g->lat - 32.0) + 3.0 * floor((-g->lon - 116.0) * SORT_SCORE_DLAT);
}

void initBakedFile(BakedGeoLocNCFile* f, size_t n) {
	f->pairs = (MemGeoPair*)malloc(n * sizeof(MemGeoPair));
	f->numpairs = 0;
	f->pairssize = n;
}

void shrinkBakedFile(BakedGeoLocNCFile* f) {
	f->pairs = (MemGeoPair*)realloc(f->pairs, f->numpairs * sizeof(MemGeoPair));
	f->pairssize = f->numpairs;
}

void push(BakedGeoLocNCFile* f, MemGeoPair p) {
	float s = sortScore(&p.g);
	if (s == 0.0) return; // invalid sort score
	if (f->numpairs == f->pairssize) f->pairs = realloc(f->pairs, f->pairssize++);
	if (f->numpairs == 0) {
		f->pairs[0] = p;
		f->numpairs++;
		return;
	}
	MemGeoPair* dst = find(f->pairs, f->numpairs, p.g);
	
	if (sortScore(&dst->g) > s) {
		// move everything up including nearest/dst
		memmove(dst + 1, dst, sizeof(MemGeoPair) * f->numpairs - (dst - f->pairs));
	}
	else { // p not going to have duplicates, unless our scoring system is messed up
	       // move everything up not including nearest/dst
		memmove(dst + 2, dst + 1, sizeof(MemGeoPair) * f->numpairs - (dst + 1 - f->pairs)); 
		dst++;
	}
	*dst = p;
	f->numpairs++;
}

MemGeoPair* find(MemGeoPair* p, size_t np, GeoCoord g) {
	if (np == 1) return p;
	float s = sortScore(&g);
	if (np == 2) {
		return s - sortScore(&p[0].g) < s - sortScore(&p[1].g) ? &p[0] : &p[1];
	}
	size_t mid = np / 2;
	if (s > sortScore(&p[mid].g)) return find(&p[mid], np - mid, g);
	else return find(&p[0], mid, g);
}

MemCoord search(const BakedGeoLocNCFile* f, const GeoCoord g) {
	return (MemCoord) {0, 0};
}

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
	size_t len;
	for (int i = 0; i < numdims; i++) {
		nc_inq_dimname(id, dimids[i], &name[0]);
		nc_inq_dimlen(id, dimids[i], &len);
		printf("\t@%d | %s (%d entries)\n", dimids[i], name, (int)len);
	}

	if (numvars) printf("%d Variables:\n", numvars);
	int varids[numvars];
	nc_type type;
	nc_inq_varids(id, NULL, &varids[0]);
	for (int i = 0; i < numvars; i++) {
		nc_inq_varname(id, varids[i], &name[0]);
		nc_inq_vartype(id, varids[i], &type);
		printf("\t@%d | %s (%d)\n", varids[i], name, type);

		int numvardims;
		nc_inq_varndims(id, varids[i], &numvardims);
		if (numvardims) printf("\t%d Dimensions\n", numvardims);
		int vardimids[numvardims];
		nc_inq_vardimid(id, varids[i], &vardimids[0]);
		for (int j = 0; j < numvardims; j++) {
			nc_inq_dimname(id, vardimids[j], &name[0]);
			printf("\t\t@%d | %s\n", vardimids[j], name);
		}

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

void* getData(const char* opendapurl, uint8_t numvars, const char** varnames, GeoCoord min, GeoCoord max) {
	if (numvars == 0 || !varnames || !opendapurl) {
		return NULL;
	}
	size_t querylen = 1 + numvars - 1; // for question mark and comma delimiters
	for (uint8_t i = 0; i < numvars; i++) {
		querylen += strlen(varnames[i]);
	}
	char* urlwithqueries = malloc(strlen(opendapurl) + querylen);
	strcpy(urlwithqueries, opendapurl);
	char* currchar = urlwithqueries + strlen(opendapurl);
	*currchar = '?';
	currchar++;
	for (uint8_t i = 0; i < numvars; i++) {
		strcpy(currchar, varnames[i]);
		currchar += strlen(varnames[i]);
		if (i + 1 != numvars) *currchar = ',';
		currchar++;
	}

	const char* encodedurl = encodeURL("https://ladsweb.modaps.eosdis.nasa.gov/opendap/hyrax/RemoteResources/laads/allData/61/MYDATML2/2002/185/MYDATML2.A2002185.0000.061.2018003215057.hdf.nc");
	printf("Getting Data from %s\n", encodedurl);

	int fileid;
	clock_t start = clock();
	printf("Opening file from remote...\n");
	if (nc_open(encodedurl, NC_NOWRITE, &fileid) != NC_NOERR) {
		printf("Took %lu seconds\n", (clock() - start) / CLOCKS_PER_SEC);
		printInfo(fileid, 0);
		nc_close(fileid);
	}
	return NULL;
}

const char* encodeURL(const char* url) {
	const char* scan = url;
	size_t curcharidx = strcspn(scan, "?,");
	char* result = "";
	while (scan[curcharidx] != '\0') {
		char* code;
		if (scan[curcharidx] == '?') code = "\%3F";
		else if (scan[curcharidx] == ',') code = "\%2C";
		char* tempresult = malloc(strlen(result) + curcharidx);
		strcpy(tempresult, result);
		strncat(tempresult, scan, curcharidx);
		strcat(tempresult, code);
		if (result[0] != '\0') free(result);
		result = tempresult;
		scan += curcharidx;
		curcharidx++;
		scan++;
		curcharidx = strcspn(scan, "?,");
	}
	char* tempresult = malloc(strlen(result) + strlen(scan));
	strcpy(tempresult, result);
	strcat(tempresult, scan);
	if (result[0] != '\0') free(result);
	result = tempresult;

	return result;
}

static size_t printURLData(void *p, size_t size, size_t nmemb, void* data) {
	for (size_t i = 0; i < nmemb; i++) {
		printf("%c", *((char*)p + i));
	}
	printf("\n");
	return size * nmemb;
}

static size_t minMaxURLData(void *p, size_t size, size_t nmemb, void* data) {
	char* charscan = (char*)p;
	CSVScanData* d = (CSVScanData*)data;
	size_t wordlen = strlen(d->word);
	int hasaletter = 0;
	while (charscan - (char*)p < nmemb * size) {
		if (*charscan == ',' || *charscan == '\n') {
			if (hasaletter == 0) {
				float x = strtof(&d->word[0], NULL);
				if (x != -999) {
					if (x < d->min) d->min = x;
					if (x > d->max) d->max = x;
				}
				printf("num found %f\n", x);
			}
			d->word[0] = '\0';
			wordlen = 0;
			hasaletter = 0;
		}
		else {
			if (isalpha(*charscan)) hasaletter = 1;
			d->word[wordlen] = *charscan;
			d->word[wordlen + 1] = '\0';
			wordlen++;
		}
		charscan++;
	}
	return size * nmemb;
}

int inBounds(const char* url, GeoCoord min, GeoCoord max) {
	CURL* c = curl_easy_init();
	char urlwithquery[strlen(url) + 4 + 1 + 8];
	strcpy(&urlwithquery[0], url);
	strcat(&urlwithquery[0], ".csv?Latitude");
	printf("url: %s\n", urlwithquery);
	curl_easy_setopt(c, CURLOPT_URL, urlwithquery);

	CSVScanData d = {"", 90., -90.};
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, minMaxURLData);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)&d);
	clock_t start = clock();
	curl_easy_perform(c);
	printf("whole request & processing took %fs\n", (float)(clock() - start) / (float)CLOCKS_PER_SEC);
	printf("min: %f, max: %f\n", d.min, d.max);
	return 0;
}

float geoLen(const GeoCoord* const g) {
	return sqrt(pow(g->lat, 2) + pow(g->lon, 2));
}

float geoDist(const GeoCoord* const g1, const GeoCoord* const g2) {
	return sqrt(pow(g1->lat - g2->lat, 2) + pow(g1->lon - g2->lon, 2));
}

float geoDot(const GeoCoord* const g1, const GeoCoord* const g2) {
	return g1->lat * g2->lat + g1->lon * g2->lon;
}

GeoCoord geoSub(const GeoCoord* const lhs, const GeoCoord* const rhs) {
	return (GeoCoord){lhs->lat - rhs->lat, lhs->lon - rhs->lon};
}

size_t* memData(MemCoord* m) {return &m->y;}

GeoCoord _memToGeo(MemCoord m, const int geogroup, const int latvar, const int lonvar) {
	GeoCoord result;
	// TODO: add way to determine appropriate dims
	// likely best to store offset/dim data in GeoLoc struct to avoid repeated inq calls
	nc_get_var1_float(geogroup, latvar, memData(&m), &result.lat);
	nc_get_var1_float(geogroup, lonvar, memData(&m), &result.lon);
	return result;
}

GeoCoord memToGeo(MemCoord m, const GeoLocNCFile* const f) {
	return _memToGeo(m, f->geogroupid, f->latvarid, f->lonvarid);
}

MemCoord _geoToMem(GeoCoord g, const int geogroup, const int latvar, const int lonvar) {
	MemCoord bounds;
	int nlatdims, nlondims;
	nc_inq_varndims(geogroup, latvar, &nlatdims);
	nc_inq_varndims(geogroup, lonvar, &nlondims);
	int latdimids[nlatdims], londimids[nlondims];
	nc_inq_vardimid(geogroup, latvar, &latdimids[0]);
	nc_inq_vardimid(geogroup, lonvar, &londimids[0]);
	size_t offset = 0;
	if (nlatdims == nlondims) {
		size_t dimlen;
		if (nlatdims == 2) {
			// for some reason y is stored before x
			// this assumes dims are always stored y-res @ 0, x-res @ 1
			nc_inq_dimlen(geogroup, latdimids[0], &dimlen);
			bounds.y = dimlen;
			nc_inq_dimlen(geogroup, latdimids[1], &dimlen);
			bounds.x = dimlen;
		}
		// if ndims == 1, we'll assume a L3SMI-style lin indep lat & lon
		else if (nlatdims == 1) {
			offset = 1;
			nc_inq_dimlen(geogroup, latdimids[0], &dimlen);
			bounds.y = dimlen;
			nc_inq_dimlen(geogroup, londimids[0], &dimlen);
			bounds.x = dimlen;
		}
	}
	else {
		printf("unknown lat/lon dimension format in geoToMem!!\n");
	}

	MemCoord guess = {bounds.y / 2, bounds.x / 2};
	GeoCoord currentGeo, dgdmx, dgdmy;
	do {
		nc_get_var1_float(geogroup, latvar, memData(&guess), &currentGeo.lat);
		nc_get_var1_float(geogroup, lonvar, memData(&guess) + offset, &currentGeo.lon);
		/*
		printf("new guess (%zu, %zu)\n", guess.x, guess.y);
		printf("corresponds to geo (%f, %f)\n", currentGeo.lat, currentGeo.lon);
		*/

		guess.x++;
		nc_get_var1_float(geogroup, latvar, memData(&guess), &dgdmx.lat);
		nc_get_var1_float(geogroup, lonvar, memData(&guess) + offset, &dgdmx.lon);
		guess.x--;
		dgdmx = geoSub(&dgdmx, &currentGeo);

		guess.y++;
		nc_get_var1_float(geogroup, latvar, memData(&guess), &dgdmy.lat);
		nc_get_var1_float(geogroup, lonvar, memData(&guess) + offset, &dgdmy.lon);
		guess.y--;
		dgdmy = geoSub(&dgdmy, &currentGeo);

		/*
		printf("dg/dm_x = (%f, %f)\n", dgdmx.lat, dgdmx.lon);
		printf("dg/dm_y = (%f, %f)\n", dgdmy.lat, dgdmy.lon);
		*/

		GeoCoord adjustedtarget = geoSub(&g, &currentGeo);
		float inc = geoDot(&dgdmx, &adjustedtarget) / geoLen(&dgdmx);
		guess.x += inc > 0 ? ceil(inc) : floor(inc);
		inc = geoDot(&dgdmy, &adjustedtarget) / geoLen(&dgdmy);
		guess.y += inc > 0 ? ceil(inc) : floor(inc);
	} while (geoDist(&g, &currentGeo) > GEO_TO_MEM_MAX_ERR_L3SMI);
	return guess;
}

MemCoord geoToMem(GeoCoord g, const GeoLocNCFile* const f) {
	MemCoord bounds;
	int nlatdims, nlondims;
	nc_inq_varndims(f->geogroupid, f->latvarid, &nlatdims);
	nc_inq_varndims(f->geogroupid, f->lonvarid, &nlondims);
	int latdimids[nlatdims], londimids[nlondims];
	nc_inq_vardimid(f->geogroupid, f->latvarid, &latdimids[0]);
	nc_inq_vardimid(f->geogroupid, f->lonvarid, &londimids[0]);
	size_t offset = 0;
	if (nlatdims == nlondims) {
		size_t dimlen;
		if (nlatdims == 2) {
			// for some reason y is stored before x
			// this assumes dims are always stored y-res @ 0, x-res @ 1
			nc_inq_dimlen(f->geogroupid, latdimids[0], &dimlen);
			bounds.y = dimlen;
			nc_inq_dimlen(f->geogroupid, latdimids[1], &dimlen);
			bounds.x = dimlen;
		}
		// if ndims == 1, we'll assume a L3SMI-style lin indep lat & lon
		else if (nlatdims == 1) {
			offset = 1;
			nc_inq_dimlen(f->geogroupid, latdimids[0], &dimlen);
			bounds.y = dimlen;
			nc_inq_dimlen(f->geogroupid, londimids[0], &dimlen);
			bounds.x = dimlen;
		}
	}
	else {
		printf("unknown lat/lon dimension format in geoToMem!!\n");
	}

	MemCoord guess = {f->bounds.y / 2, f->bounds.x / 2};
	GeoCoord currentGeo, dgdmx, dgdmy;
	size_t counter = 0;
	float incx, incy, totarget;
	do {
		nc_get_var1_float(f->geogroupid, f->latvarid, memData(&guess), &currentGeo.lat);
		nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&guess) + offset, &currentGeo.lon);
		/*
		printf("new guess (%zu, %zu)\n", guess.x, guess.y);
		printf("corresponds to geo (%f, %f)\n", currentGeo.lat, currentGeo.lon);
		*/

		guess.x++;
		nc_get_var1_float(f->geogroupid, f->latvarid, memData(&guess), &dgdmx.lat);
		nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&guess) + offset, &dgdmx.lon);
		guess.x--;
		dgdmx = geoSub(&dgdmx, &currentGeo);

		guess.y++;
		nc_get_var1_float(f->geogroupid, f->latvarid, memData(&guess), &dgdmy.lat);
		nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&guess) + offset, &dgdmy.lon);
		guess.y--;
		dgdmy = geoSub(&dgdmy, &currentGeo);

		/*
		printf("dg/dm_x = (%f, %f)\n", dgdmx.lat, dgdmx.lon);
		printf("dg/dm_y = (%f, %f)\n", dgdmy.lat, dgdmy.lon);
		*/

		/*
		GeoCoord adjustedtarget = geoSub(&g, &currentGeo);
		float inc = geoDot(&dgdmx, &adjustedtarget) / geoLen(&dgdmx);
		guess.x += inc > 0 ? ceil(inc) : floor(inc);
		if (guess.x < 0 || guess.x >= f->bounds.x) {
			guess = (MemCoord){-1u, -1u};
			break;
		}
		inc = geoDot(&dgdmy, &adjustedtarget) / geoLen(&dgdmy);
		guess.y += inc > 0 ? ceil(inc) : floor(inc);
		if (guess.y < 0 || guess.y >= f->bounds.y) {
			guess = (MemCoord){-1u, -1u};
			break;
		}
		*/

		// TODO: declare ALL of these outside the loop
		// and use totarget in while check
		GeoCoord adjustedtarget = geoSub(&g, &currentGeo);
		totarget = geoLen(&adjustedtarget);
		if (totarget < f->maxgeotomemerr) break;
		// below is what got us exact results when onscreen (after ~750 ops)
		/*
		float incx = (totarget - geoDist(&adjustedtarget, &dgdmx)) / totarget * 10.;
		float incy = (totarget - geoDist(&adjustedtarget, &dgdmy)) / totarget * 10.;
		*/
		/*
		 * One more idea is to scale incx and incy so that they're greater than 1,
		 * but keep their proportional difference (i.e. 0.004, 0.001 => 4, 1)
		 */
		incx = (totarget - geoDist(&adjustedtarget, &dgdmx)) * totarget * 100;
		incy = (totarget - geoDist(&adjustedtarget, &dgdmy)) * totarget * 100;
		if (fabsf(incx) < 1 && fabsf(incy) < 1) {
			if (fabsf(incx) > fabsf(incy)) {
				// incy = 0;
				incx /= fabsf(incy);
				incy = 1;
			}
			else {
				//incx = 0;
				incy /= fabsf(incx);
				incx = 1;
			}
		}
		// printf("incx: %f\n", incx);
		guess.x += incx > 0 ? ceil(incx) : floor(incx);
		if (guess.x < 0 || guess.x >= f->bounds.x) {
			guess = (MemCoord){-1u, -1u};
			break;
		}
		// printf("incy: %f\n", incy);
		guess.y += incy > 0 ? ceil(incy) : floor(incy);
		if (guess.y < 0 || guess.y >= f->bounds.y) {
			guess = (MemCoord){-1u, -1u};
			break;
		}

		if (counter++ > GEO_TO_MEM_OPOUT) {
			// printf("geoToMem out of ops (err = %f)\n", geoDist(&g, &currentGeo));
			break;
		}
	} while (geoDist(&g, &currentGeo) > f->maxgeotomemerr);
	// printf("took %zu ops\n", counter);
	return guess;
}

MemCoord geoToMem2(GeoCoord g, int geogroup, int latvar, int lonvar) {
	MemCoord bounds;
	int ndims;
	nc_inq_varndims(geogroup, latvar, &ndims);
	int dimids[ndims];
	nc_inq_vardimid(geogroup, latvar, &dimids[0]);
	size_t dimlen;
	// for some reason y is stored before x
	nc_inq_dimlen(geogroup, dimids[0], &dimlen);
	bounds.y = dimlen;
	nc_inq_dimlen(geogroup, dimids[1], &dimlen);
	bounds.x = dimlen;

	// guess stores y first, then x
	float guess[2] = {(float)bounds.x / 2., (float)bounds.y / 2.};
	// for access
	MemCoord guessmem;
	GeoCoord currentGeo, dgdmx, dgdmy;
	do {
		guessmem = (MemCoord){(size_t)guess[0], (size_t)guess[1]};
		nc_get_var1_float(geogroup, latvar, memData(&guessmem), &currentGeo.lat);
		nc_get_var1_float(geogroup, lonvar, memData(&guessmem), &currentGeo.lon);
		/*
		printf("new guess (%f, %f)\n", guess[1], guess[0]);
		printf("corresponds to geo (%f, %f)\n", currentGeo.lat, currentGeo.lon);
		*/

		guessmem = (MemCoord){guess[0], guess[1] + GEO_TO_MEM2_STEP};
		nc_get_var1_float(geogroup, latvar, memData(&guessmem), &dgdmx.lat);
		nc_get_var1_float(geogroup, lonvar, memData(&guessmem), &dgdmx.lon);
		dgdmx = geoSub(&dgdmx, &currentGeo);

		guessmem = (MemCoord){guess[0] + GEO_TO_MEM2_STEP, guess[1]};
		nc_get_var1_float(geogroup, latvar, memData(&guessmem), &dgdmy.lat);
		nc_get_var1_float(geogroup, lonvar, memData(&guessmem), &dgdmy.lon);
		dgdmy = geoSub(&dgdmy, &currentGeo);

		/*
		printf("dg/dm_x = (%f, %f)\n", dgdmx.lat, dgdmx.lon);
		printf("dg/dm_y = (%f, %f)\n", dgdmy.lat, dgdmy.lon);
		*/

		GeoCoord adjustedtarget = geoSub(&g, &currentGeo);
		float inc = geoDot(&dgdmx, &adjustedtarget) / geoLen(&dgdmx);
		guess[1] += inc;
		inc = geoDot(&dgdmy, &adjustedtarget) / geoLen(&dgdmy);
		guess[0] += inc;
	} while (geoDist(&g, &currentGeo) > GEO_TO_MEM_MAX_ERR_L3SMI);
	return (MemCoord){guess[0], guess[1]};
}

char** formulateMYDATML2URLs(uint16_t year, uint8_t month, uint8_t day, size_t* numurls) {
	uint16_t yday = getYearDay(year, month, day);

	const char* baseurl = "https://ladsweb.modaps.eosdis.nasa.gov/opendap/hyrax/RemoteResources/laads/allData/61/MYDATML2/";
	const char* template = "https://ladsweb.modaps.eosdis.nasa.gov/opendap/hyrax/RemoteResources/laads/allData/61/MYDATML2/%04u/%03u/MYDATML2.A%04u%03u.%02u%02u.061.2018010195314.hdf";
	const size_t len = strlen(baseurl) + 5 + 4 + 9 + 9 + 5 + 4 + 14 + 3;
	// base str + year/ + day/ + MYDATML2. + Ayearday. + hrmn. + 061. + lastedited. + hdf
	char** dst = malloc(24 * 60 / 5 * sizeof(char*));
	*numurls = 24 * 60 / 5;
	uint16_t min, hour;
	for (uint16_t i = 0; i < 24 * 60 / 5; i++) {
		min = i * 5;
		hour = floor(min / 60.);
		min = min - 60 * hour;
		dst[i] = (char*)malloc(len * sizeof(char)); 
		sprintf(dst[i], template, year, yday, year, yday, hour, min); 
		// printf("%s\n", dst[i]);
	}
	return dst;
}

uint16_t getYearDay(uint16_t year, uint8_t month, uint8_t monthday) {
	uint16_t ydaytotal = monthday;
	if (month != 1) ydaytotal += 31;
	if (month != 2) ydaytotal += (year % 4 == 0) ? 29 : 28;
	if (month != 3) ydaytotal += 31;
	if (month != 4) ydaytotal += 30;
	if (month != 5) ydaytotal += 31;
	if (month != 6) ydaytotal += 30;
	if (month != 7) ydaytotal += 31;
	if (month != 8) ydaytotal += 31;
	if (month != 9) ydaytotal += 30;
	if (month != 10) ydaytotal += 31;
	if (month != 11) ydaytotal += 30;
	return ydaytotal;
}


