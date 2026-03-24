#include <vector>
#include <string>
#include <fstream>
#include <iostream>

class CashFlowAnalysis {
private:
    std::vector<double> metrics;
    double totalRevenue;
    double totalOpex;
    double avgAnnualCashFlow;
    int projectLife;

public:
    CashFlowAnalysis() : totalRevenue(0), totalOpex(0), avgAnnualCashFlow(0), projectLife(0) {
        metrics.resize(4); // Assuming metrics stores projectLife, totalRevenue, totalOpex, avgAnnualCashFlow
    }

    void generateCashFlow(const std::vector<Phase>& phases, const EconomicParameters& params, double discountRate, double capex = 0) {
        double cumulativeNPV = -capex;
        std::vector<double> yearlyCashFlow;
        int year = 0;

        for (const auto& phase : phases) {
            double oreProduced = phase.oreTonnage;
            double metalProduced = oreProduced * phase.grade * phase.recovery / 100;
            double revenue = metalProduced * params.price;
            double miningCost = (oreProduced + phase.waste) * params.costPerTonne;
            double processingCost = oreProduced * params.processCost;
            double operatingCashFlow = revenue - (miningCost + processingCost);

            double discountFactor = 1 / std::pow(1 + discountRate, year);
            double discountedCashFlow = operatingCashFlow * discountFactor;
            cumulativeNPV += discountedCashFlow;
            yearlyCashFlow.push_back(operatingCashFlow);

            year++;
        }

        calculateNPV(yearlyCashFlow);
        calculateIRR(yearlyCashFlow);
        calculatePaybackPeriod(yearlyCashFlow);
        updateMetrics();
    }

    double calculateNPV(const std::vector<double>& cashFlows) {
        double npv = 0.0;
        for (size_t i = 0; i < cashFlows.size(); ++i) {
            npv += cashFlows[i];
        }
        return npv;
    }

    double calculateIRR(const std::vector<double>& cashFlows) {
        double lowerBound = -1.0; // Start with -100%
        double upperBound = 1.0; // Start with 100%
        double mid = 0.0;

        while (upperBound - lowerBound > 0.0001) {
            mid = (lowerBound + upperBound) / 2;
            double npv = calculateNPV(cashFlows, mid);

            if (npv > 0) {
                lowerBound = mid;
            } else {
                upperBound = mid;
            }
        }
        return mid;
    }

    void calculatePaybackPeriod(const std::vector<double>& cashFlows) {
        double cumulativeCashFlow = 0.0;
        for (size_t year = 0; year < cashFlows.size(); ++year) {
            cumulativeCashFlow += cashFlows[year];
            if (cumulativeCashFlow >= 0) {
                std::cout << "Payback Period: " << year << " years" << std::endl;
                break;
            }
        }
    }

    void updateMetrics() {
        projectLife = metrics[0];
        totalRevenue = metrics[1];
        totalOpex = metrics[2];
        avgAnnualCashFlow = metrics[3];
        // Calculate averages and totals as needed
    }

    void exportCashFlowCSV(const std::vector<double>& yearlyCashFlow) {
        std::ofstream file("CashFlow.csv");
        for (const auto& cashFlow : yearlyCashFlow) {
            file << cashFlow << "\n";
        }
        file.close();
    }

    void exportSensitivityAnalysis(double basePrice, double baseCost) {
        for (double variation : {-0.2, -0.1, 0, 0.1, 0.2}) {
            double price = basePrice * (1 + variation);
            double cost = baseCost * (1 + variation);
            // Calculate and store NPV for each variation
        }
    }
};

struct Phase {
    double oreTonnage;
    double waste;
    double grade;
    double recovery;
};

struct EconomicParameters {
    double price;
    double costPerTonne;
    double processCost;
};
