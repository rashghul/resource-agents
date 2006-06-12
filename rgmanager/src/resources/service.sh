#!/bin/bash

#
# Dummy OCF script for resource group; the OCF spec doesn't support abstract
# resources. ;(
#

# Grab nfs lock tricks if available
export NFS_TRICKS=1
if [ -f "$(dirname $0)/svclib_nfslock" ]; then
	. $(dirname $0)/svclib_nfslock
	NFS_TRICKS=0
fi

meta_data()
{
    cat <<EOT
<?xml version="1.0"?>
<resource-agent version="rgmanager 2.0" name="service">
    <version>1.0</version>

    <longdesc lang="en">
        This defines a collection of resources, known as a resource
        group or cluster service.
    </longdesc>
    <shortdesc lang="en">
        Defines a services.
    </shortdesc>

    <parameters>
        <parameter name="name" unique="1" required="1" primary="1">
            <longdesc lang="en">
                This is the name of the resource group.
            </longdesc>
            <shortdesc lang="en">
                Name
            </shortdesc>
            <content type="string"/>
        </parameter>
    
        <parameter name="domain">
            <longdesc lang="en">
                Fail over domains define lists of cluster members
                to try in the event that a resource group fails.
            </longdesc>
            <shortdesc lang="en">
                Fail over Domain
            </shortdesc>
            <content type="string"/>
        </parameter>

        <parameter name="autostart">
            <longdesc lang="en">
	    	If set to yes, this resource group will automatically be started
		after the cluster forms a quorum.  If set to no, this resource
		group will start in the 'disabled' state after the cluster forms
		a quorum.
            </longdesc>
            <shortdesc lang="en">
	    	Automatic start after quorum formation
            </shortdesc>
            <content type="boolean"/>
        </parameter>

        <parameter name="hardrecovery">
            <longdesc lang="en">
	    	If set to yes, the last owner will reboot if this resource
		group fails to stop cleanly, thus allowing the resource
		group to fail over to another node.  Use with caution; a
		badly-behaved resource could cause the entire cluster to
		reboot.  This should never be enabled if the automatic
		start feature is used.
            </longdesc>
            <shortdesc lang="en">
	    	Reboot if stop phase fails
            </shortdesc>
            <content type="boolean"/>
        </parameter>

        <parameter name="exclusive">
            <longdesc lang="en">
	    	If set, this resource group will only relocate to
		nodes which have no other resource groups running in the
		event of a failure.  If no empty nodes are available,
		this resource group will not be restarted after a failure.
		Additionally, resource groups will not automatically
		relocate to the node running this resource group.  This
		option can be overridden by manual start and/or relocate
		operations.
            </longdesc>
            <shortdesc lang="en">
	        Exclusive resource group
            </shortdesc>
            <content type="boolean"/>
        </parameter>

	<parameter name="nfslock">
	    <longdesc lang="en">
	    	Enable NFS lock workarounds.  When used with a compatible
		HA-callout program like clunfslock, this could be used
		to provide NFS lock failover, but at significant cost to
		other services on the machine.  This requires a compatible
		version of nfs-utils and manual configuration of rpc.statd;
		see 'man rpc.statd' to see if your version supports
		the -H parameter.
	    </longdesc>
	    <shortdesc lang="en">
	        Enable NFS lock workarounds
	    </shortdesc>
	    <content type="boolean"/>
	</parameter>
                
        <parameter name="recovery">
            <longdesc lang="en">
	        This currently has three possible options: "restart" tries
		to restart failed parts of this resource group locally before
		attempting to relocate (default); "relocate" does not bother
		trying to restart the service locally; "disable" disables
		the resource group if any component fails.  Note that
		any resource with a valid "recover" operation which can be
		recovered without a restart will be.
            </longdesc>
            <shortdesc lang="en">
	    	Failure recovery policy
            </shortdesc>
            <content type="string"/>
        </parameter>
    </parameters>

    <actions>
        <action name="start" timeout="5"/>
        <action name="stop" timeout="5"/>
	
	<!-- No-ops.  Groups are abstract resource types.  -->
        <action name="status" timeout="5" interval="1h"/>
        <action name="monitor" timeout="5" interval="1h"/>

        <action name="recover" timeout="5"/>
        <action name="reload" timeout="5"/>
        <action name="meta-data" timeout="5"/>
        <action name="verify-all" timeout="5"/>
    </actions>
    
    <special tag="rgmanager">
        <attributes root="1" maxinstances="1"/>
        <child type="fs" start="1" stop="8"/>
        <child type="clusterfs" start="2" stop="7"/>
        <child type="netfs" start="3" stop="6"/>
	<child type="nfsexport" start="4" stop="5"/>

	<child type="nfsclient" start="5" stop=""/>

        <child type="ip" start="6" stop="2"/>
        <child type="smb" start="7" stop="3"/>
        <child type="script" start="7" stop="1"/>
    </special>
</resource-agent>
EOT
}


#
# A Resource group is abstract, but the OCF RA API doesn't allow for abstract
# resources, so here it is.
#
case $1 in
	start)
		#
		# XXX If this is set, we kill lockd.  If there is no
		# child IP address, then clients will NOT get the reclaim
		# notification.
		#
		if [ $NFS_TRICKS -eq 0 ]; then
			if [ "$OCF_RESKEY_nfslock" = "yes" ] || \
	   		   [ "$OCF_RESKEY_nfslock" = "1" ]; then
				pkill -KILL -x lockd
			fi
		fi
		exit 0
		;;
	stop)
		exit 0
		;;
	recover|restart)
		exit 0
		;;
	status|monitor)
		exit 0
		;;
	reload)
		exit 0
		;;
	meta-data)
		meta_data
		exit 0
		;;
	verify-all)
		exit 0
		;;
	*)
		exit 0
		;;
esac