import csv
import re
from datetime import (datetime, timedelta)

# downloading day of plus 3
def getInRangeDownloadLinks(pathtoraindata, pathtoasarindex):
    inrange = []
    with open(pathtoasarindex) as i:    
        r = csv.reader(i, delimiter='\t')
        counter = 0
        startdateidx = -1
        uriidx = -1
        for col in next(r):
            if col == "beginAcquisition":
                startdateidx = counter
            elif col == "productURI":
                uriidx = counter
            counter += 1

        for row in r:
            realrow = []
            for col in row:
                realrow += re.split('\t', col)
            d = datetime.strptime(realrow[startdateidx], "%Y-%m-%dT%H:%M:%S.%fZ")
            with open(pathtoraindata) as p:
                preader = csv.reader(p)
                next(preader)
                for prow in preader:
                    d2 = datetime.strptime(prow[0], "%m/%d/%y")
                    for offset in range (0, 3):
                        if d.date() == d2.date():
                            inrange.append(realrow[uriidx])
                        d2 += timedelta(days=1)


    return inrange


asdf = getInRangeDownloadLinks("../AquaMODIS/LAXPrcpDataTemp.csv", "socat_search_result_metadata_20240820-174907.index");

print('\n'.join(asdf))
