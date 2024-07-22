import openeo
import json

con = openeo.connect("openeo.dataspace.copernicus.eu")

print(con.list_collection_ids())

con.authenticate_oidc()

print(json.dumps(con.describe_collection("SENTINEL1_GRD"), indent=2))


dc = con.load_collection(
    "SENTINEL2_L1C",
    spatial_extent={'west': -121, 'south': 32, 'east': -116, 'north': 35},
    # spatial_extent={'west': -119.1, 'south': 33.9, 'east': -119, 'north': 34},
    temporal_extent=["2020-01-01", "2020-01-03"],
    bands=["B10", "B11", "B12"]
)

print(dc)

result = dc.save_result(format='netCDF')
# result = dc.save_result(format='GTiff')

job = result.create_job()

job.start_and_wait()

job.get_results().download_files("test", name="openeotest.nc")

# dc.download("test.nc")
