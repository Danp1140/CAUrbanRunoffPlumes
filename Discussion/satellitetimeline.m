START = datetime(1990, 1, 1);
PRESENT = datetime(2024, 1, 1);

% TODO: remove years variable
years = START : calendarDuration(1, 0, 0) :  PRESENT;
yearbounds = [START, PRESENT];

% SeaWiFS as per https://eospso.nasa.gov/missions/sea-viewing-wide-field-view-sensor
% AquaMODIS as per https://oceancolor.gsfc.nasa.gov/about/missions/aqua/
% ERS-1 & ERS-2 as per https://earth.esa.int/eogateway/missions/ers
% ENVISAT as per https://earth.esa.int/eogateway/missions/envisat
% SENTINEL-1 as per https://www.esa.int/Applications/Observing_the_Earth/Copernicus/Sentinel-1/Mission_ends_for_Copernicus_Sentinel-1B_satellite and https://www.esa.int/Applications/Observing_the_Earth/Copernicus/The_Sentinel_missions
% Sentinel-2 as per https://www.esa.int/Applications/Observing_the_Earth/Copernicus/Sentinel-2/Sentinel-2C_joins_the_Copernicus_family_in_orbit and https://www.esa.int/Applications/Observing_the_Earth/Copernicus/The_Sentinel_missions
% RADARSAT-1 as per https://www.asc-csa.gc.ca/eng/satellites/radarsat1/what-is-radarsat1.asp & https://space.oscar.wmo.int/satellites/view/radarsat_1
% RADARSAT-2 as per https://www.asc-csa.gc.ca/eng/satellites/radarsat2/what-is-radarsat2.asp
% RCM as per https://www.asc-csa.gc.ca/eng/satellites/radarsat/what-is-rcm.asp
timespanlist = {...
	[datetime(1997, 8, 1), datetime(2010, 12, 11)], ...
	[datetime(2002, 4, 5), PRESENT], ...
	[datetime(1991, 7, 17), datetime(2000, 3, 10)], ...
	[datetime(1995, 4, 21), datetime(2011, 9, 5)], ...
	[datetime(2002, 3, 1), datetime(2012, 4, 8)], ...
	[datetime(2014, 4, 3), datetime(2021, 12, 23)], ...
	[datetime(2015, 6, 23), PRESENT], ...
	[datetime(1995, 11, 4), datetime(2013, 3, 29)], ...
	[datetime(2007, 12, 14), PRESENT], ...
	[datetime(2019, 6, 12), PRESENT]};
namelist = ["SeaWiFS", "AquaMODIS", "ERS-1", "ERS-2", "ENVISAT", "Sentinel-1(A & B)", ...
	"Sentinel-2(A, B, & C)", "RADARSAT-1", "RADARSAT-2", "RCM"];
typelist = ["OC", "OC", "SAR", "SAR", "SAR", "SAR", "OC", "SAR", "SAR", "SAR"];

lastamerican = 2.5;
lasteuropean = 7.5;
lastcanadian = 10.5;

hold on
for i = 1 : length(timespanlist)
	if typelist(i) == "OC"
		color = '#0072BD';
	else 
		color = '#EDB120'; 
	end
	p = plot(timespanlist{i}, i * ones(2), 'LineWidth', 15, 'Color', color);
	if typelist(i) == "OC" && exist("firstoc") ~= 1
		firstoc = p;
	elseif typelist(i) == "SAR" && exist("firstsar") ~= 1
		firstsar = p;
	end
	text(timespanlist{i}(1), i, namelist(i), 'Color', 'w')
end
plot(yearbounds, lastamerican * ones(2), 'Color', 'k')
plot(yearbounds, lasteuropean * ones(2), 'Color', 'k')
hold off
xlabel("Year")
xlim(yearbounds)
datetick('x', "'yy")

ylim([0, lastcanadian])
yticks([lastamerican / 2, ...
	lastamerican + (lasteuropean - lastamerican) / 2, ...
	lasteuropean + (lastcanadian - lasteuropean) / 2])
set(gca,'YTickLabel',["American", "European", "Canadian"]);

legend([firstoc(1), firstsar(1)], "Ocean Color", "Synthetic Aperture Radar", 'Location', 'northwest')


