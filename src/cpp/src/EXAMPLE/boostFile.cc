/**
 * @author Antoine Laudrain <antoine.laudrain@desy.de>
 * @date January 2024
 * @brief Boost all particles from a *.slcio file to match the beam conditions from HALHF.
 *  The output file matches the input file name: *-boosted.slcio
 */

#include <iostream>
#include <string>
#include <vector> 
#include <filesystem>

#include <lcio.h>
#include <IOIMPL/LCFactory.h>
#include <UTIL/LCIterator.h>
#include <IMPL/MCParticleImpl.h>

#include "Math/Boost.h"
#include "Math/Vector4D.h"
#include "TFile.h"
#include "RooWorkspace.h"

using std::string;
using std::vector;
using std::cout;
using std::endl;

template <class T>
ROOT::Math::LorentzVector<T>
getBoosted(const ROOT::Math::LorentzVector<T>& lv) {
    // We only need to compute the boost once
    // -> avoid doing it for every function call...
    // I wish I could make all of this constexpr...
    static const auto em = ROOT::Math::PxPyPzEVector(0, 0, 500, 500); // GeV
    static const auto ep = ROOT::Math::PxPyPzEVector(0, 0, -31.3, 31.3); // GeV
    static const auto com = em + ep;
    static const auto boost = ROOT::Math::Boost(com.BoostToCM()).Inverse();
    // .BoostToCM returns the vector need to bring the system to the CoM,
    // not the CoM to the system.
    return boost(lv);
}

void boostFile(string fname) {
    // Create a LCIO reader.
    auto* lcReader = IOIMPL::LCFactory::getInstance()->createLCReader();
    lcReader->open(fname);
    cout << "N events: " << lcReader->getNumberOfEvents() << endl;

    // Change input file name to *-boosted.*
    auto inpath = std::filesystem::path(fname);
    auto outpath = inpath.replace_filename(
        inpath.stem().string() + "-boosted" + inpath.extension().string()
    );
    cout << "Will write: " << outpath.string() << endl;

    // Create a LCIO writer.
    auto* lcWriter = IOIMPL::LCFactory::getInstance()->createLCWriter();
    lcWriter->open(outpath.string(), EVENT::LCIO::WRITE_NEW);

    EVENT::LCEvent* evt = nullptr;
    int nEvtRead = 0;

    // Event loop.
    // We read in Update mode to allow for in-place modification of the
    // collections.
    while ( (evt = lcReader->readNextEvent(EVENT::LCIO::UPDATE)) != nullptr ) {
        cout << "Event " << nEvtRead << endl;

        // MCParticle loop.
        // Implicit cast to the IMPL class to access the setters.
        // Example: https://ilcsoft.desy.de/LCIO/current/doc/doxygen_api/html/classUTIL_1_1LCIterator.html
        UTIL::LCIterator<IMPL::MCParticleImpl> it(evt, "MCParticle");
        while ( auto* particle = it.next() ) {
            // momentum is stored in GeV
            auto lv = ROOT::Math::PxPyPzEVector(
                particle->getMomentum()[0],
                particle->getMomentum()[1],
                particle->getMomentum()[2],
                particle->getEnergy()
            );
            // cout << particle->getMomentum()[0] << ", "
            //      << particle->getMomentum()[1] << ", "
            //      << particle->getMomentum()[2] << ", "
            //      << particle->getEnergy() << endl;
            auto boosted = getBoosted(lv);
            double momentum[3] = {
                boosted.Px(),
                boosted.Py(),
                boosted.Pz()
            };
            particle->setMomentum(momentum);
        }

        lcWriter->writeEvent(evt) ;

        nEvtRead++;
        // if (nEvtRead >= 2) {
        //     break;
        // }
    }  // end event loop
}


int main(int argc, char** argv ) {

    // read file names from command line (only argument)
    if (argc < 2) {
        cout << " usage:  boostLCIO <input-file1> [[input-file2],...]" << endl << endl ;
        exit(1) ;
    }

    // int nFiles = argc - 1;
    // vector<string> fnames;
    // fnames.reserve(nFiles);

    for (int i = 1; i < argc; i++) {
        // fnames.push_back(argv[i]);
        boostFile(argv[i]);
    }
    
    return 0;
}

