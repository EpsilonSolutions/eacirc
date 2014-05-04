/* 
 * File:   PolynomialCircuit.cpp
 * Author: ph4r05
 * 
 * Created on April 29, 2014, 4:20 PM
 */

#include "PolynomialCircuit.h"
#include "circuit/ICircuit.h"
#include "PolynomialCircuitIO.h"
#include "GAPolyCallbacks.h"
#include "Term.h"
#include <cmath>
#include "XMLProcessor.h"

PolynomialCircuit::PolynomialCircuit() : ICircuit(CIRCUIT_POLYNOMIAL) { }

PolynomialCircuit::~PolynomialCircuit() { }

GAGenome* PolynomialCircuit::createGenome(bool setCallbacks) {
    // Has to compute genome dimensions.
    int numVariables = pGlobals->settings->main.circuitSizeInput;
    int numPolynomials = pGlobals->settings->main.circuitSizeOutput;
    unsigned int   termSize = Term::getTermSize(numVariables); // Length of one term in terms of POLY_GENOME_ITEM_TYPE.    
    
    GA2DArrayGenome<POLY_GENOME_ITEM_TYPE> * g = new GA2DArrayGenome<POLY_GENOME_ITEM_TYPE>(
            numPolynomials, 
            1 + termSize * pGlobals->settings->polyCircuit.genomeInitMaxTerms,               // number of terms N + N terms.
            this->getEvaluator());
    
    if (setCallbacks){
        setGACallbacks(g);
    }
    
    return g;
}

GAPopulation* PolynomialCircuit::createPopulation() {
    int numVariables = pGlobals->settings->main.circuitSizeInput;
    int numPolynomials = pGlobals->settings->main.circuitSizeOutput;
    unsigned int   termSize = Term::getTermSize(numVariables);   // Length of one term in terms of POLY_GENOME_ITEM_TYPE.    
    
    GA2DArrayGenome<POLY_GENOME_ITEM_TYPE> g(
            numPolynomials, 
            1 + termSize * pGlobals->settings->polyCircuit.genomeInitMaxTerms,               // number of terms N + N terms.
            this->getEvaluator());
    setGACallbacks(&g);
    
    GAPopulation * population = new GAPopulation(g, pGlobals->settings->ga.popupationSize);
    return population;
}

bool PolynomialCircuit::postProcess(GAGenome& originalGenome, GAGenome& prunnedGenome) {
    return false;
}

int PolynomialCircuit::loadCircuitConfiguration(TiXmlNode* pRoot) {
    // parsing EACIRC/POLYNOMIAL_CIRCUIT
    pGlobals->settings->polyCircuit.genomeInitMaxTerms = atoi(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/MAX_TERMS").c_str());
    pGlobals->settings->polyCircuit.genomeInitTermCountProbability = atof(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/TERM_COUNT_P").c_str());
    pGlobals->settings->polyCircuit.genomeInitTermStopProbability  = atof(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/TERM_VAR_P").c_str());
    pGlobals->settings->polyCircuit.mutateAddTermProbability       = atof(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/ADD_TERM_P").c_str());
    pGlobals->settings->polyCircuit.mutateAddTermStrategy          = atoi(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/ADD_TERM_STRATEGY").c_str());
    pGlobals->settings->polyCircuit.mutateRemoveTermProbability    = atof(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/RM_TERM_P").c_str());
    pGlobals->settings->polyCircuit.mutateRemoveTermStrategy       = atoi(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/RM_TERM_STRATEGY").c_str());
    pGlobals->settings->polyCircuit.crossoverRandomizePolySelect   = atoi(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/CROSSOVER_RANDOMIZE_POLY").c_str()) ? true : false;
    pGlobals->settings->polyCircuit.crossoverTermsProbability      = atof(getXMLElementValue(pRoot,"POLYNOMIAL_CIRCUIT/CROSSOVER_TERM_P").c_str());

    return STAT_OK;
}
