#include <stdio.h>
#include <string.h>

#include "os.h"
#include "os_nif.h"
#include "log.h"

#include "wl80211_client.h"
#include "wl80211_survey.h"

#include "target.h"

#define MODULE_ID LOG_MODULE_ID_TARGET


/******************************************************************************
 *  PRIVATE definitions
 *****************************************************************************/

/******************************************************************************
 *  INTERFACE definitions
 *****************************************************************************/
bool target_is_radio_interface_ready(char *phy_name)
{
    bool    exists;
    char    buf[16] = { 0 };


    if (!phy_name)
        return false;

    if (os_nif_is_interface_ready(phy_name))
        return true;

    for (int ind = 1; ind < TARGET_MAX_VIF; ind++)
    {
        if (snprintf(buf, sizeof(buf), "%s.%d", phy_name, ind) >= (int)sizeof(buf))
            return false;  // should not happen

        exists = false;
        if (os_nif_exists(buf, &exists) != true || !exists)
            break;

        // if at least one exists and is ready, return true
        if (os_nif_is_interface_ready(buf))
            return true;
    }

    return false;
}

bool target_is_interface_ready(char *if_name)
{
    bool rc;

    rc = os_nif_is_interface_ready(if_name);
    if (rc != true) {
        return false;
    }

    return true;
}

/******************************************************************************
 *  RADIO definitions
 *****************************************************************************/
bool target_radio_tx_stats_enable(
        radio_entry_t              *radio_cfg,
        bool                        enable)
{
    return true;
}

bool target_radio_fast_scan_enable(
        radio_entry_t              *radio_cfg,
        ifname_t                    if_name)
{
    return true;
}

/******************************************************************************
 *  CLIENT definitions
 *****************************************************************************/
target_client_record_t* target_client_record_alloc()
{
    return wl80211_client_record_alloc();
}

void target_client_record_free(target_client_record_t *result)
{
    wl80211_client_record_free(result);
}

bool target_stats_clients_get(
        radio_entry_t              *radio_cfg,
        radio_essid_t              *essid,
        target_stats_clients_cb_t  *client_cb,
        ds_dlist_t                 *client_list,
        void                       *client_ctx)
{
    char vifname[32];
    int i;
    bool rc;

    if (essid)
    {
        // currently only NULL supported
        // that means, scan all vif interfaces
        return false;
    }

    for (i=0; i<TARGET_MAX_VIF; i++)
    {
        // format: radio_ifname.index eg. wl0.1
        // except wl0.0 => wl0
        if (i == 0) {
            STRSCPY(vifname, radio_cfg->phy_name);
        } else {
            snprintf(vifname, sizeof(vifname), "%s.%d", radio_cfg->phy_name, i);
        }
        LOGD("Fetching VIF %s clients", vifname);
        rc =
            wl80211_client_list_get(
                    radio_cfg,
                    vifname,
                    client_list);
        if (rc != true) {
            return false;
        }
    }

    (*client_cb)(client_list, client_ctx, true);

    return true;
}

bool target_stats_clients_convert(
        radio_entry_t              *radio_cfg,
        target_client_record_t     *data_new,
        target_client_record_t     *data_old,
        dpp_client_record_t        *client_result)
{
    bool rc;

    rc = wl80211_client_stats_convert(
                radio_cfg,
                data_new,
                data_old,
                client_result);

    if (rc != true) {
        return false;
    }

    return true;
}

/******************************************************************************
 *  SURVEY definitions
 *****************************************************************************/
target_survey_record_t* target_survey_record_alloc()
{
    return wl80211_survey_record_alloc();
}

void target_survey_record_free(target_survey_record_t *result)
{
    wl80211_survey_record_free(result);
}

bool target_stats_survey_get(
        radio_entry_t              *radio_cfg,
        uint32_t                   *chan_list,
        uint32_t                    chan_num,
        radio_scan_type_t           scan_type,
        target_stats_survey_cb_t   *survey_cb,
        ds_dlist_t                 *survey_list,
        void                       *survey_ctx)
{
    bool rc;

    rc =
        wl80211_survey_results_get(
                radio_cfg,
                chan_list,
                chan_num,
                scan_type,
                survey_list);
    if (rc != true) {
        (*survey_cb)(survey_list, survey_ctx, false);
        return false;
    }

    (*survey_cb)(survey_list, survey_ctx, true);

    return true;
}

bool target_stats_survey_convert(
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type,
        target_survey_record_t     *data_new,
        target_survey_record_t     *data_old,
        dpp_survey_record_t        *survey_record)
{
    bool rc;

    rc =
        wl80211_survey_results_convert(
                radio_cfg,
                scan_type,
                data_new,
                data_old,
                survey_record);
    if (rc != true) {
        return false;
    }

    return true;
}

/******************************************************************************
 *  NEIGHBORS definitions
 *****************************************************************************/
bool target_stats_scan_start(
        radio_entry_t              *radio_cfg,
        uint32_t                   *chan_list,
        uint32_t                    chan_num,
        radio_scan_type_t           scan_type,
        int32_t                     dwell_time,
        target_scan_cb_t           *scan_cb,
        void                       *scan_ctx)
{
    bool rc;

    rc =
        wl80211_scan_channel(
            radio_cfg,
            chan_list,
            chan_num,
            scan_type,
            dwell_time,
            scan_cb,
            scan_ctx);
    if (rc != true) {
        return false;
    }

    return true;
}

bool target_stats_scan_stop(
        radio_entry_t              *radio_cfg,
        radio_scan_type_t           scan_type)
{
    /*
     * The current survey support is normalized over time and
     * we need to resync it every before we start to collect the
     * samples. Until the BCM adds micro seconds we shall set
     * the normalization to 10s (sampling interval with 10 is
     * started just after ...)
     */
    bool rc;

    if (scan_type == RADIO_SCAN_TYPE_ONCHAN)
    {
        rc =
            wl80211_survey_set_interval(
                    radio_cfg,
                    10);
        if (rc != true) {
            return false;
        }
    }

    return true;
}

bool target_stats_scan_get(
        radio_entry_t              *radio_cfg,
        uint32_t                   *chan_list,
        uint32_t                    chan_num,
        radio_scan_type_t           scan_type,
        dpp_neighbor_report_data_t *scan_results)
{
    bool rc;

    rc =
        wl80211_scan_results_get(
            radio_cfg,
            chan_list,
            chan_num,
            scan_type,
            scan_results);
    if (rc != true) {
        return false;
    }

    return true;
}

/******************************************************************************
 *  DEVICE definitions
 *****************************************************************************/

/******************************************************************************
 *  CAPACITY definitions
 *****************************************************************************/
bool target_stats_capacity_enable(
        radio_entry_t              *radio_cfg,
        bool                        enabled)
{
    return true;
}

bool target_stats_capacity_get(
        radio_entry_t              *radio_cfg,
        target_capacity_data_t     *capacity_new)
{
    return true;
}

bool target_stats_capacity_convert(
        target_capacity_data_t     *capacity_new,
        target_capacity_data_t     *capacity_old,
        dpp_capacity_record_t      *capacity_entry)
{
    return true;
}
