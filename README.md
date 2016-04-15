# uavc_v4lctl
This ROS node is a wrapper for the v4lctl tool.
It provides two services to set or get particular bttv parameter.
If a yaml file will be provided a save and restore capability will be enabled.
Parameter are then loaded and set automatically in the capture card when starting the node.
Changed parameter are stored to a yaml file when the program ends.
Additionally a dynamic reconfigure rqt gui is available to manipulate bttv parameter.

For additional documentation see http://wiki.ros.org/uavc_v4lctl
