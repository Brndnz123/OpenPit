// AppState.h

#ifndef APPSTATE_H
#define APPSTATE_H

struct EconomicParams {
    double cost;
    double revenue;
};

struct PitOptimizationParams {
    int maxIterations;
    double tolerance;
};

struct AppEvents {
    bool isMiningActive;
    bool isSurveyingActive;
};

struct AppState {
    EconomicParams econParams;
    PitOptimizationParams optParams;
    AppEvents events;
    // Kriging parameters
    double krigingParam1;
    double krigingParam2;
};

#endif // APPSTATE_H
