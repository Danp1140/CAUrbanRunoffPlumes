% what intermediate data should we bother showing?
% how busy is too busy?

studyyears = 2004 : 2022;

rain = readtimetable("../SatelliteData/AquaMODIS/LAXPrcpDataTemp.csv");
rain.DATE = rain.DATE + years(2000); % 2-digit year in csv is misinterpreted

for i = length(rain.DATE) : -1 : 1
	if (year(rain.DATE(i)) < 2004) || (year(rain.DATE(i)) > 2022)
		rain(i, :) = [];
	end
end

numrains = retime(rain(:, "PRCP"), 'yearly', @length);
rainamts = retime(rain(:, "PRCP"), 'yearly', @mean);
meanrain = mean(rain.PRCP);

sites = {
	readtimetable("../Processing/Data/LARiverBackup8-30.csv")
	readtimetable("../Processing/Data/SGRiverBackup8-30.csv")
	readtimetable("../Processing/Data/SARiverBackup8-30.csv")
	readtimetable("../Processing/Data/BCreekBackup8-30.csv")
};

colors = [
	"#0072BD"
	"#D95319"
	"#EDB120"
	"#7E2F8E"
];

numplumes{4} = [];
sizeavgs{4} = [];
sizemedians{4} = [];
sizestds{4} = [];
sizestderrs{4} = [];
intavgs{4} = [];
intmedians{4} = [];
intstds{4} = [];
intstderrs{4} = [];
for i = 1 : length(sites)
	numplumes{i} = retime(sites{i}(:, "MYD09GAArea_km_2_"), 'yearly', @length).MYD09GAArea_km_2_;
	sizeavgs{i} = retime(sites{i}(:, "MYD09GAArea_km_2_"), 'yearly', @mean).MYD09GAArea_km_2_;
	sizemedians{i} = retime(sites{i}(:, "MYD09GAArea_km_2_"), 'yearly', @median).MYD09GAArea_km_2_;
	sizestds{i} = retime(sites{i}(:, "MYD09GAArea_km_2_"), 'yearly', @std).MYD09GAArea_km_2_;
	sizestderrs{i} = sizestds{i} ./ sqrt(numplumes{i});
	intavgs{i} = retime(sites{i}(:, "MYD09GAAvg_Intensity_refl_km_2_"), 'yearly', @mean).MYD09GAAvg_Intensity_refl_km_2_;
	intmedians{i} = retime(sites{i}(:, "MYD09GAAvg_Intensity_refl_km_2_"), 'yearly', @median).MYD09GAAvg_Intensity_refl_km_2_;
	intstds{i} = retime(sites{i}(:, "MYD09GAAvg_Intensity_refl_km_2_"), 'yearly', @std).MYD09GAAvg_Intensity_refl_km_2_;
	intstderrs{i} = intstds{i} ./ sqrt(numplumes{i});
end

%% Yearly Average Size
% figure
% hold on
% for i = 1 : length(sites)
% 	errorbar(studyyears, sizeavgs{i}, sizestderrs{i}, 'LineWidth', 1, 'Color', colors(i))
% end
% hold off
% legend("Los Angeles River", "San Gabriel River", "Santa Ana River", "Ballona Creek")
% xlabel("Year")
% ylabel("Average Plume Size (km^2)")
% title("Yearly Average Plume Sizes")

%% Yearly Average Intensity
% figure
% hold on
% for i = 1 : length(sites)
% 	errorbar(studyyears, intavgs{i}, intstderrs{i}, 'LineWidth', 1, 'Color', colors(i))
% end
% hold off
% legend("Los Angeles River", "San Gabriel River", "Santa Ana River", "Ballona Creek")
% xlabel("Year")
% ylabel("Average Plume Intensity (refl/km^2)")
% title("Yearly Average Plume Intensities")

%% Yearly Average Rain Amount Versus Number of Events (Line Plot)
% figure
% hold on
% plot(year(rainamts.DATE), rainamts.PRCP, 'LineWidth', 1, 'Color', 'k')
% for i = 1 : length(sites)
% 	plot(studyyears, numplumes{i}, 'LineWidth', 1, 'Color', colors(i))
% end
% hold off
% legend("Yearly Avg Rainfall from Events >6mm", ...
% 	"Los Angeles River", "San Gabriel River", "Santa Ana River", "Ballona Creek")
% xlabel("Year")
% ylabel("Count")
% title("Count of Rain Events and Detected Plumes")

%% Rain Amount Versus Average Plume Size (Scatter Plot)
figure
hold on
for i = 1 : length(sites)
    m = fitlm(rainamts.PRCP, sizeavgs{i});
    r2 = m.Rsquared.Ordinary;
    [rho, pval] = corr(rainamts.PRCP, sizeavgs{i});
    plot(rainamts.PRCP, sizeavgs{i}, 'o', 'MarkerFaceColor', colors(i))
    a = sort(rainamts.PRCP, 1, 'ascend');
    p = plot([0, a(end)], ...
        [m.Coefficients.Estimate(1), m.Coefficients.Estimate(1) + m.Coefficients.Estimate(2) * a(end)], ...
        'Color', colors(i));
    text(p.XData(end), p.YData(end), "r^2 = " + r2 + newline + "\rho = " + rho);
end
hold off

%% Adjusted for # eventso
% maxnumrains = max(numrains).(1);
% normsizeavgs = {};
% normintavgs = {};
% figure
% hold on
% for i = 1 : length(sites)
% 	normsizeavgs{i} = sizeavgs{i} ./ numrains.PRCP * maxnumrains;
% 	normintavgs{i} = intavgs{i} ./ numrains.PRCP * maxnumrains;
% 	yyaxis("left")
% 	plot(studyyears, normsizeavgs{i}, "-", 'LineWidth', 1, 'Color', colors(i))
% 	yyaxis("right")
% 	plot(studyyears, normintavgs{i}, "--", 'LineWidth', 1, 'Color', colors(i))
% end
% hold off
% xlabel("Year")
% yyaxis("left")
% ylabel("Average Plume Size (km^2)")
% yyaxis("right")
% ylabel("Average Plume Intensity (refl/km^2)")
% title("Count-adjusted yearly average plume sizes & intensities")

%% Adjusted for amount of rain (yearly avg)
normamtsizeavgs{4} = [];
normamtintavgs{4} = [];
figure
tiledlayout(2, 1)
nexttile
hold on
for i = 1 : length(sites)
    normamtsizeavgs{i} = sizeavgs{i} ./ rainamts.PRCP;
    errorbar(studyyears, normamtsizeavgs{i}, sizestderrs{i} ./ rainamts.PRCP, 'LineWidth', 1, 'Color', colors(i))
end
hold off
title("Normalized yearly average plume sizes & intensities")
legend(["Los Angeles River", "San Gabriel River", "Santa Ana River", "Ballona Creek"], ...
    'Location', 'northeast')
ylabel("ize (km^2/mm)")
set(gca, 'XTick', [])
nexttile
hold on
for i = 1 : length(sites) 
    normamtintavgs{i} = intavgs{i} ./ rainamts.PRCP;
    errorbar(studyyears, normamtintavgs{i}, intstderrs{i} ./ rainamts.PRCP, 'LineWidth', 1, 'Color', colors(i))
end
hold off
xlabel("Year")
ylabel("Intensity (refl/(km^2*mm))")


%% T-Test for 2013 Spike Significance
% Note: Below is just for site 1 rn
fprintf("2013 anomaly stats\n")
for i = 1 : length(sites) 
    fprintf("site %d", i)
    pop1 = [];
    for j = 1 : length(sites{i}.Date)
        if year(sites{i}.Date(j)) < 2013
            pop1 = [pop1, sites{i}.MYD09GAArea_km_2_(j)];
        end
    end
    pop2 = [];
    for j = 1 : length(sites{i}.Date)
        if year(sites{i}.Date(j)) > 2013
            pop2 = [pop2, sites{i}.MYD09GAArea_km_2_(j)];
        end
    end
    xbar = mean(pop2) - mean(pop1);
    se = sqrt((std(pop1) / sqrt(length(pop1))) ^ 2 + (std(pop2) / sqrt(length(pop2))) ^ 2);
    z = xbar / se;
    [tval, pval] = ttest2(pop1, pop2);
    fprintf("\tz-score: %1.4f\n", z)
    fprintf("\tp from z-score: %1.4f\n", 2 * (1 - normcdf(z)))
    fprintf("\tp from t test: %1.4f\n", pval)
end
% temp = sites{1};
% pop1 = [];
% for i = 1 : length(temp.Date)
%     if year(temp.Date(i)) < 2013
%         pop1 = [pop1, temp.MYD09GAArea_km_2_(i)];
%     end
% end
% pop2 = [];
% temp = sites{1};
% for i = 1 : length(temp.Date)
%     if year(temp.Date(i)) > 2013
%         pop2 = [pop2, temp.MYD09GAArea_km_2_(i)];
%     end
% end
% xbar = mean(pop2) - mean(pop1)
% se = sqrt(std(pop1) / sqrt(length(pop1)) + std(pop2) / sqrt(length(pop2)))
% z = xbar / se;
% [tval, pval] = ttest2(pop1, pop2);
% fprintf("z-score of 2013 anomaly: %d\n", z)
% fprintf("p value from that: %d\n", 2 * (1 - normcdf(z)))
% fprintf("p value from t test: %d\n", pval)

yearpop1 = normamtsizeavgs{1}(1 : 9);
figure
arimam = arima(1, 0, 0);
em = estimate(arimam, yearpop1);
summarize(em);
f = filter(em, normamtsizeavgs{1});
sim = simulate(em, length(yearpop1) + 10, 'NumPaths', 20);
hold on
plot(1 : length(normamtsizeavgs{1}), normamtsizeavgs{1}, 'LineWidth', 2)
plot(1 : length(f), normamtsizeavgs{1} - f, 'LineWidth', 2)
% plot(1 : (length(yearpop1) + 10), sim)
hold off
