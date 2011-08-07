#
# Regular cron jobs for the luckygps package
#
0 4	* * *	root	[ -x /usr/bin/luckygps_maintenance ] && /usr/bin/luckygps_maintenance
