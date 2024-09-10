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

GeoCoord memToGeo(MemCoord m, const GeoLocNCFile* const f) {
	GeoCoord result;
	nc_get_var1_float(f->geogroupid, f->latvarid, memData(&m) + f->latoffset, &result.lat);
	nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&m) + f->lonoffset, &result.lon);
	return result;
}

GeoCoord subpixelMemToGeo(float* m, const GeoLocNCFile* const f) {
	// nearest filtering
	/*
	if (m[0] < 0) {
		GeoCoord result;
		result = memToGeo((MemCoord){0, round(m[1])}, f);
		result.lat += m[0];
		return result;
	}
	if (m[0] > (float)f->bounds.y) {
		GeoCoord result;
		result = memToGeo((MemCoord){f->bounds.y - 1, round(m[1])}, f);
		result.lat += m[0] - f->bounds.y - 1;
		return result;
	}
	if (m[1] < 0) {
		GeoCoord result;
		result = memToGeo((MemCoord){round(m[1]), 0}, f);
		result.lon += m[1];
		return result;
	}
	if (m[1] > (float)f->bounds.x) {
		GeoCoord result;
		result = memToGeo((MemCoord){round(m[1]), f->bounds.x - 1}, f);
		result.lon += m[1] - f->bounds.x - 1;
		return result;
	}
	*/
	if (m[0] < 0 || m[1] < 0 || m[0] > (float)f->bounds.y - 1 || m[1] > (float) f->bounds.y - 1) 
		return (GeoCoord){-999, -999};
	return memToGeo((MemCoord){round(m[0]), round(m[1])}, f);
	// linear filtering (incomplete)
	/*
	GeoCoord result, gtemp;
	MemCoord m0 = {round(m[0]), round(m[1])}, m1 = m0, m2 = m0;
	if ((float)m0.y > m[0]) m1.y++;
	else m1.y--;
	if ((float)m0.x > m[1]) m2.x++;
	else m2.y--;
	nc_get_var1_float(f->geogroupid, f->latvarid, memData(&m0) + f->latoffset, &result.lat);
	nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&m0) + f->lonoffset, &result.lon);
	nc_get_var1_float(f->geogroupid, f->latvarid, memData(&m1) + f->latoffset, &gtemp.lat);
	nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&m1) + f->lonoffset, &gtemp.lon);
	result.lat += gtemp.lat * (float)m0.y - m[0];
	result.lon += gtemp.lon * (float)m0.y - m[0];
	nc_get_var1_float(f->geogroupid, f->latvarid, memData(&m2) + f->latoffset, &gtemp.lat);
	nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&m2) + f->lonoffset, &gtemp.lon);
	result.lat += gtemp.lat;
	result.lon += gtemp.lon;
	*/
}

MemCoord geoToMem(GeoCoord g, const GeoLocNCFile* const f) {
	MemCoord guess = {f->bounds.y / 4, f->bounds.x / 4};
	GeoCoord currentGeo, dgdmx, dgdmy;
	size_t counter = 0;
	float incx, incy, totarget;
	do {
		nc_get_var1_float(f->geogroupid, f->latvarid, memData(&guess) + f->latoffset, &currentGeo.lat);
		nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&guess) + f->lonoffset, &currentGeo.lon);
		/*
		printf("new guess (%zu, %zu)\n", guess.x, guess.y);
		printf("corresponds to geo (%f, %f)\n", currentGeo.lat, currentGeo.lon);
		*/

		guess.x++;
		nc_get_var1_float(f->geogroupid, f->latvarid, memData(&guess) + f->latoffset, &dgdmx.lat);
		nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&guess) + f->lonoffset, &dgdmx.lon);
		guess.x--;
		dgdmx = geoSub(&dgdmx, &currentGeo);

		guess.y++;
		nc_get_var1_float(f->geogroupid, f->latvarid, memData(&guess) + f->latoffset, &dgdmy.lat);
		nc_get_var1_float(f->geogroupid, f->lonvarid, memData(&guess) + f->lonoffset, &dgdmy.lon);
		guess.y--;
		dgdmy = geoSub(&dgdmy, &currentGeo);

		/*
		printf("dg/dm_x = (%f, %f)\n", dgdmx.lat, dgdmx.lon);
		printf("dg/dm_y = (%f, %f)\n", dgdmy.lat, dgdmy.lon);
		*/

		// and use totarget in while check
		GeoCoord adjustedtarget = geoSub(&g, &currentGeo);
		totarget = geoLen(&adjustedtarget);
		if (totarget < f->maxgeotomemerr) break;
		// original code in commit
		incx = (totarget - geoDist(&adjustedtarget, &dgdmx)) * totarget * 100;
		incy = (totarget - geoDist(&adjustedtarget, &dgdmy)) * totarget * 100;
		/*
		incx = (totarget - geoDist(&adjustedtarget, &dgdmx)) * totarget * f->bounds.x;
		incy = (totarget - geoDist(&adjustedtarget, &dgdmy)) * totarget * f->bounds.y;
		*/
		if (fabsf(incx) < 1 && fabsf(incy) < 1) {
			if (fabsf(incx) > fabsf(incy)) {
				incx /= fabsf(incy);
				incy = 1;
			}
			else {
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

MemCoord geoToMem2(GeoCoord g, const GeoLocNCFile* const f) {
	const size_t width = f->bounds.x / 4,
	      height = f->bounds.y / 4;
	float simp[3][2];
	simp[0][0] = f->bounds.y / 2 - height;
	simp[0][1] = f->bounds.x / 2 - width;
	simp[1][0] = f->bounds.y / 2 - height;
	simp[1][1] = f->bounds.x / 2 + width;
	simp[2][0] = f->bounds.y / 2 + height;
	simp[2][1] = f->bounds.x / 2;
	float* simptemp[3];
	for (uint8_t i = 0; i < 3; i++) simptemp[i] = &simp[i][0];
	simplex2(simptemp, g, f, 1, 2, 0.5, 0.5, 0);
	MemCoord result;
	if (simptemp[0][0] == -1) result = (MemCoord){-1u, -1u};
	else result = (MemCoord){round(simp[0][0]), round(simp[0][1])};
	return result;
}

void simplex(MemCoord* insimp, GeoCoord g, const GeoLocNCFile* const f, float alpha, float gamma, float rho, float sigma) {
	for (uint8_t i = 0; i < 3; i++) {
		printf("{%zu, %zu}\n", insimp[i].x, insimp[i].y);
		// if (i > 0 && insimp[i].x == insimp[0].x && insimp[i].y == insimp[0].y) return;
	}
	if (insimp[0].x == insimp[1].x 
		&& insimp[0].y == insimp[0].y 
		&& insimp[0].x == insimp[2].x
		&& insimp[0].y == insimp[2].y) {
		printf("converged to point\n");
		return;
	}
	// Setup
	
	GeoCoord gtemp;
	// could be combined with below
	gtemp = memToGeo(insimp[0], f);
	if (geoDist(&g, &gtemp) <= f->maxgeotomemerr) return;

	float fpoints[3];
	for (uint8_t i = 0; i < 3; i++) {
		gtemp = memToGeo(insimp[i], f);
		fpoints[i] = geoDist(&g, &gtemp);
	}
	// qsort(&fpoints[0], 3, sizeof(float), fcomp);
	float swaptemp;
	MemCoord memswaptemp;
	if (fpoints[0] > fpoints[1]) {
		swaptemp = fpoints[0];
		fpoints[0] = fpoints[1];
		fpoints[1] = swaptemp;
		memswaptemp = insimp[0];
		insimp[0] = insimp[1];
		insimp[1] = memswaptemp;
	}
	if (fpoints[2] < fpoints[1]) {
		swaptemp = fpoints[1];
		fpoints[1] = fpoints[2];
		fpoints[2] = swaptemp;
		memswaptemp = insimp[1];
		insimp[1] = insimp[2];
		insimp[2] = memswaptemp;
		if (fpoints[1] < fpoints[0]) {
			swaptemp = fpoints[0];
			fpoints[0] = fpoints[1];
			fpoints[1] = swaptemp;
			memswaptemp = insimp[0];
			insimp[0] = insimp[1];
			insimp[1] = memswaptemp;
		}
	}
	MemCoord centroid = (MemCoord){(int)insimp[0].y + ((int)insimp[1].y - (int)insimp[0].y) / 2, (int)insimp[0].x + ((int)insimp[1].x - (int)insimp[0].x) / 2};

	// Reflection
	// MemCoord refl = (MemCoord){centroid.y + alpha * sizeTypeSub(centroid.y, insimp[2].y), centroid.x + alpha * sizeTypeSub(centroid.x, insimp[2].x)};
	MemCoord refl = derivedVector(f, &centroid, alpha, &centroid, &insimp[2]);
	gtemp = memToGeo(refl, f);
	float frefl = geoDist(&g, &gtemp);
	if (fpoints[0] <= frefl && frefl <= fpoints[2]) {
		insimp[2] = refl;
		printf("reflecting\n");
		simplex(insimp, g, f, alpha, gamma, rho, sigma);
		return;
	}

	// Expansion
	if (fpoints[0] >= frefl) {
		// MemCoord exp = (MemCoord){centroid.y + gamma * sizeTypeSub(refl.y, centroid.y), centroid.x + gamma * sizeTypeSub(refl.x, centroid.x)};
		MemCoord exp = derivedVector(f, &centroid, gamma, &refl, &centroid);
		gtemp = memToGeo(exp, f);
		float fexp = geoDist(&g, &gtemp);
		printf("expanding\n");
		if (fexp < frefl) {
			insimp[2] = exp;
			simplex(insimp, g, f, alpha, gamma, rho, sigma);
			return;
		}
		else {
			insimp[2] = refl;
			simplex(insimp, g, f, alpha, gamma, rho, sigma);
			return;
		}
	}

	// Contraction
	if (frefl < fpoints[2]) {
		// MemCoord cont = (MemCoord){centroid.y + rho * sizeTypeSub(refl.y, centroid.y), centroid.x + rho * sizeTypeSub(refl.x, centroid.x)};
		MemCoord cont = derivedVector(f, &centroid, rho, &refl, &centroid);
		gtemp = memToGeo(cont, f);
		float fcont = geoDist(&g, &gtemp);
		if (fcont < frefl) {
			insimp[2] = cont;
			printf("outward contracting\n");
			simplex(insimp, g, f, alpha, gamma, rho, sigma);
			return;
		}
	} else {
		// MemCoord cont = (MemCoord){centroid.y + rho * sizeTypeSub(insimp[2].y, centroid.y), centroid.x + rho * sizeTypeSub(insimp[2].x, centroid.x)};
		MemCoord cont = derivedVector(f, &centroid, rho, &insimp[2], &centroid);
		gtemp = memToGeo(cont, f);
		float fcont = geoDist(&g, &gtemp);
		if (fcont < fpoints[2]) {
			insimp[2] = cont;
			printf("inward contracting\n");
			simplex(insimp, g, f, alpha, gamma, rho, sigma);
			return;
		}
	}

	// Shrink
	for (uint8_t i = 1; i < 3; i++) {
		insimp[i] = derivedVector(f, &insimp[0], sigma, &insimp[i], &insimp[0]);
		// insimp[i].x = insimp[0].x + sigma * sizeTypeSub(insimp[i].x, insimp[0].x);
		// insimp[i].y = insimp[0].y + sigma * sizeTypeSub(insimp[i].y, insimp[0].y);
	}
	printf("shrinking\n");
	simplex(insimp, g, f, alpha, gamma, rho, sigma);
	return;
}

void simplex2(float** insimp, GeoCoord g, const GeoLocNCFile* const f, float alpha, float gamma, float rho, float sigma, size_t depth) {
	// Edge Cases
	if (depth > SIMPLEX_MAX_DEPTH) {
		insimp[0][0] = -1;
		return;
	}
	/*
	if (insimp[0][1] == insimp[1][1] 
		&& insimp[0][0] == insimp[0][0] 
		&& insimp[0][1] == insimp[2][1]
		&& insimp[0][0] == insimp[2][0]) {
		printf("converged to point\n");
		return;
	}
	*/

	// Setup
	GeoCoord gtemp;
	float fpoints[3];
	for (uint8_t i = 0; i < 3; i++) {
		gtemp = subpixelMemToGeo(insimp[i], f);
		fpoints[i] = geoDist(&g, &gtemp);
	}
	float swaptemp;
	float memswaptemp[2];
	if (fpoints[0] > fpoints[1]) {
		swaptemp = fpoints[0];
		fpoints[0] = fpoints[1];
		fpoints[1] = swaptemp;
		memcpy(memswaptemp, insimp[0], 2 * sizeof(float));
		memcpy(insimp[0], insimp[1], 2 * sizeof(float));
		memcpy(insimp[1], memswaptemp, 2 * sizeof(float));
	}
	if (fpoints[2] < fpoints[1]) {
		swaptemp = fpoints[1];
		fpoints[1] = fpoints[2];
		fpoints[2] = swaptemp;
		memcpy(memswaptemp, insimp[1], 2 * sizeof(float));
		memcpy(insimp[1], insimp[2], 2 * sizeof(float));
		memcpy(insimp[2], memswaptemp, 2 * sizeof(float));
		if (fpoints[1] < fpoints[0]) {
			swaptemp = fpoints[0];
			fpoints[0] = fpoints[1];
			fpoints[1] = swaptemp;
			memcpy(memswaptemp, insimp[0], 2 * sizeof(float));
			memcpy(insimp[0], insimp[1], 2 * sizeof(float));
			memcpy(insimp[1], memswaptemp, 2 * sizeof(float));
		}
	}
	gtemp = subpixelMemToGeo(insimp[0], f);
	if (geoDist(&g, &gtemp) <= f->maxgeotomemerr) {
		printf("deph %zu\n", depth);
		return;
	}
	float centroid[2] = {insimp[0][0] + (insimp[1][0] - insimp[0][0]) / 2, insimp[0][1] + (insimp[1][1] - insimp[0][1]) / 2};

	// for (uint8_t i = 0; i < 3; i++) printf("[%d]: {%f, %f} => %f\n", (int)i, insimp[i][1], insimp[i][0], fpoints[i]);

	// Reflection
	float refl[2];
	derivedVector2(&refl[0], f, centroid, alpha, centroid, insimp[2]);
	gtemp = subpixelMemToGeo(refl, f);
	float frefl = geoDist(&g, &gtemp);
	if (fpoints[0] <= frefl && frefl < fpoints[1]) {
		memcpy(insimp[2], refl, 2 * sizeof(float));
#ifdef VERBOSE_GEOTOMEM2
		printf("reflecting\n");
#endif
		simplex2(insimp, g, f, alpha, gamma, rho, sigma, depth + 1);
		return;
	}

	// Expansion
	if (frefl < fpoints[0]) {
		float exp[2];
		derivedVector2(&exp[0], f, centroid, gamma, refl, centroid);
		gtemp = subpixelMemToGeo(exp, f);
		float fexp = geoDist(&g, &gtemp);
#ifdef VERBOSE_GEOTOMEM2
		printf("expanding\n");
#endif
		if (fexp < frefl) {
			memcpy(insimp[2], exp, 2 * sizeof(float));
			simplex2(insimp, g, f, alpha, gamma, rho, sigma, depth + 1);
			return;
		}
		else {
			memcpy(insimp[2], refl, 2 * sizeof(float));
			simplex2(insimp, g, f, alpha, gamma, rho, sigma, depth + 1);
			return;
		}
	}

	// Contraction
	if (frefl < fpoints[2]) {
		float cont[2];
		derivedVector2(&cont[0], f, centroid, rho, refl, centroid);
		gtemp = subpixelMemToGeo(cont, f);
		float fcont = geoDist(&g, &gtemp);
		if (fcont < frefl) {
			memcpy(insimp[2], cont, 2 * sizeof(float));
#ifdef VERBOSE_GEOTOMEM2
			printf("outward contracting\n");
#endif
			simplex2(insimp, g, f, alpha, gamma, rho, sigma, depth + 1);
			return;
		}
	} else {
		float cont[2];
		derivedVector2(&cont[0], f, centroid, rho, insimp[2], centroid);
		gtemp = subpixelMemToGeo(cont, f);
		float fcont = geoDist(&g, &gtemp);
		if (fcont < fpoints[2]) {
			memcpy(insimp[2], cont, 2 * sizeof(float));
#ifdef VERBOSE_GEOTOMEM2
			printf("inward contracting\n");
#endif
			simplex2(insimp, g, f, alpha, gamma, rho, sigma, depth + 1);
			return;
		}
	}

	// Shrink
	for (uint8_t i = 1; i < 3; i++) {
		derivedVector2(insimp[i], f, insimp[0], sigma, insimp[i], insimp[0]);
	}
#ifdef VERBOSE_GEOTOMEM2
	printf("shrinking\n");
#endif
	simplex2(insimp, g, f, alpha, gamma, rho, sigma, depth + 1);
	return;
}

MemCoord derivedVector(const GeoLocNCFile* const f, const MemCoord* const centroid, float m, const MemCoord* const v1, const MemCoord* const v2) {
	int xtemp = (int)centroid->x + m * ((int)v1->x - (int)v2->x);
	int ytemp = (int)centroid->y + m * ((int)v1->y - (int)v2->y);
	if (xtemp >= f->bounds.x) xtemp = f->bounds.x - 1;
	if (ytemp >= f->bounds.y) ytemp = f->bounds.y - 1;
	return (MemCoord){ytemp < 0 ? 0 : (size_t)ytemp, xtemp < 0 ? 0 : (size_t)xtemp};
}

void derivedVector2(float* dst, const GeoLocNCFile* const f, const float* const centroid, float m, const float* const v1, const float* const v2) {
	dst[0] = centroid[0] + m * (v1[0] - v2[0]);
	dst[1] = centroid[1] + m * (v1[1] - v2[1]);
	/*
	if (dst[0] > (float)f->bounds.y - 1) dst[0] = f->bounds.y - 1;
	else if (dst[0] < 0) dst[0] = 0;
	if (dst[1] > (float)f->bounds.x - 1) dst[1] = f->bounds.x - 1;
	else if (dst[1] < 0) dst[1] = 0;
	*/
}

size_t sizeTypeSub(size_t lhs, size_t rhs) {
	if (lhs < rhs) return 0;
	return lhs - rhs;
}

int fcomp(const void* lhs, const void* rhs) {
	float flhs = *((float*)lhs);
	float frhs = *((float*)rhs);
	if (flhs < frhs) return -1;
	if (flhs > frhs) return 1;
	else return 0;
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


