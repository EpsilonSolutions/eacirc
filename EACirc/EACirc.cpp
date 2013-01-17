#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <ctime>

#include "EACirc.h"
#include "EACglobals.h"
#include "CommonFnc.h"
#include "random_generator/IRndGen.h"
#include "random_generator/BiasRndGen.h"
#include "random_generator/QuantumRndGen.h"
#include "random_generator/MD5RndGen.h"
//libinclude (galib/GA1DArrayGenome.h)
#include "GA1DArrayGenome.h"
#include "XMLProcessor.h"
#include "Evaluator.h"
#include "CircuitGenome.h"
#include "estream/estreamInterface.h"
#include "test_vector_generator/ITestVectGener.h"
#include "test_vector_generator/EstreamTestVectGener.h"
#include "estream/EncryptorDecryptor.h"
#include "EAC_circuit.h"
#include "standalone_testers/TestDistinctorCircuit.h"

#ifdef _WIN32
	#include <Windows.h>
	#define getpid() GetCurrentProcessId()
#endif
#ifdef __linux__
	#include <sys/types.h>
	#include <unistd.h>
#endif

IRndGen*                rndGen = NULL;
IRndGen*                biasRndGen = NULL;
GA_CIRCUIT*             pGACirc = NULL;
EncryptorDecryptor*		encryptorDecryptor = NULL;

EACirc::EACirc()
    : m_status(STAT_OK), m_originalSeed(0), m_currentGalibSeed(0), m_evaluator(NULL), m_gaData(NULL),
      m_readyToRun(0), m_actGener(0), m_oldGenerations(0) {
}

EACirc::~EACirc() {
    if (m_gaData) delete m_gaData;
    m_gaData = NULL;
    if (m_evaluator) delete m_evaluator;
    m_evaluator = NULL;
    if (encryptorDecryptor) delete encryptorDecryptor;
    encryptorDecryptor = NULL;
    if (pGACirc) pGACirc->release();
    pGACirc = NULL;
    if (rndGen) delete rndGen;
    rndGen = NULL;
    if (biasRndGen) delete biasRndGen;
    biasRndGen = NULL;
    if (mainGenerator) delete mainGenerator;
    mainGenerator = NULL;
}

int EACirc::getStatus() const {
    return m_status;
}

void EACirc::loadConfiguration(const string filename) {
    if (m_status != STAT_OK) return;
    m_status = LoadConfigScript(filename, &basicSettings);
    if (m_status != STAT_OK) {
        mainLogger.out() << "error: Could not read configuration data from " << FILE_CONFIG << "." << endl;
    }
    // CREATE STRUCTURE OF CIRCUIT FROM BASIC SETTINGS
    pGACirc = &(basicSettings.gaCircuitConfig);
    pGACirc->allocate();

    if (basicSettings.recommenceComputation || basicSettings.gaConfig.evolutionOff) {
        basicSettings.loadInitialPopulation = true;
    }

    if (basicSettings.gaCircuitConfig.changeGalibSeedFrequency != 0 &&
            basicSettings.gaCircuitConfig.changeGalibSeedFrequency % basicSettings.gaCircuitConfig.testVectorChangeGener != 0) {
        mainLogger.out() << "error: GAlib reseeding frequency must be multiple of test vector change frequency." << endl;
        m_status = STAT_CONFIG_INCORRECT;
    }
    if (basicSettings.gaCircuitConfig.TVCGProgressive == 1 &&
            basicSettings.gaCircuitConfig.changeGalibSeedFrequency != 0) {
        mainLogger.out() << "error: Prograsive teste vector generation cannot be used when saving state." << endl;
        m_status = STAT_CONFIG_INCORRECT;
    }

    if (m_status == STAT_OK) {
        m_readyToRun |= EACIRC_CONFIG_LOADED;
    }
}


void EACirc::saveState(const string filename) {
    TiXmlElement* pRoot = new TiXmlElement("eacirc_state");
    TiXmlElement* pElem;
    TiXmlElement* pElem2;

    pElem = new TiXmlElement("generations_required");
    pElem->LinkEndChild(new TiXmlText(toString(basicSettings.gaConfig.nGeners + m_oldGenerations).c_str()));
    pRoot->LinkEndChild(pElem);
    pElem = new TiXmlElement("generations_finished");
    pElem->LinkEndChild(new TiXmlText(toString(m_actGener + m_oldGenerations).c_str()));
    pRoot->LinkEndChild(pElem);
    pElem = new TiXmlElement("main_seed");
    pElem->LinkEndChild(new TiXmlText(toString(m_originalSeed).c_str()));
    pRoot->LinkEndChild(pElem);
    pElem = new TiXmlElement("current_galib_seed");
    pElem->LinkEndChild(new TiXmlText(toString(m_currentGalibSeed).c_str()));
    pRoot->LinkEndChild(pElem);

    pElem = new TiXmlElement("random_generators");
    pElem2 = new TiXmlElement("main_generator");
    pElem2->LinkEndChild(mainGenerator->exportGenerator());
    pElem->LinkEndChild(pElem2);
    pElem2 = new TiXmlElement("rndgen");
    pElem2->LinkEndChild(rndGen->exportGenerator());
    pElem->LinkEndChild(pElem2);
    pElem2 = new TiXmlElement("biasrndgen");
    pElem2->LinkEndChild(biasRndGen->exportGenerator());
    pElem->LinkEndChild(pElem2);
    pRoot->LinkEndChild(pElem);

    m_status = saveXMLFile(pRoot,filename);
    if (m_status != STAT_OK) {
        mainLogger.out() << "error: Cannot save state to file " << filename << "." << endl;
    } else {
        mainLogger.out() << "info: State successfully saved to file " << filename << "." << endl;
    }
}

void EACirc::loadState(const string filename) {
    TiXmlNode* pRoot = NULL;
    m_status = loadXMLFile(pRoot,filename);
    if (m_status != STAT_OK) {
        mainLogger.out() << "error: Could not load state from file " << filename << "." << endl;
        return;
    }

    // restore generations done
    m_oldGenerations = atol(getXMLElementValue(pRoot,"generations_finished").c_str());
    // restore main seed
    istringstream(getXMLElementValue(pRoot,"main_seed")) >> m_originalSeed;
    basicSettings.rndGen.randomSeed = m_originalSeed;
    // restore current galib seed
    istringstream(getXMLElementValue(pRoot,"current_galib_seed")) >> m_currentGalibSeed;
    // initialize random generators (main, quantum, bias)
    mainGenerator = IRndGen::parseGenerator(getXMLElement(pRoot,"random_generators/main_generator/generator"));
    rndGen = IRndGen::parseGenerator(getXMLElement(pRoot,"random_generators/rndgen/generator"));
    biasRndGen = IRndGen::parseGenerator(getXMLElement(pRoot,"random_generators/biasrndgen/generator"));

    delete pRoot;
    mainLogger.out() << "info: State successfully loaded from file " << filename << "." << endl;
}

void EACirc::createState() {
    // INIT MAIN GENERATOR
    // if useFixedSeed and proper seed was specified, use it
    if (basicSettings.rndGen.useFixedSeed && basicSettings.rndGen.randomSeed != 0) {
        m_originalSeed = basicSettings.rndGen.randomSeed;
        mainGenerator = new MD5RndGen(m_originalSeed);
        mainLogger.out() << "info: Using fixed seed: " << m_originalSeed << endl;
    } else {
        // generate random seed, if none provided
        mainGenerator = new MD5RndGen(clock() + time(NULL) + getpid());
        mainGenerator->getRandomFromInterval(ULONG_MAX,&m_originalSeed);
        delete mainGenerator;
        mainGenerator = NULL; // necessary !!! (see guts of MD5RndGen)
        mainGenerator = new MD5RndGen(m_originalSeed);
        mainLogger.out() << "info: Using system-generated random seed: " << m_originalSeed << endl;
    }

    // INIT RNG
    unsigned long generatorSeed;
    mainGenerator->getRandomFromInterval(ULONG_MAX,&generatorSeed);
    rndGen = new QuantumRndGen(generatorSeed, basicSettings.rndGen.QRBGSPath);
    mainLogger.out() << "info: Random generator initialized (" << rndGen->shortDescription() << ")" <<endl;
    // INIT BIAS RNDGEN
    mainGenerator->getRandomFromInterval(ULONG_MAX,&generatorSeed);
    biasRndGen = new BiasRndGen(generatorSeed, basicSettings.rndGen.QRBGSPath, basicSettings.rndGen.biasFactor);
    mainLogger.out() << "info: Bias random generator initialized (" << biasRndGen->shortDescription() << ")" <<endl;

    // GENERATE SEED FOR GALIB
    mainGenerator->getRandomFromInterval(ULONG_MAX,&m_currentGalibSeed);
    mainLogger.out() << "info: State successfully initialized." << endl;
}

void EACirc::savePopulation(const string filename) {
    TiXmlElement* pRoot = new TiXmlElement("eacirc_population");
    TiXmlElement* pElem = NULL;
    TiXmlElement* pElem2 = NULL;

    pElem = new TiXmlElement("population_size");
    pElem->LinkEndChild(new TiXmlText(toString(basicSettings.gaConfig.popSize).c_str()));
    pRoot->LinkEndChild(pElem);
    pElem = new TiXmlElement("genome_size");
    pElem->LinkEndChild(new TiXmlText(toString(basicSettings.gaCircuitConfig.genomeSize).c_str()));
    pRoot->LinkEndChild(pElem);
    pElem = new TiXmlElement("population");
    string textCircuit;
    for (int i = 0; i < basicSettings.gaConfig.popSize; i++) {
        // note: it is not necessary to call individual i in SCALED order
        //       however then the population files differ in order ('diff' cannot be used to fing bugs)
        GA1DArrayGenome<unsigned long>* pGenome = (GA1DArrayGenome<unsigned long>*) &(m_gaData->population().individual(i,GAPopulation::SCALED));
        m_status = CircuitGenome::writeGenome(*pGenome ,textCircuit);
        if (m_status != STAT_OK) {
            mainLogger.out() << "error: Could not save genome in population to file " << filename << "." << endl;
            return;
        }
        pElem2 = new TiXmlElement("genome");
        pElem2->LinkEndChild(new TiXmlText(textCircuit.c_str()));
        pElem->LinkEndChild(pElem2);
    }
    pRoot->LinkEndChild(pElem);

    m_status = saveXMLFile(pRoot, filename);
    if (m_status != STAT_OK) {
        mainLogger.out() << "error: Cannot save population to file " << filename << "." << endl;
    } else {
        mainLogger.out() << "info: Population successfully saved to file " << filename << "." << endl;
    }
}

void EACirc::loadPopulation(const string filename) {
    TiXmlNode* pRoot = NULL;
    m_status = loadXMLFile(pRoot,filename);
    if (m_status != STAT_OK) {
        mainLogger.out() << "error: Could not load state from file " << filename << "." << endl;
        return;
    }
    int savedPopulationSize = atoi(getXMLElementValue(pRoot,"population_size").c_str());
    int savedGenomeSize = atoi(getXMLElementValue(pRoot,"genome_size").c_str());
    if (savedGenomeSize != basicSettings.gaCircuitConfig.genomeSize) {
        mainLogger.out() << "error: Cannot load population - incompatible genome size." << endl;
        mainLogger.out() << "       given genome size: " << savedGenomeSize << endl;
        mainLogger.out() << "       required genome size: " << basicSettings.gaCircuitConfig.genomeSize << endl;
        m_status = STAT_INCOMPATIBLE_PARAMETER;
        delete pRoot;
        return;
    }
    GAPopulation population;
    GA1DArrayGenome<unsigned long> genome(pGACirc->genomeSize, CircuitGenome::Evaluator);
    // INIT GENOME STRUCTURES
    genome.initializer(CircuitGenome::Initializer);
    genome.mutator(CircuitGenome::Mutator);
    genome.crossover(CircuitGenome::Crossover);
    // LOAD genomes
    TiXmlElement* pGenome = getXMLElement(pRoot,"population/genome")->ToElement();
    string textCircuit;
    for (int i = 0; i < savedPopulationSize; i++) {
        if (pGenome->GetText() == NULL) {
            mainLogger.out() << "error: Too few genomes in population - expecting " << savedPopulationSize << "." << endl;
            m_status = STAT_INCOMPATIBLE_PARAMETER;
            delete pRoot;
            return;
        }
        textCircuit = pGenome->GetText();
        CircuitGenome::readGenome(genome,textCircuit);
        population.add(genome);
        pGenome = pGenome->NextSiblingElement();
    }
    seedAndResetGAlib(population);
    delete pRoot;
    mainLogger.out() << "info: Population successfully loaded from file " << filename << "." << endl;
}

void EACirc::createPopulation() {
    // seed GAlib (initializations may require random numbers)
    GAResetRNG(m_currentGalibSeed);
    // temporary structure for genome (empty or loaded from file)
    GA1DArrayGenome<unsigned long> genome(basicSettings.gaCircuitConfig.genomeSize, CircuitGenome::Evaluator);
    genome.initializer(CircuitGenome::Initializer);
    genome.mutator(CircuitGenome::Mutator);
    genome.crossover(CircuitGenome::Crossover);
    GAPopulation population(genome,basicSettings.gaConfig.popSize);
    // create genetic algorithm and initialize population
    seedAndResetGAlib(population);
    mainLogger.out() << "info: Initializing population." << endl;
    m_gaData->initialize();
    // reset GAlib seed
    mainGenerator->getRandomFromInterval(ULONG_MAX,&m_currentGalibSeed);
    seedAndResetGAlib(m_gaData->population());

    mainLogger.out() << "info: Population successfully initialized." << endl;
}

void EACirc::saveProgress(const string stateFilename, const string populationFilename) {
    if (m_status != STAT_OK) return;
    saveState(stateFilename);
    savePopulation(populationFilename);
}

void EACirc::initializeState() {
    if (m_status != STAT_OK) return;
    if ((m_readyToRun & EACIRC_CONFIG_LOADED) != EACIRC_CONFIG_LOADED) {
        m_status = STAT_CONFIG_SCRIPT_INCOMPLETE;
        return;
    }
    // load or create STATE
    if (basicSettings.recommenceComputation) {
        loadState(FILE_STATE);
    } else {
        createState();
    }
    // load or create POPULATION
    if (basicSettings.loadInitialPopulation) {
        loadPopulation(FILE_POPULATION);
        mainLogger.out() << "info: Initializing Evaluator and Encryptor-Decryptor." << endl;
        // encryptorDecryptor should not be initialized (extra usage of random generator!)
        // encryptorDecryptor = new EncryptorDecryptor();
        m_evaluator = new Evaluator();
    } else {
        mainLogger.out() << "info: Initializing Evaluator and Encryptor-Decryptor." << endl;
        encryptorDecryptor = new EncryptorDecryptor();
        m_evaluator = new Evaluator();
        m_evaluator->generateTestVectors();
        createPopulation();
    }

    if (m_status == STAT_OK) {
        m_readyToRun |= EACIRC_INITIALIZED;
    }
}

void EACirc::prepare() {
    if (m_status != STAT_OK) return;
    if ((m_readyToRun & (EACIRC_CONFIG_LOADED | EACIRC_INITIALIZED)) !=
            (EACIRC_CONFIG_LOADED | EACIRC_INITIALIZED)) {
        m_status = STAT_CONFIG_SCRIPT_INCOMPLETE;
        return;
    }

    // PREPARE THE LOGGING FILES
    std::remove(FILE_BOINC_FRACTION_DONE);
    if (!basicSettings.recommenceComputation) {
        std::remove(FILE_FITNESS_PROGRESS);
        std::remove(FILE_BEST_FITNESS);
        std::remove(FILE_AVG_FITNESS);
        std::remove(FILE_GALIB_SCORES);
        std::remove(FILE_TEST_VECTORS);
        std::remove(FILE_TEST_DATA_1);
        std::remove(FILE_TEST_DATA_2);
    }

    //LOG THE TESTVECTGENER METHOD
    if (basicSettings.gaCircuitConfig.testVectorGenerMethod == ESTREAM_CONST) {
        if (!basicSettings.recommenceComputation) {
            ofstream out(FILE_FITNESS_PROGRESS, ios::app);
            out << "Using Ecrypt candidate n." << basicSettings.gaCircuitConfig.testVectorEstream;
            out << " (" <<  basicSettings.gaCircuitConfig.limitAlgRoundsCount << " rounds) AND candidate n.";
            out << basicSettings.gaCircuitConfig.testVectorEstream2 << " (";
            out << basicSettings.gaCircuitConfig.limitAlgRoundsCount2 << " rounds)" << endl;
            out.close();
        }
        mainLogger.out() << "info: stream1: using " << estreamToString(basicSettings.gaCircuitConfig.testVectorEstream);
        mainLogger.out() << " (" << basicSettings.gaCircuitConfig.limitAlgRoundsCount << " rounds)" << endl;
        mainLogger.out() << "info: stream2: using " << estreamToString(basicSettings.gaCircuitConfig.testVectorEstream2);
        mainLogger.out() << " (" << basicSettings.gaCircuitConfig.limitAlgRoundsCount2 << " rounds)" << endl;
    }

    if (m_status == STAT_OK) {
        m_readyToRun |= EACIRC_PREPARED;
    }
}

void EACirc::seedAndResetGAlib(const GAPopulation &population) {
    // set GAlib seed
    GAResetRNG(m_currentGalibSeed);
    // init new instance of gentetic algorithm
    GASteadyStateGA* gaTemp = new GASteadyStateGA(population);
    // reset population stats
    gaTemp->pop->touch();
    // delete any previous instance of genetic algorithm
    if (m_gaData != NULL) {
        delete m_gaData;
        m_gaData = NULL;
    }
    m_gaData = gaTemp;
    // initialize the new genetic algorithm
    m_gaData->populationSize(basicSettings.gaConfig.popSize);
    m_gaData->nReplacement(2 * basicSettings.gaConfig.popSize / 3);
    m_gaData->nGenerations(basicSettings.gaConfig.nGeners);
    m_gaData->pCrossover(basicSettings.gaConfig.pCross);
    m_gaData->pMutation(basicSettings.gaConfig.pMutt);
    m_gaData->scoreFilename(FILE_GALIB_SCORES);
    m_gaData->scoreFrequency(1);	// keep the scores of every generation
    m_gaData->flushFrequency(1);	// specify how often to write the score to disk
    m_gaData->selectScores(GAStatistics::AllScores);
    mainLogger.out() << "info: GAlib seeded and reset." << endl;
}

void EACirc::run() {
    if (m_status != STAT_OK) return;
    if ((m_readyToRun & (EACIRC_CONFIG_LOADED | EACIRC_INITIALIZED | EACIRC_PREPARED)) !=
            (EACIRC_CONFIG_LOADED | EACIRC_INITIALIZED | EACIRC_PREPARED)) {
        m_status = STAT_CONFIG_SCRIPT_INCOMPLETE;
        return;
    }

    // SAVE INITIAL STATE
    saveProgress(FILE_STATE_INITIAL,FILE_POPULATION_INITIAL);

    int	changed = 1;
    bool evaluateNow = false;
    basicSettings.gaCircuitConfig.clearFitnessStats();
    fstream fitfile;

    GA1DArrayGenome<unsigned long> genomeBest(basicSettings.gaCircuitConfig.genomeSize, CircuitGenome::Evaluator);
    genomeBest = m_gaData->population().individual(0);

    mainLogger.out() << "info: Starting evolution." << endl;
    for (m_actGener = 1; m_actGener <= basicSettings.gaConfig.nGeners; m_actGener++) {
        if (m_status != STAT_OK) {
            mainLogger.out() << "error: Ooops, something went wrong, stopping. " << "(error: " << ErrorToString(m_status) << " )." << endl;
            break;
        }

        //FRACTION FILE FOR BOINC
        fitfile.open(FILE_BOINC_FRACTION_DONE, fstream::out | ios::trunc);
        fitfile << ((float)(m_actGener))/((float)(basicSettings.gaConfig.nGeners));
        fitfile.close();

        // DO NOT EVOLVE..
        if (basicSettings.gaConfig.evolutionOff) {
            m_evaluator->generateTestVectors();
            m_evaluator->evaluateStep(genomeBest, m_actGener);
            continue;
        }

        // GENERATE TEST VECTORS IF NEEDED
        if (basicSettings.gaCircuitConfig.TVCGProgressive) {
            // TODO: understand and correct
            if (changed > m_actGener/basicSettings.gaCircuitConfig.testVectorChangeGener + 1) {
                m_evaluator->generateTestVectors();
                evaluateNow = true;
                changed = 0;
            }
        } else {
            if (m_actGener %(basicSettings.gaCircuitConfig.testVectorChangeGener) == 1) {

                //temporary
                /* temporaroly moving to run cycle, since random keys are generated only on creation of EncryptorDecryptor
                  TODO: remake tenerating of test vectors so that IV and KEY is also generated when needed */
                mainLogger.out() << "info: recreating Encryptor-Decryptor." << endl;
                // INIT EVALUATOR and ENCRYPTOR-DECRYPTOR (according to loaded settings)
                if (encryptorDecryptor != NULL) delete encryptorDecryptor;
                encryptorDecryptor = NULL;
                encryptorDecryptor = new EncryptorDecryptor();

                m_evaluator->generateTestVectors();
            }
            if ( basicSettings.gaCircuitConfig.evaluateBeforeTestVectorChange &&
                 m_actGener %(basicSettings.gaCircuitConfig.testVectorChangeGener) == 0) {
                evaluateNow = true;
            }
            if (!basicSettings.gaCircuitConfig.evaluateBeforeTestVectorChange &&
                 m_actGener % (basicSettings.gaCircuitConfig.testVectorChangeGener) == 1) {
                evaluateNow = true;
            }
        }
        // variable for computing when TVCGProgressive = true
        changed++;

        // RESET EVALUTION FOR ALL GENOMS
        m_gaData->pop->flushEvalution();
        // GA evolution step
        m_gaData->step();

        genomeBest = (GA1DArrayGenome<unsigned long>&) m_gaData->population().best();// .statistics().bestIndividual();

        if (evaluateNow || basicSettings.gaCircuitConfig.evaluateEveryStep) {
            m_evaluator->evaluateStep(genomeBest, m_actGener + m_oldGenerations);
            evaluateNow = false;
        }

        // if needed, reseed GAlib and save state and population
        if (basicSettings.gaCircuitConfig.changeGalibSeedFrequency != 0
                && m_actGener % basicSettings.gaCircuitConfig.changeGalibSeedFrequency == 0) {
            mainGenerator->getRandomFromInterval(ULONG_MAX,&m_currentGalibSeed);
            seedAndResetGAlib(m_gaData->population());
            saveProgress(FILE_STATE,FILE_POPULATION);
        }
    }

    // commented for testing purposes of saving state

    // GENERATE FRESH NEW SET AND EVALUATE THE RESULT
    //m_evaluator->generateTestVectors();
    //m_evaluator->evaluateStep(genomeBest, m_actGener);
    //genomeBest = (GA1DArrayGenome<unsigned long>&) m_gaData->population().best();// .statistics().bestIndividual();
    //Print the best circuit
    CircuitGenome::PrintCircuit(genomeBest,FILE_BEST_CIRCUIT,0,1);
    //m_status = saveState(FILE_STATE,FILE_POPULATION);
}
