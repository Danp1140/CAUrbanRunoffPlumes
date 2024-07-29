import urllib.request
import json
import datetime
import csv
import re
import os

cloudAPIURL = "https://ladsweb.modaps.eosdis.nasa.gov/api/v1/files/product=MYDATML2&collection=61&areaOfInterest=x-121y32,x-116y35&dateRanges="
cloudFolderPrefix = "MYDATML2/"
downloadURLPrefix = "https://ladsweb.modaps.eosdis.nasa.gov/opendap/hyrax/RemoteResources/laads/"

L3URLPrefix = "https://oceandata.sci.gsfc.nasa.gov/opendap/MODISA/L3SMI/"
L3FolderPrefix = "L3SMI/"

MYD09GAAPIURL = "https://ladsweb.modaps.eosdis.nasa.gov/api/v1/files/product=MYD09GA&collection=61&areaOfInterest=x-121y32,x-116y35&dateRanges=" 
MYD09GAFolderPrefix = "MYD09GA/"

def addDateRange(year, month, day):
    return cloudAPIURL + str(year) + "-" + str(month).zfill(2) + "-" + str(day).zfill(2)

def addDateRangeToAPIURL(url, year, month, day):
    return url + str(year) + "-" + str(month).zfill(2) + "-" + str(day).zfill(2)

def getDownloadURL(apiRelURL):
    return downloadURLPrefix + apiRelURL[9:] + ".nc4?Latitude,Longitude,Cloud_Mask"

def getMYD09GADownloadURL(apiRelURL):
    """
    return urllib.parse.quote(downloadURLPrefix + apiRelURL[9:] + ".nc4?dap4.ce=/Latitude[0:1:1199][0:1:1199];/Longitude[0:1:1199][0:1:1199];/sur_refl_b04_1[0:1:2399][0:1:2399]", safe="=?/:;")
    """
    return downloadURLPrefix + apiRelURL[9:] + ".nc4?Latitude_1,Longitude_1,sur_refl_b04_1"

def getL3DownloadURL(year, month, day):
    return L3URLPrefix + str(year) + "/" + str(month).zfill(2) + str(day).zfill(2) + "/AQUA_MODIS." + str(year) + str(month).zfill(2) + str(day).zfill(2) + ".L3m.DAY.RRS.Rrs_555.4km.nc.dap.nc4" 

def logError(e):
    with open("errorlog.txt", 'a') as log:
        log.write(e + "\n")

def logHTTPError(e):
    with open("errorlog.txt", 'a') as log:
        log.write("HTTP Error " + str(e.code) + " when accessing " + e.url + "\n")
        log.write("Reason: " + e.reason + "\n")
        log.write("Headers: " + str(e.headers) + "\n")

def downloadATML2ForDay(year, month, day):
    try:
        response = urllib.request.urlopen(addDateRange(year, month, day))
        print(addDateRange(year, month, day))
    except urllib.error.HTTPError as err:
        logHTTPError(err)   
        return
    try:
        jsonresp = json.loads(response.read().decode())
    except:
        logError(str(datetime.datetime.now()) + " | JSON didn't work out, probably got an empty response back\nWas working on " + str(year) + "/" + str(month) + "/" + str(day) + "\nFrom decoded response:\n"
                 + response.read().decode() + "\n")
        return
    for key in jsonresp:
        print(str(datetime.datetime.now()) + " | Downloading " + getDownloadURL(jsonresp[key]["fileURL"]))
        try:
            binresp = urllib.request.urlopen(getDownloadURL(jsonresp[key]["fileURL"]))
        except urllib.error.HTTPError as err:
            logHTTPError(err)
            continue
        filepath = cloudFolderPrefix + str(year) + "/" + str(month) + "/" + str(day) + "/" + jsonresp[key]["name"] + ".nc4"
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, 'wb') as output:
            output.write(binresp.read()) 

def downloadMYD09GAForDay(year, month, day):
    try:
        response = urllib.request.urlopen(addDateRangeToAPIURL(MYD09GAAPIURL, year, month, day))
    except urllib.error.HTTPError as err:
        logHTTPError(err)   
        return
    try:
        jsonresp = json.loads(response.read().decode())
    except:
        logError(str(datetime.datetime.now()) + " | JSON didn't work out, probably got an empty response back\nWas working on " + str(year) + "/" + str(month) + "/" + str(day) + "\nFrom decoded response:\n"
                 + response.read().decode() + "\n")
    for key in jsonresp:
        print(getMYD09GADownloadURL(jsonresp[key]["fileURL"]))
        try:
            binresp = urllib.request.urlopen(getMYD09GADownloadURL(jsonresp[key]["fileURL"]))
        except urllib.error.HTTPError as err:
            logHTTPError(err)
            continue
        filepath = MYD09GAFolderPrefix + str(year) + "/" + str(month) + "/" + str(day) + "/" + jsonresp[key]["name"] + ".nc4"
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, 'wb') as output:
            output.write(binresp.read()) 



def downloadL3SMIForDay(year, month, day):
    url = getL3DownloadURL(year, month, day);
    try:
        response = urllib.request.urlopen(url)
        print(url)
    except urllib.error.HTTPError as err:
        logHTTPError(err)
        return
    filepath = L3FolderPrefix + str(year) + "/" + str(month) + "/" + str(day) + "/" + url[(len(L3URLPrefix) + 5 + 5):]
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'wb') as output:
        output.write(response.read())


def downloadForRainDays():
    with open("LAXPrcpDataTemp.csv") as csvfile:
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
            # downloadATML2ForDay(int(x[2]), int(x[0]), int(x[1]))
            downloadL3SMIForDay(int(x[2]), int(x[0]), int(x[1]))

# downloads not just rain days but n days after any rain day
def downloadForRainDaysPlusN(n):
    downloaded = []
    with open("LAXPrcpDataTemp.csv") as csvfile:
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
            for i in range(0, n):
                di = d + datetime.timedelta(days=i)
                if di not in downloaded:
                    downloaded.append(di)
                    downloadMYD09GAForDay(di.year, di.month, di.day)


# downloadForRainDays()

# downloadMYD09GAForDay(2004, 1, 1)
downloadForRainDaysPlusN(3)
