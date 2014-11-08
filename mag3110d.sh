#! /bin/sh
# /etc/init.d/mag3110d
#
### BEGIN INIT INFO
# Provides:          mag3110d
# Required-Start:    $syslog $time $remote_fs
# Required-Stop:     $syslog $time $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Read magnetometer chip MAG3110
### END INIT INFO
#
# Author:      Jaakko Koivuniemi <oh7bf@sral.fi>	
#

NAME=mag3110d
PATH=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/usr/sbin/mag3110d
PIDFILE=/var/run/mag3110d.pid

test -x $DAEMON || exit 0

. /lib/lsb/init-functions

case "$1" in
  start)
	log_daemon_msg "Starting daemon" $NAME
        start_daemon -p $PIDFILE $DAEMON
        log_end_msg $?     
    ;;
  stop)
	log_daemon_msg "Stopping daemon" $NAME
        killproc -p $PIDFILE $DAEMON
        log_end_msg $?	
    ;;
  force-reload|restart)
    $0 stop
    $0 start
    ;;
  status)
    status_of_proc -p $PIDFILE $DAEMON $NAME && exit 0 || exit $? 
    ;;
  *)
    echo "Usage: /etc/init.d/mag3110d {start|stop|restart|force-reload|status}"
    exit 1
    ;;
esac

exit 0
