#!/bin/sh
# {# jinja-parse #}
INSTALL_PREFIX={{INSTALL_PREFIX}}

gre_filter()
{
    awk '
    /[0-9]+: ([^:])+:/ {
        IF=substr($2, 1, index($2, "@") - 1)
    }

    / +gretap remote/ {
        print IF
    }'
}

gre_purge()
{
    ip -d link show | gre_filter | while read IF
    do
        echo "Removing GRE tunnel: $IF"
        ip link del "$IF"
    done
}

# It is useful to know which interfaces were present before starting
# the teardown in case something stalls later on
ls /sys/class/net

# save NOP list
${INSTALL_PREFIX}/bin/nol.sh save

# NM/WM/SM can interact with wifi driver therefore
# nas and epad must be killed afterwards to
# avoid races and unexpected driver sequences.
echo "killing managers"
killall -s SIGKILL dm cm nm wm lm sm bm um om qm fsm fcm

# From this point on CM is dead and no one is kicking
# watchdog.  There's less than 60s to complete everything
# down below before device throws a panic and reboots.

killall -s SIGKILL nas eapd
rm -f /tmp/.nas_ping_supported
rm -f /tmp/.eapd_ping_supported

# Stop miniupnd
/etc/init.d/miniupnpd stop
echo "miniupnpd stop"

# Purge all GRE tunnels
gre_purge

# Stop cloud connection
echo "Removing manager"
ovs-vsctl del-manager

# Stop openvswitch
/etc/init.d/openvswitch stop
echo "openvswitch stop"

# We don't want to leave stale wifi interfaces running
# around. WM2 won't touch anything that isn't in Config
# table so remove all extra vifs and put the primary
# channels down.
for i in $(ls /sys/class/net/)
do
    echo "$i" | grep "^wl" && {
        wl -i $i bss down
        wl -i $i down
    }
done

# nvram storage is used to tell nas/eapd where to listen for
# EAPOL. WM2/target uses lan ifnames dynamically and
# allocates them on-demand. Since we're tearing everything
# down and we're the only ones touching these entries just
# wipe them out so WM2/target gets a fresh start as if it was
# a clean reboot.
for i in $(nvram getall | grep lan.*_ifname)
do
    nvram unset $(echo "$i" | cut -d= -f1)
done

# This takes a few seconds to execute. Would be probably better
# to have a tool linked to libnvram or something.
if test -e /data/.kernel_nvram.setting
then
    nvram getall | cut -d= -f1 | sed 's/^/unset /' | xargs -n 1024 nvram
    nvram loadfile /data/.kernel_nvram.setting
fi

# Stop DNS service
for PID in $(pgrep dnsmasq); do echo "kill dns: $(cat /proc/$PID/cmdline)"; kill $PID; done

# Stop all DHCP clients
killall udhcpc

# Remove existing DHCP leases
rm /tmp/dhcp.leases

# Reset DNS files
rm -rf /tmp/dns

sleep 2
