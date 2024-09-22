import csv
import re
import datetime
import subprocess

def processForRainDaysPlusN(n, year):
    print("processing for " + str(year))
    processed = []
    # TODO: redo process log name
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
            d = datetime.datetime(int(x[2]), int(x[0]), int(x[1])) 
            if d.year < year:
                continue
            if d.year > year:
                break
            for i in range(0, n):
                di = d + datetime.timedelta(days=i)
                if di not in processed:
                    processed.append(di)
                    print("Processing " + str(di))
                    subprocess.run(["./QuantifyPlumes", str(di.year), str(di.month), str(di.day)], stdout=log)
    log.close()

def downloadInBackground(year):
    downloads = []
    print("downloading MYD09GA for " + str(year))
    downloads.append(subprocess.Popen(["/opt/miniconda3/bin/python", "../SatelliteData/AquaMODIS/downloadMYD09GA.py", str(year)]))
    return downloads

def deleteFiles(year):
    print("deleting MYD09GA for " + str(year))
    subprocess.run(["rm", "-r", "../SatelliteData/AquaMODIS/MYD09GA/" + str(year)])

"""
subprocess.run(["rm", "Data/LARiver.csv"])
subprocess.run(["rm", "Data/SGRiver.csv"])
subprocess.run(["rm", "Data/SARiver.csv"])
subprocess.run(["rm", "Data/BCreek.csv"])
with open("Data/LARiver.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (km^2)", "MYD09GA Avg. Intensity (refl/km^2)"])
with open("Data/SGRiver.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (km^2)", "MYD09GA Avg. Intensity (refl/km^2)"])
with open("Data/SARiver.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (km^2)", "MYD09GA Avg. Intensity (refl/km^2)"])
with open("Data/BCreek.csv", 'w') as f:
    csv.writer(f).writerow(["Date", "MYD09GA Area (km^2)", "MYD09GA Avg. Intensity (refl/km^2)"])
    """

"""
for y in range(2011, 2024):
    downloads = downloadInBackground(y) 
    if y != 2004:
        processForRainDaysPlusN(3, y - 1)
        deleteFiles(y - 1)
    # wait on download to finish before advancing to download next year and process just downloaded
    for d in downloads:
        d.communicate()
        """

processForRainDaysPlusN(3, 2023)
