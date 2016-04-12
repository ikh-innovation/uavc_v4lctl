# uavc_v4lctl
This ROS node is a wrapper for the v4lctl tool.
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
