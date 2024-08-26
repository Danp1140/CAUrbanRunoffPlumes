from osgeo import (gdal, osr)
import numpy

gdal.UseExceptions()
src = gdal.Open("/Users/danp/Downloads/ASA_IMP_1PNESA20051017_175915_000000182041_00399_18990_0000.N1")

band = src.GetRasterBand(1)
array = band.ReadAsArray()
[rows, cols] = array.shape
dst = gdal.GetDriverByName("NetCDF").Create("test.nc", cols, rows, 1, gdal.GDT_UInt16) 
lat2 = 32.430007 
lon2 = -116.834847
lat1 = 31.544908
lon1 = -118.202463
firstnear = [int(src.GetMetadata()['SPH_FIRST_NEAR_LAT']) / 1000000, int(src.GetMetadata()['SPH_FIRST_NEAR_LONG']) / 1000000];
firstfar = [int(src.GetMetadata()['SPH_FIRST_FAR_LAT']) / 1000000, int(src.GetMetadata()['SPH_FIRST_FAR_LONG']) / 1000000];
lastnear = [int(src.GetMetadata()['SPH_LAST_NEAR_LAT']) / 1000000, int(src.GetMetadata()['SPH_LAST_NEAR_LONG']) / 1000000];
lastfar = [int(src.GetMetadata()['SPH_LAST_FAR_LAT']) / 1000000, int(src.GetMetadata()['SPH_LAST_FAR_LONG']) / 1000000];
print(firstnear)
print(firstfar)
print(lastnear)
print(lastfar)
firstnear, lastfar = lastfar, firstnear
firstfar, lastnear = lastnear, firstfar
# dst.SetGeoTransform((lon1, (lon2 - lon1) / rows, 0, lat2, 0, (lat1 - lat2) / cols));
theta = numpy.arctan(abs(firstnear[1] - firstfar[1]) / abs(firstnear[0] - firstfar[0]))
width = numpy.sqrt((firstnear[0] - firstfar[0]) ** 2 + (firstnear[1] - firstfar[1]) ** 2) / rows 
height = -numpy.sqrt((firstnear[0] - lastnear[0]) ** 2 + (firstnear[1] - lastnear[1]) ** 2) / cols
width = abs(firstnear[1] - lastfar[1]) / rows
height = -abs(firstnear[0] - lastfar[0]) / cols
print(width)
print(height)
print(theta)
"""
dst.SetGeoTransform((
    firstfar[1], 
    numpy.cos(theta) * width, 
    -numpy.sin(theta) * width, 
    firstfar[0], 
    numpy.sin(theta) * height, 
    numpy.cos(theta) * height))
    """
"""
dst.SetGeoTransform((
    firstfar[1], 
    numpy.cos(theta) * width, 
    -numpy.sin(theta) * width, 
    firstfar[0], 
    -numpy.cos(90 - theta) * height, 
    -numpy.sin(90 - theta) * height))
    """
dst.SetGeoTransform((
    firstnear[1],
    abs(lastfar[1] - firstnear[1]) / rows,
    numpy.sqrt((firstnear[0] - lastfar[0]) ** 2 + (firstnear[1] - lastfar[1]) ** 2) / 9799,
    firstnear[0],
    numpy.sqrt((firstnear[0] - lastfar[0]) ** 2 + (firstnear[1] - lastfar[1]) ** 2) / 8481,
    abs(lastfar[0] - firstnear[0]) / cols))
print("Our GeoTransform:")
print(dst.GetGeoTransform())
# print(src.GetGCPs())
spatialref = osr.SpatialReference()
spatialref.SetWellKnownGeogCS('WGS84')
dst.SetProjection(spatialref.ExportToWkt())
# dst.SetProjection(src.GetProjection())
dst.GetRasterBand(1).WriteArray(array)
dst.FlushCache()
