import csv
import re
import datetime
import subprocess

def processForRainDaysPlusN(n):
    processed = []
    log = open("process-log-" + str(datetime.datetime.now()) + ".txt", 'w')
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
                    subprocess.run(["./QuantifyPlumes", str(di.year), str(di.month), str(di.day)], stdout=log)
    log.close()

subprocess.run(["rm", "Data/LARiver.csv"])
subprocess.run(["rm", "Data/SGRiver.csv"])
subprocess.run(["rm", "Data/SARiver.csv"])
subprocess.run(["rm", "Data/BCreek.csv"])
with open("Data/LARiver.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (m^2)", "MYD09GA Avg. Intensity (refl/m^2)"])
with open("Data/SGRiver.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (m^2)", "MYD09GA Avg. Intensity (refl/m^2)"])
with open("Data/SARiver.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (m^2)", "MYD09GA Avg. Intensity (refl/m^2)"])
with open("Data/BCreek.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (m^2)", "MYD09GA Avg. Intensity (refl/m^2)"])
subprocess.run(["rm", "-r", "2004/"])
subprocess.run(["mkdir", "2004"])
processForRainDaysPlusN(3)
