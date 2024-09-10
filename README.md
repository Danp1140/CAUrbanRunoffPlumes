# 

## Description

This repository contains Python scripts and C source code used to collect and analyze satellite data to study pollution plumes caused by urban runoff during and after significant rain events.

Check out the issues tab to see current areas of development!

## Building

If you wish to use these tools yourself, you will need to make some alterations after cloning the repository and compile the parts written in C. While relative filepaths within the repository are used wherever possible, there are absolute filepaths in `getdata2.py`, `processall.py`, `QuantifyPlumes.c`, and `NCHelpers.h`. While these should get phased out sometime in the future, for now they must be changed to reflect the correct locations for your machine. 

Once those changes have been made, use your C compiler of choice to compile `NCHelpers.c` and `QuantifyPlumes.c`. Be sure to link your intermediate `NCHelpers.o` as well as `libnetcdf` and `libcurl` when compiling the final executable!

Here is an example build script:

	gcc NCHelpers.c -o build/NCHelpers.o -c
	gcc QuantifyPlumes.c -o build/QuantifyPlumes.o -c
	gcc -o QuantifyPlumes build/NCHelpers.o build/QuantifyPlumes.o -l netcdf -L/usr/local/lib -lcurl

## Overview of Structure

These tools were designed to run using minimal resources (read: a 2019 MacBook Pro with 40 GB of disk space and a slow network). As a result, every year is broken up into a download phase and an analysis phase. For each year, the data are downloaded, then analyized, then deleted so as to not exceed the machine's disk space. While one year is being downloaded, the last year is being simultaneously analyzed, as the tasks exert loads on different parts of the system.

Notably, cloud masks from the MYDATML2 data set, as well as land/sea masks, are pre-downloaded. This is because they are significantly smaller files and they are needed for all analysis.

`Processing/processall.py` coordinates the download and analysis tasks. 

### Downloading

To download a year, `processall.py` executes `downloadInBackground` which in turn executes Python scripts for each satellite dataset as concurrent subprocesses. The only such script currently written is `SatelliteData/AquaMODISdownloadMYD09GA.py`, but the system is intended for ease of expansion. Download scripts may be self-contained, but `downloadMYD09GA.py` uses code imported from `SatelliteData/AquaMODISgetdata2.py`, which contains various functions and definitions useful for downloading AquaMODIS data (particularly from LAADS).

While that data is downloading, if a previous year has already been downloaded `processall.py` calls `Processing/QuantifyPlumes`. More details on this executable further down. After `QuantifyPlumes` has finished executing, the files for the year it just analyzed are deleted. Before proceeding to the next year, `processall.py` waits for both the download and analysis steps to finish.

### Analysis

`QuantifyPlumes` is an executable compiled from `QuantifyPlumes.c`, `NCHelpers.h`, and `NCHelpers.c`. These tools were written in C to allow for greater performance optimization since they require a large number of operations and iterations. `QuantifyPlumes.c` contains the `main` function and calls utility functions from the `NCHelpers.h` header. It begins by opening all of the NetCDF files it will need for analysis–the land/sea mask for each study site, the cloud mask for the day, as well as all ocean color or SAR datasets for the day. It also opens `Processing/useless.txt`. This file is used to keep track of which satellite images do not contain any meaningful part of any study site, since download queries have limited tools for filtering downloads by area. The files in `useless.txt` can be referenced during the download phase (as in `getdata2.py`'s `downloadLAADSForDay`) to ensure such files are not erroneously downloaded in the future. Finally, `QuantifyPlumes.c` opens up a CSV file for each site, in which it will store the total area of the detected plume, as well as the average intensity.

