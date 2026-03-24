#pragma once
#include <vector>
#include <string>

class CashFlowAnalysis {
private:
    std::vector<double> metrics;
    std::vector<double> yearlyCashFlow;
    double totalRevenue;
    double totalOpex;
    double avgAnnualCashFlow;
    double capex;
    int projectLife;

public:
    CashFlowAnalysis();
    
    void generateCashFlow(double discountRate);
    double calculateNPV();
    double calculateIRR();
    void calculatePaybackPeriod();
    void updateMetrics();
    void addCashFlow(double amount);
    void exportCashFlowCSV(const std::string& filename);
    void exportSensitivityAnalysis(const std::string& filename, double basePrice, double baseCost);
};
