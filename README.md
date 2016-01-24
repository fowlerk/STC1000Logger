<style type="text/css">@import url('https://themes.googleusercontent.com/fonts/css?kit=wAPX1HepqA24RkYW1AuHYA');ol{margin:0;padding:0}.c0{orphans:2;widows:2;direction:ltr;height:11pt}.c1{orphans:2;widows:2;text-align:justify;direction:ltr}.c3{page-break-after:avoid;orphans:2;widows:2;direction:ltr}.c6{orphans:2;widows:2;direction:ltr}.c2{background-color:#ffffff;max-width:468pt;padding:72pt 72pt 72pt 72pt}.c10{color:#1155cc;text-decoration:underline}.c5{color:inherit;text-decoration:inherit}.c8{font-size:10pt;font-family:"Calibri"}.c9{font-style:italic}.c7{text-align:justify}.c4{color:#333333}.title{padding-top:0pt;color:#000000;font-size:26pt;padding-bottom:3pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}.subtitle{padding-top:0pt;color:#666666;font-size:15pt;padding-bottom:16pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}li{color:#000000;font-size:11pt;font-family:"Arial"}p{margin:0;color:#000000;font-size:11pt;font-family:"Arial"}h1{padding-top:20pt;color:#000000;font-size:20pt;padding-bottom:6pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}h2{padding-top:18pt;color:#000000;font-size:16pt;padding-bottom:6pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}h3{padding-top:16pt;color:#434343;font-size:14pt;padding-bottom:4pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}h4{padding-top:14pt;color:#666666;font-size:12pt;padding-bottom:4pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}h5{padding-top:12pt;color:#666666;font-size:11pt;padding-bottom:4pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;orphans:2;widows:2;text-align:left}h6{padding-top:12pt;color:#666666;font-size:11pt;padding-bottom:4pt;font-family:"Arial";line-height:1.15;page-break-after:avoid;font-style:italic;orphans:2;widows:2;text-align:left}</style>

<a name="h.1i2zk5u7ai7j" id="h.1i2zk5u7ai7j"></a><span>

STC-1000+ Wireless Data Logger
------------------------------
</span
<span>This project creates a wireless data logger to interface with the STC-1000+ dual-stage temperature controller, running the (great!) communications firmware designed and written by Mats Staffansson. &nbsp;Let me take the opportunity to thank Mats for all of his great contributions to the homebrewing community through his tireless efforts in creating the various versions of the STC firmware. &nbsp;Without him, none of this would have been possible.</span>

<span></span>

<span>This project retrieves key data from the STC-1000 on a periodic basis as defined in the code. &nbsp;The parameters retrieved and logged are: &nbsp;setpoint, current probe temperature, status of the cooling and heating relays, run-mode (profile or thermostat), step (for profile), and duration hours.</span>

<span></span>

<span>This version of the project was written specifically for the Adafruit Huzzah ESP-8266 package. &nbsp;Through the various iterations of this effort, I’ve used other versions of the ESP (notably the Cactus Micro version initially), but settled on this one because of its stability, breakout of most of the GPIO pins, and huge (4M) flash size. &nbsp;This has allowed be to include support for much more extensive error-checking and correction in the code, along with support for an external display (an SSD1306 based OLED) to monitor what’s going on in real time.</span>

<span>Note that the STC should be flashed with the communications firmware provided by Mats (picprog_com.ino).</span>

## <a name="h.4m64hkezxzs4" id="h.4m64hkezxzs4"></a><span>Background</span>

<span>Being a homebrewer for several years now, I initially became interested in temperature control for my fermentations when I wanted to tackle my first lager. &nbsp;I stumbled on Mats thread on the STC, and was impressed with what he’d done. &nbsp;After purchasing and using a controller with his firmware a couple of times, I wanted to be able to log my fermentation activity in order to help improve my future attempts. &nbsp;Mats inspired me in some of his discussions on using the ESP in order to do this, though I eventually decided to take a somewhat different route than the base ESP-01 using Sming he’d mentioned. &nbsp;I ran across the Cactus Micro package from April Brother and decided to use it for my first attempt. &nbsp;It combines the Atmel ATmega32U4 MCU and the ESP-8266 WiFi SOC. &nbsp;Though I managed to put together a version that ran (fairly) well on this package, I found it somewhat unreliable in that it would hang every few days to hours. &nbsp;A simple restart would get it running again, but I was frustrated by not knowing what was going on in real time, and more so, by not having enough flash to be able to add what I considered to be adequate error checking and correction to the code. &nbsp;(This may have been as much a reflection of my barely-adequate coding or electronics abilities, but I tire quickly of trying to optimize code to fit in smaller flash.)</span>

<span></span>

<span>So, I began looking around for better hardware solutions that were still relatively inexpensive. &nbsp;I decided to try the Adafruit Huzzah ESP-8266 as an alternative. &nbsp;It didn’t take long to fall in love with this package. &nbsp;It seems to be much more stable, has tons of flash (4M), breaks out all of the GPIO’s for use, and supports nice additions like h/w SPI (used for the OLED display).</span>

<span></span>

## <a name="h.xv2j79aatsdk" id="h.xv2j79aatsdk"></a><span>Getting it Running</span>

### <a name="h.bcedjkomiwtp" id="h.bcedjkomiwtp"></a><span>The Data Stream</span>

<span>Though there are a number of free data-streaming services available on-line, I decided to first try one provided by SparkFun at data.sparkfun.com. &nbsp;It is a public implementation of a Phant server, though the code is freely available to download and run your own instance if you desire. &nbsp;For my initial attempt, I didn’t mind putting this up so that it was available to the public, but was very interested in being able to run my own instance in the future. &nbsp;In order to get started, you must first go to data.sparkfun.com and set up a data stream for logging your data. &nbsp;It is quite simple and fast to do so, just be sure to create fieldnames that match exactly the names used in the code. &nbsp;Also be sure to note the public and private keys, as these must be entered in the code to connect to the stream once created.</span>

### <a name="h.38tna2nuir11" id="h.38tna2nuir11"></a><span>The OLED Library</span>

<span>One of the downsides for working with the ESP-8266 is that some of the libraries still have not been ported to this platform for the Arduino IDE (which is what I used for my efforts). &nbsp;When I decided to use the SSD1306 controller-based OLED, I found this to be a minor stumbling block initially, but eventually discovered someone had been able to make minor edits to the Adafruit library to support this. &nbsp;It essentially amounts to a couple of trivial edits (additions) to one of the library files (Adafruit_GFX.cpp), and is described here: &nbsp;</span><span class="c10">[https://github.com/somhi/ESP_SSD1306](https://www.google.com/url?q=https://github.com/somhi/ESP_SSD1306&sa=D&ust=1453667017938000&usg=AFQjCNEX0X2irPnhU-f3TZ4cv6QDnpFcrQ)</span>


<span>I saved the edits to a copy of the original library called ESP_SSD1306 so that I could use either the original or the modified one going forward. &nbsp;(Thanks to SOMHI for his efforts on this, and Adafruit / LadyAda for their work in creating the original library!)</span>

<span></span>

<span>Note that I used a 4-wire SPI version of the OLED, though I’ve read others have gotten the I2C version working as well. &nbsp;I’ve used both 1.3 inch and 0.96 inch versions of these successfully.</span>

<span></span>

<span>Final note on the OLED...I added this strictly to have a local display of what was happening throughout the logging process as a realtime monitor. &nbsp;It is strictly optional and the logger will work fine without this connected (though you’ll be a little blind about what’s going on unless you have a serial monitor connected).</span>

### <a name="h.spws1gqy5c4z" id="h.spws1gqy5c4z"></a><span>NTP Server</span>

<span>One of the later enhancements I made to the original code was to connect to an NTP server to be able display the time on the OLED periodically. &nbsp;This is displayed on line 1 in a non-scrolling region of the display so that it is always present and updated (well, assuming connection to the server is successful anyway!) &nbsp;The current version of the code is hard-coded for Eastern Daylight Time (offset of -5 hours from GMT). &nbsp;This must currently be edited manually, as I’ve not added code to automatically configure this or calculate daylight savings time (yet).</span>

### <a name="h.u7gqb9lxkoel" id="h.u7gqb9lxkoel"></a><span>Wiring Diagram</span>

<span>I’ve included a Fritzing diagram of the wiring to the Huzzah to help in connecting the various components. &nbsp;Keep in mind that this is my first attempt at using Fritzing, and combined with my barely-adequate electronics knowledge, well, you get what you pay for here.</span>

<span></span>

<span>Note that the project is powered externally (not from the STC power supply), using a USB 5V supply. &nbsp;I’ve included a Shottky (bat85) &nbsp;diode to keep the STC from attempting to power the Huzzah. &nbsp;I’ve also added a couple of resistors (7.5K and 3.9K ohm) in a voltage-divider to drop the 5V signal from the STC to 3.3V range for the Huzzah. &nbsp;Finally, I’ve added a pull-down 10K resistor for the </span><span class="c4 c9">ICSPCLK </span><span class="c4">line as Mats recommended in his documentation.</span>

<span class="c4"></span>

### <a name="h.rvssv856ckfm" id="h.rvssv856ckfm"></a><span>Futures</span>

<span>One of my satisfactions in building something like this, especially when writing code I’ve found, is that I’m never quite fully done -- I’m always finding something else I’d like to add to make it better. &nbsp;That said, I’ve been looking at building in a web-configurator in order to configure the various settings (network settings, NTP server, timezone, Phant settings, etc.) &nbsp;I’d also like it to be able to check for DST automatically. &nbsp;Stay tuned...</span>

<span></span>