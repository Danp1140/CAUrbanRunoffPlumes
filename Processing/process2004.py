import csv
import re
import datetime
import subprocess

def processForRainDaysPlusN(n):
    processed = []
    log = open("process-log-" + str(datetime.datetime.now()), 'w')
    with open("../SatelliteData/AquaMODIS/LAXPrcpDataTemp.csv") as csvfile:
        for row in csv.reader(csvfile):
            x = re.split("/", row[0])
            if len(x) < 3:
                continue
            if len(x[0]) == 1:
                x[0] = "0" + x[0]
            if len(x[1]) == 1:
                x[1] = "0" + x[1]
            if len(x[2]) == 2:
                x[2] = "20" + x[2]
            if x[2] != "2004":
                continue;
            d = datetime.datetime(int(x[2]), int(x[0]), int(x[1])) 
            for i in range(0, n):
                di = d + datetime.timedelta(days=i)
                if di not in processed:
                    processed.append(di)
                    print("Processing " + str(di))
                    subprocess.run(["./QuantifyPlumes", x[2], x[0], x[1]], stdout=log)
    log.close()

processForRainDaysPlusN(1)
