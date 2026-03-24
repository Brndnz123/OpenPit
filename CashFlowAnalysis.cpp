#include "CashFlowAnalysis.h"
#include <cmath>
#include <iostream>
#include <fstream>

CashFlowAnalysis::CashFlowAnalysis() 
    : totalRevenue(0), totalOpex(0), avgAnnualCashFlow(0), projectLife(0), capex(0) {
    metrics.resize(4);
}

void CashFlowAnalysis::generateCashFlow(double discountRate) {
    if (yearlyCashFlow.empty()) return;
    
    double cumulativeNPV = -capex;
    for (size_t year = 0; year < yearlyCashFlow.size(); ++year) {
        double discountFactor = 1.0 / std::pow(1.0 + discountRate, (double)year);
        double discountedCashFlow = yearlyCashFlow[year] * discountFactor;
        cumulativeNPV += discountedCashFlow;
    }
    
    calculateNPV();
    calculateIRR();
    calculatePaybackPeriod();
    updateMetrics();
}

double CashFlowAnalysis::calculateNPV() {
    double npv = -capex;
    for (size_t i = 0; i < yearlyCashFlow.size(); ++i) {
        npv += yearlyCashFlow[i];
    }
    return npv;
}

double CashFlowAnalysis::calculateIRR() {
    double lowerBound = -0.99;
    double upperBound = 1.0;
    
    for (int iter = 0; iter < 100; ++iter) {
        double mid = (lowerBound + upperBound) / 2.0;
        double npv = 0.0;
        
        for (size_t i = 0; i < yearlyCashFlow.size(); ++i) {
            npv += yearlyCashFlow[i] / std::pow(1.0 + mid, (double)i);
        }
        
        if (npv > 0) lowerBound = mid;
        else upperBound = mid;
    }
    
    return (lowerBound + upperBound) / 2.0;
}

void CashFlowAnalysis::calculatePaybackPeriod() {
    double cumulativeCashFlow = -capex;
    for (size_t year = 0; year < yearlyCashFlow.size(); ++year) {
        cumulativeCashFlow += yearlyCashFlow[year];
        if (cumulativeCashFlow >= 0) {
            std::cout << "Payback Period: " << year << " years\n";
            return;
        }
    }
}

void CashFlowAnalysis::updateMetrics() {
    projectLife = (int)yearlyCashFlow.size();
    totalRevenue = 0;
    totalOpex = 0;
    for (double cf : yearlyCashFlow) {
        if (cf > 0) totalRevenue += cf;
        else totalOpex += -cf;
    }
    if (projectLife > 0)
        avgAnnualCashFlow = totalRevenue / projectLife;
}

void CashFlowAnalysis::addCashFlow(double amount) {
    yearlyCashFlow.push_back(amount);
}

void CashFlowAnalysis::exportCashFlowCSV(const std::string& filename) {
    std::ofstream file(filename);
    for (const auto& cf : yearlyCashFlow) {
        file << cf << "\n";
    }
    file.close();
}

void CashFlowAnalysis::exportSensitivityAnalysis(const std::string& filename, double basePrice, double baseCost) {
    std::ofstream file(filename);
    file << "Variation,Price,Cost,NPV\n";
    
    double v1 = -0.2;
    double v2 = -0.1;
    double v3 = 0.0;
    double v4 = 0.1;
    double v5 = 0.2;
    
    for (int i = 0; i < 5; ++i) {
        double variation = (i == 0) ? v1 : (i == 1) ? v2 : (i == 2) ? v3 : (i == 3) ? v4 : v5;
        double price = basePrice * (1.0 + variation);
        double cost = baseCost * (1.0 + variation);
        double npv = calculateNPV();
        file << variation << "," << price << "," << cost << "," << npv << "\n";
    }
    file.close();
}
