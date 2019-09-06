/**
 * \file CRTSimHitViewer.cc
 *
 *
 * Author:
 */

#include <iostream>
#include <array>

#include "canvas/Utilities/InputTag.h"
#include "core/SelectionBase.hh"
#include "core/Event.hh"
#include "core/Experiment.hh"
#include "core/ProviderManager.hh"

#include "lardataobj/Simulation/AuxDetSimChannel.h"
#include "icaruscode/CRT/CRTProducts/CRTHit.hh"
#include "fhiclcpp/ParameterSet.h"

#include "TH1D.h"
#include "TGraph.h"

namespace ana {
  namespace SBNOsc {

/**
 * \class CRTSimHitViewer
 * \brief Electron neutrino event selection
 */
class CRTSimHitViewer : public core::SelectionBase {
public:
  /** Constructor. */
  CRTSimHitViewer() {}

  /**
   * Initialization.
   *
   * \param config A configuration, as a FHiCL ParameterSet object
   */
  void Initialize(fhicl::ParameterSet* config=NULL) {
    fCRTHitTag = config ? config->get<std::string>("CRTHitTag", "crtsimhit") : "crtsimhit";
    event_ind = 0;
  }

  /** Finalize and write objects to the output file. */
  void Finalize() {
  }

  /**
   * Process one event.
   *
   * \param ev A single event, as a gallery::Event
   * \param Reconstructed interactions
   * \return True to keep event
   */
  bool ProcessEvent(const gallery::Event& ev, const std::vector<event::Interaction> &truth, std::vector<event::RecoInteraction>& reco) {
    const std::vector<icarus::crt::CRTHit> &crt_hits = *ev.getValidHandle<std::vector<icarus::crt::CRTHit>>(fCRTHitTag);
    const std::vector<sim::AuxDetSimChannel> &crt_sim_channels = *ev.getValidHandle<std::vector<sim::AuxDetSimChannel>>("largeant");
    std::cout << "Event: " << event_ind << std::endl;

    for (const sim::AuxDetSimChannel &sim_channel: crt_sim_channels) {
      int channel = sim_channel.AuxDetID();
      std::cout << "Channel: " << channel << std::endl;
      for (const sim::AuxDetIDE &ide: sim_channel.AuxDetIDEs()) {
        std::cout << "IDE\n";
        std::cout << "entry T: " << ide.entryT << " exit T: " << ide.exitT << std::endl; 
        std::cout << "entry X: " << ide.entryX << " entry Y: " << ide.entryY << " entry Z: " << ide.entryZ << std::endl;
        std::cout << "exit X: " << ide.exitX << " exit Y: " << ide.exitY << " exit Z: " << ide.exitZ << std::endl;
      }
    }

    for (const icarus::crt::CRTHit &crt_hit: crt_hits) {
      std::cout << "TS0 NS: " << crt_hit.ts0_ns << std::endl;
      std::cout << "TS1 NS: " << crt_hit.ts1_ns << std::endl;
      std::cout << "X: " << crt_hit.x_pos << " Y: " << crt_hit.y_pos << " Z: " << crt_hit.z_pos << std::endl;
      std::cout << "Xerr: " << crt_hit.x_err << " Yerr: " << crt_hit.y_err << " Z: " << crt_hit.z_err << std::endl;
      std::cout << "Plane: " << crt_hit.plane << std::endl;
    }
    event_ind += 1;

    return false; 
  }

protected:
  std::string fCRTHitTag;
  unsigned event_ind;
};

  }  // namespace SBNOsc
}  // namespace ana
DECLARE_SBN_PROCESSOR(ana::SBNOsc::CRTSimHitViewer)

