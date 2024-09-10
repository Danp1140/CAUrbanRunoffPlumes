from getdata2 import (execForRainDaysPlusN, downloadLAADSForDay, MYD09GAAPIURL, MYD09GAFolderPrefix)
import sys

execForRainDaysPlusN(3, int(sys.argv[1]), lambda y, m, d : downloadLAADSForDay(y, m, d, MYD09GAAPIURL, "/Users/danp/Desktop/CAUrbanRunoffPlumes/SatelliteData/AquaMODIS/" + MYD09GAFolderPrefix, "Latitude_1", "Longitude_1", "sur_refl_b04_1"))
