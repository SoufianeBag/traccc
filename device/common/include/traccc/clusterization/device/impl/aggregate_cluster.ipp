/** TRACCC library, part of the ACTS project (R&D line)
 *
 * (c) 2022-2023 CERN for the benefit of the ACTS project
 *
 * Mozilla Public License Version 2.0
 */

#pragma once

// Project include(s)
#include <vecmem/memory/device_atomic_ref.hpp>

#include "traccc/clusterization/detail/measurement_creation_helper.hpp"

namespace traccc::device {
TRACCC_HOST_DEVICE
inline void aggregate_cluster(
    const alt_cell_collection_types::const_device& cells,
    const cell_module_collection_types::const_device& modules,
    const vecmem::data::vector_view<unsigned short> f_view,
    const unsigned int start, const unsigned int end, const unsigned short cid,
    alt_measurement& out, vecmem::data::vector_view<unsigned int> cell_links,
    const unsigned int link) {

    const vecmem::device_vector<unsigned short> f(f_view);
    vecmem::device_vector<unsigned int> cell_links_device(cell_links);

    /*
     * Now, we iterate over all other cells to check if they belong
     * to our cluster. Note that we can start at the current index
     * because no cell is ever a child of a cluster owned by a cell
     * with a higher ID.
     */
    scalar totalWeight = 0.;
    point2 mean{0., 0.}, var{0., 0.};
    const auto module_link = cells[cid + start].module_link;
    const cell_module this_module = modules.at(module_link);
    const unsigned short partition_size = end - start;

    channel_id maxChannel1 = std::numeric_limits<channel_id>::min();

    for (unsigned short j = cid; j < partition_size; j++) {

        assert(j < f.size());

        const unsigned int pos = j + start;
        /*
         * Terminate the process earlier if we have reached a cell sufficiently
         * in a different module.
         */
        if (cells[pos].module_link != module_link) {
            break;
        }

        const cell this_cell = cells[pos].c;

        /*
         * If the value of this cell is equal to our, that means it
         * is part of our cluster. In that case, we take its values
         * for position and add them to our accumulators.
         */
        if (f[j] == cid) {

            if (this_cell.channel1 > maxChannel1) {
                maxChannel1 = this_cell.channel1;
            }

            const float weight = traccc::detail::signal_cell_modelling(
                this_cell.activation, this_module);

            if (weight > this_module.threshold) {
                totalWeight += this_cell.activation;
                const point2 cell_position =
                    traccc::detail::position_from_cell(this_cell, this_module);
                const point2 prev = mean;
                const point2 diff = cell_position - prev;

                mean = prev + (weight / totalWeight) * diff;
                for (char i = 0; i < 2; ++i) {
                    var[i] = var[i] +
                             weight * (diff[i]) * (cell_position[i] - mean[i]);
                }
            }

            cell_links_device.at(pos) = link;
        }

        /*
         * Terminate the process earlier if we have reached a cell sufficiently
         * far away from the cluster in the dominant axis.
         */
        if (this_cell.channel1 > maxChannel1 + 1) {
            break;
        }
    }
    if (totalWeight > static_cast<scalar>(0.)) {
        for (char i = 0; i < 2; ++i) {
            var[i] /= totalWeight;
        }
        const auto pitch = this_module.pixel.get_pitch();
        var = var + point2{pitch[0] * pitch[0] / static_cast<scalar>(12.),
                           pitch[1] * pitch[1] / static_cast<scalar>(12.)};
    }

    /*
     * Fill output vector with calculated cluster properties
     */
    out.local = mean;
    out.variance = var;
    out.module_link = module_link;
}


TRACCC_DEVICE
inline void aggregate_cluster2(
    const cell_module_collection_types::const_device& modules,
    /*const vecmem::data::vector_view<cluster> f_view,*/cluster* id_clusters,
    const unsigned int start, const unsigned int end, const unsigned short cid,
    spacepoint_collection_types::device spacepoints, vecmem::data::vector_view<unsigned int> cell_links,
    const unsigned int link) {

    //const vecmem::device_vector<cluster> id_clusters(f_view);
    vecmem::device_vector<unsigned int> cell_links_device(cell_links);

    /*
     * Now, we iterate over all other cells to check if they belong
     * to our cluster. Note that we can start at the current index
     * because no cell is ever a child of a cluster owned by a cell
     * with a higher ID.
     */
    scalar totalWeight = 0.;
    point2 mean{0., 0.}, var{0., 0.};
    const unsigned int mod_link = id_clusters[cid].module_link;
    const cell_module this_module = modules.at(mod_link);
    const unsigned short partition_size = end - start;

    channel_id maxChannel1 = std::numeric_limits<channel_id>::min();
    #pragma unroll
    for (unsigned short j = cid; j < partition_size; j++) {

        
        assert(j < max_cells_per_partition);
        const unsigned int pos = j + start;
        /*
         * Terminate the process earlier if we have reached a cell sufficiently
         * in a different module.
         */
        if (id_clusters[j].module_link != mod_link) {
            break;
        }

        
        
        /*
         * If the value of this cell is equal to our, that means it
         * is part of our cluster. In that case, we take its values
         * for position and add them to our accumulators.
         */
        if (id_clusters[j].id_cluster == cid) {

            if (id_clusters[j].channel1 > maxChannel1) {
                maxChannel1 = id_clusters[j].channel1;
            }
            
            const float weight = traccc::detail::signal_cell_modelling(
                id_clusters[j].activation, this_module);
            //printf("weight %u \n" , weight);
            if (weight > this_module.threshold) {
                totalWeight += id_clusters[j].activation;
                const point2 cell_position =
                    traccc::detail::position_from_cell2(id_clusters[j].channel0 , id_clusters[j].channel1, this_module);
                const point2 prev = mean;
                const point2 diff = cell_position - prev;

                mean = prev + (weight / totalWeight) * diff;
                for (char i = 0; i < 2; ++i) {
                    var[i] = var[i] +
                             weight * (diff[i]) * (cell_position[i] - mean[i]);
                }
            }

            cell_links_device.at(pos) = link;
        }

        /*
         * Terminate the process earlier if we have reached a cell sufficiently
         * far away from the cluster in the dominant axis.
         */
        if (id_clusters[j].channel1 > maxChannel1 + 1) {
            break;
        }
    }
    if (totalWeight > static_cast<scalar>(0.)) {
        for (char i = 0; i < 2; ++i) {
            var[i] /= totalWeight;
        }
        const auto pitch = this_module.pixel.get_pitch();
        var = var + point2{pitch[0] * pitch[0] / static_cast<scalar>(12.),
                           pitch[1] * pitch[1] / static_cast<scalar>(12.)};
    }

    /*
     * Fill output vector with calculated cluster properties
     */
   
    //printf(" mean[0] %u , mean[1] %u \n " , mean[0] ,  mean[1] );
     point3 local_3d = {mean[0], mean[1], 0.};
    point3 global = this_module.placement.point_to_global(local_3d);
    spacepoint[link].global = global;
    spacepoint[link].meas = {mean, var, 0};
    // Fill the result object with this spacepoint
   // spacepoints_device[globalIndex] = {global, {mean, var, 0}};
}

}  // namespace traccc::device