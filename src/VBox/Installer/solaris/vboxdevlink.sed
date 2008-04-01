# sed class script to modify /etc/devlink.tab
!install
/name=vboxdrv/d
$i\
type=ddi_pseudo;name=vboxdrv	\\D

!remove
/name=vboxdrv/d

