/*
  File: v4lctl_node.cpp
  Date: 10.04.2016
  Author: Patrick Feuser
  Copyrights: 2016 UAV-Concept http://uav-concept.com
  License: GNU GPLv3

  This ROS node is a wrapper for the v4lctl command line tool.
  It provides two services to set or get particular bttv parameter.
  If a yaml file will be provided a save and restore capability will be enabled.
  Parameter are then loaded and set automatically in the capture card when starting the node.
  Changed parameter are stored to a yaml file when the program ends.
  Additionally a dynamic reconfigure rqt gui is available to manipulate bttv parameter.

  The below v4lctl list output is based on an Osprey 440 capture card.
  This node is optimized for this card.
  Your card may output other options then change this file and v4lctlNodeDyn.cfg to your needs.

  $ v4lctl list
  attribute  | type   | current | default | comment
  -----------+--------+---------+---------+-------------------------------------
  norm       | choice | PAL-I   | NTSC    | NTSC NTSC-M NTSC-M-JP NTSC-M-KR PAL PAL-BG PAL-H PAL-I PAL-DK PAL-M PAL-N PAL-Nc PAL-60 SECAM SECAM-B SECAM-G SECAM-H SECAM-DK SECAM-L SECAM-Lc
  input      | choice | Composi | Composi | Composite0 Composite1 Composite2 Composite3
  bright     | int    |   32768 |   32768 | range is 0 => 65280
  contrast   | int    |   32768 |   27648 | range is 0 => 65408
  color      | int    |   32768 |   32768 | range is 0 => 65408
  hue        | int    |   32768 |   32768 | range is 0 => 65280
  mute       | bool   | on      | off     |
  Chroma AGC | bool   | off     | off     |
  Color Kill | bool   | off     | off     |
  Comb Filte | bool   | off     | off     |
  Auto Mute  | bool   | on      | on      |
  Luma Decim | bool   | off     | off     |
  AGC Crush  | bool   | on      | on      |
  VCR Hack   | bool   | off     | off     |
  Whitecrush | int    |     127 |     127 | range is 0 => 255
  Whitecrush | int    |     207 |     207 | range is 0 => 255
  UV Ratio   | int    |      51 |      50 | range is 0 => 100
  Full Luma  | bool   | off     | off     |
  Coring     | int    |       0 |       0 | range is 0 => 3

  See also http://linux.die.net/man/1/v4lctl

  setnorm <norm>
    Set the TV norm (NTSC/PAL/SECAM).
  setinput [ <input> | next ]
    Set the video input (Television/Composite1/...)
  capture [ on | off | overlay | grabdisplay ]
    Set capture mode.
  volume mute on | off
    mute / unmute audio.
  volume <arg>
  color <arg>
  hue <arg>
  bright <arg>
  contrast <arg>
    Set the parameter to the specified value. <arg> can be one of the following: A percent value ("70%" for example). Some absolute value ("32768"), the valid range is hardware specific. Relative values can be specified too by prefixing with "+=" or "-=" ("+=10%" or "-=2000"). The keywords "inc" and "dec" are accepted to and will increase and decrease the given value in small steps.
  setattr <name> <value>
    Set set the value of some attribute (color, contrast, ... can be set this way too).
  show [ <name> ]
    Show the value current of some attribute.

  Samples:
  $ v4lctl -c /dev/video0 bright 70%
  $ v4lctl -c /dev/video0 contrast "60%"
  $ v4lctl -c /dev/video0 color 27648
  $ v4lctl -c /dev/video0 setattr "Auto Mute" off
*/
#include <ros/ros.h>
#include <dynamic_reconfigure/server.h>
#include <uavc_v4lctl/v4lctlNodeDynConfig.h>
#include <uavc_v4lctl/v4lctlSet.h>
#include <uavc_v4lctl/v4lctlGet.h>
#include <opencv2/opencv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>
#include <sstream>
#include <map>

#define YAML_OPT_TO_NAME(s) boost::replace_all_copy(boost::replace_all_copy(s,"\"","-")," ","_")
#define YAML_NAME_TO_OPT(s) boost::replace_all_copy(boost::replace_all_copy(s,"-","\""),"_"," ")

namespace uavc_v4lctl {

class v4lctlNode
{
  ros::NodeHandle nh_;

  ros::ServiceServer srv_v4lctl_get_;
  ros::ServiceServer srv_v4lctl_set_;

  v4lctlNodeDynConfig cfg_;
  dynamic_reconfigure::Server<v4lctlNodeDynConfig> cfg_server_;

  std::map<std::string, std::string> fs_map_;

  std::string device_;
  std::string yaml_;

public:

  v4lctlNode()
  {
    ros::NodeHandle pnh("~");

    pnh.param<std::string>("device", device_, "/dev/video0");
    pnh.param<std::string>("yaml", yaml_, "");

    if (!yaml_.empty())
      restoreFromYaml();

    cfg_server_.setCallback(boost::bind(&v4lctlNode::configCb, this, _1, _2));

    srv_v4lctl_set_ = nh_.advertiseService("v4lctlSet", &v4lctlNode::v4lctlSetCb, this);
    srv_v4lctl_get_ = nh_.advertiseService("v4lctlGet", &v4lctlNode::v4lctlGetCb, this);
  }

  ~v4lctlNode()
  {
    cv::FileStorage fs;

    if (!yaml_.empty() && fs.open(yaml_, cv::FileStorage::WRITE))
    {
      std::map<std::string, std::string>::iterator it = fs_map_.begin();

      while (it != fs_map_.end())
      {
        std::string key = (*it).first;

        fs << key << fs_map_[key];
        it++;
      }
    }
  }

  std::string v4lctlGet(std::string name)
  {
    FILE *pipe;
    char stdout[128];
    std::string result;
    std::stringstream ss;

    ss << "v4lctl -c " << device_ << " show \"" << name << "\"";

    if ((pipe=popen(ss.str().c_str(), "r")))
    {
      if (fgets(stdout, sizeof(stdout), pipe))
        result = boost::regex_replace(std::string(stdout), boost::regex("^" + name + ": ([a-zA-Z0-9\\-]+).*"), "$1");

      ROS_INFO_STREAM("Call '" << ss.str() << "' Result: " << result);

      pclose(pipe);
    }
    else
      ROS_ERROR_STREAM("Call failed '" << ss.str() << "'");

    return result;
  }

  bool v4lctlSet(std::string name, std::string value)
  {
    std::stringstream ss;
    bool result = false;

    ss << "v4lctl -c " << device_ << " " << name << " " << value;

    ROS_INFO_STREAM("Call '" << ss.str() << "'");

    if ((result=std::system(ss.str().c_str()) > -1))
      fs_map_[YAML_OPT_TO_NAME(name)] = value;

    return result;
  }

  void restoreFromYaml()
  {
    cv::FileStorage fs;

    if (fs.open(yaml_, cv::FileStorage::READ))
    {
      cv::FileNode fn = fs.root();

      for (cv::FileNodeIterator fit = fn.begin(); fit != fn.end(); fit++)
      {
        cv::FileNode item = *fit;

        v4lctlSet(YAML_NAME_TO_OPT(item.name()), (std::string)item);
      }
    }
  }

  void initConfig(v4lctlNodeDynConfig &config, bool defaults = false)
  {
    std::string s;

#define V4LCTL_GET_PERCENT(name,default,max) (defaults || (s=v4lctlGet(name)).empty() ? default : (atoi(s.c_str()) * 100 / max))
#define V4LCTL_GET_BOOL(name,default)        (defaults || (s=v4lctlGet(name)).empty() ? default : boost::regex_match(s, boost::regex("^on\$")))
#define V4LCTL_GET_INT(name,default)         (defaults || (s=v4lctlGet(name)).empty() ? default : atoi(s.c_str()))
#define V4LCTL_GET_STRING(name,default)      (defaults || (s=v4lctlGet(name)).empty() ? default : s)

    config.input            = V4LCTL_GET_STRING ("input",           "Composite0");
    config.norm             = V4LCTL_GET_STRING ("norm",                 "PAL-I");
    config.bright           = V4LCTL_GET_PERCENT("bright",             50, 65280);
    config.contrast         = V4LCTL_GET_PERCENT("contrast",           40, 65280);
    config.color            = V4LCTL_GET_PERCENT("color",              50, 65280);
    config.hue              = V4LCTL_GET_PERCENT("hue",                50, 65280);
    config.UV_Ratio         = V4LCTL_GET_INT    ("UV Ratio",                  50);
    config.Coring           = V4LCTL_GET_INT    ("Coring",                     0);
    config.Whitecrush_Lower = V4LCTL_GET_PERCENT("Whitecrush Lower",     50, 255);
    config.Whitecrush_Upper = V4LCTL_GET_PERCENT("Whitecrush Upper",     80, 255);
    config.mute             = V4LCTL_GET_BOOL   ("mute",                   false);
    config.Chroma_AGC       = V4LCTL_GET_BOOL   ("Chroma AGC",             false);
    config.Color_Killer     = V4LCTL_GET_BOOL   ("Color Killer",           false);
    config.Comb_Filter      = V4LCTL_GET_BOOL   ("Comb Filter",            false);
    config.Auto_Mute        = V4LCTL_GET_BOOL   ("Auto Mute",               true);
    config.Luma_Decim       = V4LCTL_GET_BOOL   ("Luma Decimation Filter", false);
    config.AGC_Crush        = V4LCTL_GET_BOOL   ("AGC Crush",               true);
    config.VCR_Hack         = V4LCTL_GET_BOOL   ("VCR Hack",               false);
    config.Full_Luma        = V4LCTL_GET_BOOL   ("Full Luma Range",        false);
  }

  void configCb(v4lctlNodeDynConfig &config, int level)
  {
    if (level < 0)
    {
      initConfig(config);
      cfg_ = config;

      ROS_INFO("Reconfigure server initialized");

      return;
    }

    if (config.Set_Defaults)
    {
      initConfig(config, true);
      config.Set_Defaults = false;
    }

    if (config.input != cfg_.input)
      v4lctlSet("setattr input", config.input);

    if (config.norm != cfg_.norm)
      v4lctlSet("setnorm", config.norm);

    if (config.bright != cfg_.bright)
      v4lctlSet("bright", std::to_string(config.bright)+'%');

    if (config.contrast != cfg_.contrast)
      v4lctlSet("contrast", std::to_string(config.contrast)+'%');

    if (config.color != cfg_.color)
      v4lctlSet("color", std::to_string(config.color)+'%');

    if (config.hue != cfg_.hue)
      v4lctlSet("hue", std::to_string(config.hue)+'%');

    if (config.UV_Ratio != cfg_.UV_Ratio)
      v4lctlSet("setattr \"UV Ratio\"", std::to_string(config.UV_Ratio)+'%');

    if (config.Coring != cfg_.Coring)
      v4lctlSet("setattr Coring", std::to_string(config.Coring));

    if (config.Whitecrush_Lower != cfg_.Whitecrush_Lower)
      v4lctlSet("setattr \"Whitecrush Lower\"", std::to_string(config.Whitecrush_Lower)+'%');

    if (config.Whitecrush_Upper != cfg_.Whitecrush_Upper)
      v4lctlSet("setattr \"Whitecrush Upper\"", std::to_string(config.Whitecrush_Upper)+'%');

    if (config.mute != cfg_.mute)
      v4lctlSet("setattr mute", config.mute ? "on" : "off");

    if (config.Chroma_AGC != cfg_.Chroma_AGC)
      v4lctlSet("setattr \"Chroma AGC\"", config.Chroma_AGC ? "on" : "off");

    if (config.Color_Killer != cfg_.Color_Killer)
      v4lctlSet("setattr \"Color Killer\"", config.Color_Killer ? "on" : "off");

    if (config.Comb_Filter != cfg_.Comb_Filter)
      v4lctlSet("setattr \"Comb Filter\"", config.Comb_Filter ? "on" : "off");

    if (config.Auto_Mute != cfg_.Auto_Mute)
      v4lctlSet("setattr \"Auto Mute\"", config.Auto_Mute ? "on" : "off");

    if (config.Luma_Decim != cfg_.Luma_Decim)
      v4lctlSet("setattr \"Luma Decimation Filter\"", config.Luma_Decim ? "on" : "off");

    if (config.AGC_Crush != cfg_.AGC_Crush)
      v4lctlSet("setattr \"AGC Crush\"", config.AGC_Crush ? "on" : "off");

    if (config.VCR_Hack != cfg_.VCR_Hack)
      v4lctlSet("setattr \"VCR Hack\"", config.VCR_Hack ? "on" : "off");

    if (config.Full_Luma != cfg_.Full_Luma)
      v4lctlSet("setattr \"Full Luma Range\"", config.Full_Luma ? "on" : "off");

    cfg_ = config;
  }

  bool v4lctlGetCb(v4lctlGet::Request  &req, v4lctlGet::Response &res)
  {
    res.result = v4lctlGet(req.name);

    return true;
  }

  bool v4lctlSetCb(v4lctlSet::Request  &req, v4lctlSet::Response &res)
  {
    return v4lctlSet(req.name, req.value);
  }
};

}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "v4lctl");

  uavc_v4lctl::v4lctlNode n;

  ros::spin();

  return 0;
}
