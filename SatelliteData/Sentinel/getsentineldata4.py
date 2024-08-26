import openeo
import json
import datetime

lariver = (33.755, -118.185)
sgriver = (33.74, -118.115)
sariver = (33.63, -117.96)
bcreek = (33.96, -118.46)

SQUARE_WIDTH = 0.2 # in degrees
RESIDUAL_DAYS = 30 # download this many extra days after rainy day
# RESIDUAL_DAYS extra long in case we dont have super recent Sentinel coverage

# gives a generous spatial extent for load_collection based on SQUARE_WIDTH
def generateSpatialExt(center):
    return {'west': center[1] - SQUARE_WIDTH,
            'south': center[0] - SQUARE_WIDTH,
            'east': center[1] + SQUARE_WIDTH,
            'north': center[0] + SQUARE_WIDTH}

def generateTemporalExt(year, month, day):
    return str(year) + "-" + str(month).zfill(2) + "-" + str(day).zfill(2);

def getDataCube(connection, name, bands, year, month, day, site):
    start = datetime.datetime(year, month, day)
    end = start + datetime.timedelta(days=RESIDUAL_DAYS)
    cube = connection.load_collection(
            name,
            spatial_extent=generateSpatialExt(site),
            temporal_extent=[generateTemporalExt(year, month, day),
                             generateTemporalExt(end.year, end.month, end.day)],
            bands=bands)
    if name == "SENTINEL1_GRD":
        print("sentinel1")
        cube = cube.sar_backscatter(coefficient='sigma0-ellipsoid')
    cube = cube.filter_bbox(bbox=generateSpatialExt(site))
    cube = cube.resample_spatial(resolution=0, projection=3426)
    # cube = openeo.processes.vector_reproject(cube, projection=3426)
    print(name)
    print(generateSpatialExt(site))
    print(generateTemporalExt(year, month, day))
    print(generateTemporalExt(end.year, end.month, end.day))
    print(bands)
    return cube
    

con = openeo.connect("openeo.dataspace.copernicus.eu")

print(con.list_collection_ids())

con.authenticate_oidc()

# sentinel 1 seems to be the only one with good sar???
# print(json.dumps(con.describe_collection("SENTINEL1_GRD"), indent=2))

# trial download, real download should be different bands and in more specific polys due to resolution
"""
dc = con.load_collection(
    "SENTINEL2_L1C",
    spatial_extent={'west': -121, 'south': 32, 'east': -116, 'north': 35},
    temporal_extent=["2020-01-01", "2020-01-03"],
    bands=["B10", "B11", "B12"]
)

print(dc)
"""

# coastal ultrablue, near IR, & highest wavelength IR
# dc = getDataCube(con, "SENTINEL2_L2A", ["B01", "B08", "B12"], 2020, 1, 1, lariver)
dc = getDataCube(con, "SENTINEL1_GRD", ["VV", "VH"], 2016, 1, 1, lariver)

result = dc.save_result(format='netCDF')

job = result.create_job()

job.start_and_wait()

#job = result.execute_batch()

job.get_results().download_files("test")
