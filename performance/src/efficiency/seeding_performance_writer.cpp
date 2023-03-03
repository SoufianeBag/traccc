/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

// Local include(s).
#include "traccc/efficiency/seeding_performance_writer.hpp"

#include "duplication_plot_tool.hpp"
#include "eff_plot_tool.hpp"
#include "track_classification.hpp"

// ROOT include(s).
#ifdef TRACCC_HAVE_ROOT
#include <TFile.h>
#endif  // TRACCC_HAVE_ROOT

// System include(s).
#include <iostream>
#include <memory>
#include <stdexcept>

namespace traccc {
namespace details {

struct seeding_performance_writer_data {

    /// Constructor
    seeding_performance_writer_data(
        const seeding_performance_writer::config& cfg)
        : m_eff_plot_tool({cfg.var_binning}),
          m_duplication_plot_tool({cfg.var_binning}) {}

    /// Plot tool for efficiency
    eff_plot_tool m_eff_plot_tool;
    std::map<std::string, eff_plot_tool::eff_plot_cache> m_eff_plot_caches;

    /// Plot tool for duplication rate
    duplication_plot_tool m_duplication_plot_tool;
    std::map<std::string, duplication_plot_tool::duplication_plot_cache>
        m_duplication_plot_caches;

    measurement_particle_map m_measurement_particle_map;
    particle_map m_particle_map;

};  // struct seeding_performance_writer_data

}  // namespace details

seeding_performance_writer::seeding_performance_writer(const config& cfg)
    : m_cfg(cfg),
      m_data(std::make_unique<details::seeding_performance_writer_data>(cfg)) {}

seeding_performance_writer::~seeding_performance_writer() {}

void seeding_performance_writer::add_cache(std::string_view name) {

    m_data->m_eff_plot_tool.book(name, m_data->m_eff_plot_caches[name.data()]);
    m_data->m_duplication_plot_tool.book(
        name, m_data->m_duplication_plot_caches[name.data()]);
}

void seeding_performance_writer::write(
    std::string_view name, const seed_collection_types::const_view& seeds_view,
    const spacepoint_container_types::const_view& spacepoints_view,
    const event_map& evt_map) {

    std::map<particle_id, std::size_t> match_counter;

    // Iterate over the seeds.
    seed_collection_types::const_device seeds(seeds_view);
    for (const seed& sd : seeds) {

        // Check which particle matches this seed.
        std::vector<particle_hit_count> particle_hit_counts =
            identify_contributing_particles(
                sd.get_measurements(spacepoints_view), evt_map.meas_ptc_map);

        if (particle_hit_counts.size() == 1) {
            auto pid = particle_hit_counts.at(0).ptc.particle_id;
            match_counter[pid]++;
        }
    }

    for (auto const& [pid, ptc] : evt_map.ptc_map) {

        // Count only charged particles which satisfiy pT_cut
        if (ptc.charge == 0 || getter::perp(ptc.mom) < m_cfg.pT_cut) {
            continue;
        }

        bool is_matched = false;
        std::size_t n_matched_seeds_for_particle = 0;
        auto it = match_counter.find(pid);
        if (it != match_counter.end()) {
            is_matched = true;
            n_matched_seeds_for_particle = it->second;
        }

        m_data->m_eff_plot_tool.fill(m_data->m_eff_plot_caches[name.data()],
                                     ptc, is_matched);
        m_data->m_duplication_plot_tool.fill(
            m_data->m_duplication_plot_caches[name.data()], ptc,
            n_matched_seeds_for_particle - 1);
    }
}

void seeding_performance_writer::finalize() {

#ifdef TRACCC_HAVE_ROOT
    // Open the output file.
    std::unique_ptr<TFile> ofile(
        TFile::Open(m_cfg.file_path.c_str(), m_cfg.file_mode.c_str()));
    if ((!ofile) || ofile->IsZombie()) {
        throw std::runtime_error("Could not open output file \"" +
                                 m_cfg.file_path + "\" in mode \"" +
                                 m_cfg.file_mode + "\"");
    }
    ofile->cd();
#else
    std::cout << "ROOT file \"" << m_cfg.file_path << "\" is NOT created"
              << std::endl;
#endif  // TRACCC_HAVE_ROOT

    for (auto const& [name, cache] : m_data->m_eff_plot_caches) {
        m_data->m_eff_plot_tool.write(cache);
    }

    for (auto const& [name, cache] : m_data->m_duplication_plot_caches) {
        m_data->m_duplication_plot_tool.write(cache);
    }
}

}  // namespace traccc
