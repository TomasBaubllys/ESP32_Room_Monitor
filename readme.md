# Smart room monitor using ESP32

**Author:** Tomas Baublys  
**Course:** Introduction to robotics  

---

## Project Overview
The assigment was to use all of the compoments in the arduino set that we have not used yet. So I've decided to try to make something with an ESP32, this project offers:

- *Real time clock* - a 4-Digit 7-Segment display to show the current time (it is synced once during setup from pool.ntp.org and is currently set to Vilnius local  time) 
- *Humidity and temperature monitoring* - once the ESP32 boots up you can go to its local ip address to monitor all of the sensors

---

## Hardware Setup

- You can view or download the component list here: [components.csv](components.csv)

## Circuit Diagram
![Circuit photo](real_photo.jpg)

## Wirring Diagram
![Wirring diagram](wirring_hw3.pdf)

---

## Software

- Written in **Arduino C++**.

## Usage
You will have to create the provided wiring diagram.

1. Connect the hardware according to the pinout table.
   For more details, refer to the [Wiring Diagram](wirring_hw3.jpeg)  
2. Upload the code to your ESP32.
3. Have fun!.

--- 

# Notes
- This project was mostly to understand how different components can work together, so it kinda feels useless in some places, atleast the clock looks nice... If however you wish to implement it yourself feel free to adjust the relay on threshold value. Also don't forget to set your wifi password and ssid.


